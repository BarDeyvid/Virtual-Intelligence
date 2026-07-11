/**
 * @file PresenceDetector.hpp
 * @brief Webcam-based user presence detection (night-shift feature).
 *
 * Always-on streaming by default: os CLIs ligam start_stream() no boot e a
 * webcam fica aberta pelo resto da sessão, só pausando quando o
 * VRAMResourceManager pede (LISTENING/SPEAKING) — o throttle religa sozinho
 * ao sair da pressão. check()/capture_to_file() continuam existindo para
 * consumo pontual (ex.: sem stream ativo, ou chamada direta da tool) e
 * abrem/fecham a webcam sozinhos nesse caso. Feeds two consumers:
 *  - the `webcam_check` tool (Alyssa "olha" pela webcam quando perguntada)
 *  - the ProactivityEngine presence triggers (welcome-back / don't talk to
 *    an empty chair)
 *
 * Graceful degradation everywhere: no webcam, no cascade file, capture
 * failure — all report "unavailable" instead of throwing.
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace alyssa_vision {

/**
 * @struct PresenceResult
 * @brief Outcome of one webcam presence check.
 */
struct PresenceResult {
    bool available = false;   ///< false = webcam/cascade unusable (don't trust the rest)
    bool present = false;     ///< true when at least one face was detected
    int face_count = 0;
    double brightness = 0.0;  ///< Mean frame luminance 0-255 (dark room hint)
    std::string error;        ///< Filled when available == false
    cv::Mat preview;          ///< Frame RGB reduzido com os rostos marcados (aba Vision da CLI)
};

/**
 * @class PresenceDetector
 * @brief Face-based presence detection through the default webcam.
 */
class PresenceDetector {
public:
    /**
     * @param camera_index cv::VideoCapture device index (default 0).
     * @param cascade_path Haar cascade path; default resolves to the copy
     *        shipped in config/ (auto-copied to the build dir).
     */
    explicit PresenceDetector(int camera_index = 0,
                              const std::string& cascade_path = "config/haarcascade_frontalface_default.xml");

    ~PresenceDetector(); ///< Encerra o streaming se ainda estiver ativo.

    /// true when the cascade loaded — webcam itself is only probed per check.
    bool is_ready() const { return cascade_loaded; }

    /**
     * @brief Open webcam, grab one frame, detect faces, release webcam.
     * @details Blocking (~1-2s on Windows MSMF). Call from a background thread.
     *          Com o streaming ativo, retorna o último frame do stream na hora
     *          (sem reabrir a webcam).
     */
    PresenceResult check();

    /**
     * @brief Mantém a webcam aberta com captura contínua (~30 FPS).
     * @param on_frame Chamado (na thread de streaming) após cada frame publicado.
     * @details Uso padrão hoje: always-on, ligado no boot do CLI e mantido
     *          pelo resto da sessão (a LED fica acesa continuamente, exceto
     *          nas pausas do throttle de VRAM). check() e capture_to_file()
     *          passam a usar o frame do stream. O preview atualiza a cada
     *          frame; a detecção Haar roda a cada 3 (os retângulos entre
     *          detecções reutilizam o último resultado).
     */
    void start_stream(std::function<void()> on_frame = nullptr);

    /// Para a captura contínua e libera a webcam (LED apaga). Bloqueia até a thread encerrar.
    void stop_stream();

    bool is_streaming() const { return streaming; }

    /// Último resultado publicado pelo stream (available=false se ainda sem frame).
    PresenceResult latest();

    /**
     * @brief Capture a frame and save it to disk (webcam variant of screenshot).
     * @param filename Output image path.
     * @return Human-readable result string (PT-BR) for the tool pipeline.
     */
    std::string capture_to_file(const std::string& filename);

private:
    int camera_index;
    cv::CascadeClassifier face_cascade;
    bool cascade_loaded = false;
    std::mutex webcam_mtx;    ///< Tool call e thread de proatividade disputam a webcam

    std::atomic<bool> streaming{false};
    std::thread stream_thread;
    std::mutex stream_lifecycle_mtx; ///< start/stop podem vir de threads diferentes
    std::mutex latest_mtx;           ///< Guarda latest_result e latest_frame
    PresenceResult latest_result;
    cv::Mat latest_frame;            ///< Frame cheio (BGR) p/ capture_to_file durante o stream
    std::function<void()> on_new_frame;

    /// Grab a single frame; empty Mat on failure.
    cv::Mat grab_frame(std::string& error_out);

    /// Detecção + brilho + preview a partir de um frame já capturado.
    PresenceResult analyze(cv::Mat& frame);

    /// Detecção Haar em escala reduzida (rápida mesmo em 720p); retângulos
    /// voltam em coordenadas do frame original. Também mede o brilho.
    std::vector<cv::Rect> detect_faces(const cv::Mat& frame, double& brightness_out);

    /// Monta o PresenceResult (preview colorido + retângulos) sem re-detectar.
    PresenceResult compose_result(const cv::Mat& frame,
                                  const std::vector<cv::Rect>& faces,
                                  double brightness);

    /// Loop da thread de streaming: captura, analisa e publica até streaming = false.
    void stream_loop();

    /// Atualiza latest_result/latest_frame e dispara o callback de UI.
    void publish(const PresenceResult& result, const cv::Mat& frame);
};

} // namespace alyssa_vision
