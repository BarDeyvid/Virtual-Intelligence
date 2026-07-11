// PresenceDetector.cpp
// Webcam presence detection via Haar face cascade (night-shift feature).

#include "PresenceDetector.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace alyssa_vision {

namespace {

constexpr int kPreviewWidth = 120;  ///< Colunas de '▀' na aba Vision da CLI
constexpr int kDetectWidth  = 400;  ///< Largura do frame usado na detecção Haar

/// Pede o melhor modo à webcam: MJPG (YUY2 cru não passa de ~10 FPS em 720p
/// na maioria das câmeras USB), 1280x720 e 30 FPS. Câmeras que não suportam
/// simplesmente ignoram — o OpenCV mantém o modo default.
void configure_capture(cv::VideoCapture& cap) {
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_FPS, 30);
}

#ifdef _WIN32
// MSMF é o backend default do cv::VideoCapture no Windows e é conhecido por
// travar por dezenas de segundos (às vezes minutos, dependendo do driver) na
// negociação inicial de exposição/resolução de webcams USB. DSHOW abre as
// mesmas câmeras em bem menos de 1s — é o motivo do "demorou 3 minutos".
constexpr int kPreferredBackend = cv::CAP_DSHOW;
#else
constexpr int kPreferredBackend = cv::CAP_ANY;
#endif

/// Abre a câmera pelo backend rápido; se ela não existir nesse backend
/// específico, cai pro default da plataforma antes de desistir.
bool open_camera(cv::VideoCapture& cap, int camera_index) {
    if (cap.open(camera_index, kPreferredBackend) && cap.isOpened()) return true;
    cap.release();
    return cap.open(camera_index);
}

} // namespace

PresenceDetector::PresenceDetector(int camera_index, const std::string& cascade_path)
    : camera_index(camera_index)
{
    // Tenta o caminho dado e um fallback relativo à raiz do repo (execução
    // fora do diretório de build).
    const std::string candidates[] = { cascade_path, "../" + cascade_path };
    for (const auto& path : candidates) {
        if (face_cascade.load(path)) {
            cascade_loaded = true;
            std::cout << "[Presence] Haar cascade carregado de: " << path << std::endl;
            break;
        }
    }
    if (!cascade_loaded) {
        std::cerr << "[Presence] Cascade não encontrado (" << cascade_path
                  << "). Detecção de presença desativada." << std::endl;
    }
}

cv::Mat PresenceDetector::grab_frame(std::string& error_out) {
    cv::VideoCapture cap;
    if (!open_camera(cap, camera_index)) {
        error_out = "webcam indisponível (índice " + std::to_string(camera_index) + ")";
        return {};
    }
    configure_capture(cap);

    // Descarta os primeiros frames: várias webcams entregam frames escuros
    // enquanto ajustam exposição.
    cv::Mat frame;
    for (int i = 0; i < 3; ++i) cap.read(frame);
    if (!cap.read(frame) || frame.empty()) {
        error_out = "falha ao capturar frame da webcam";
        return {};
    }
    return frame; // cap fecha no destrutor → LED apaga
}

PresenceResult PresenceDetector::check() {
    // Stream ativo = a webcam já é nossa; devolve o último frame analisado.
    if (streaming) return latest();

    std::lock_guard<std::mutex> lock(webcam_mtx);
    PresenceResult result;

    if (!cascade_loaded) {
        result.error = "cascade de detecção facial não carregado";
        return result;
    }

    std::string error;
    cv::Mat frame = grab_frame(error);
    if (frame.empty()) {
        result.error = error;
        return result;
    }

    return analyze(frame);
}

PresenceResult PresenceDetector::analyze(cv::Mat& frame) {
    double brightness = 0.0;
    std::vector<cv::Rect> faces = detect_faces(frame, brightness);
    return compose_result(frame, faces, brightness);
}

std::vector<cv::Rect> PresenceDetector::detect_faces(const cv::Mat& frame,
                                                     double& brightness_out) {
    // Detecta numa cópia reduzida: Haar em 720p custa caro à toa — o cascade
    // frontal funciona igual em ~400px, e a captura pôde subir de resolução
    // (preview mais nítido) sem estourar o custo da detecção.
    const double scale = std::min(1.0, static_cast<double>(kDetectWidth) / frame.cols);
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    if (scale < 1.0) {
        cv::resize(gray, gray, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    brightness_out = cv::mean(gray)[0];
    cv::equalizeHist(gray, gray);

    // 60px de rosto na escala antiga (640) ≈ 37px na escala de detecção (400)
    const int min_face = std::max(24, static_cast<int>(60 * (kDetectWidth / 640.0)));
    std::vector<cv::Rect> faces;
    face_cascade.detectMultiScale(gray, faces, 1.1, 5, 0, cv::Size(min_face, min_face));

    // Volta os retângulos para as coordenadas do frame original
    if (scale < 1.0) {
        const double inv = 1.0 / scale;
        for (auto& f : faces) {
            f.x = static_cast<int>(f.x * inv);
            f.y = static_cast<int>(f.y * inv);
            f.width = static_cast<int>(f.width * inv);
            f.height = static_cast<int>(f.height * inv);
        }
    }
    return faces;
}

PresenceResult PresenceDetector::compose_result(const cv::Mat& frame,
                                                const std::vector<cv::Rect>& faces,
                                                double brightness) {
    PresenceResult result;
    result.available = true;
    result.brightness = brightness;
    result.face_count = static_cast<int>(faces.size());
    result.present = !faces.empty();

    // Preview para a aba Vision da CLI: kPreviewWidth colunas de meia-célula
    // '▀' no terminal. INTER_AREA no downscale = menos aliasing que o default.
    // Os retângulos são desenhados já na escala reduzida (espessura 1 = 1 px
    // do preview, visível após o downscale).
    const int preview_h = std::max(2, frame.rows * kPreviewWidth / frame.cols);
    const double sx = static_cast<double>(kPreviewWidth) / frame.cols;
    const double sy = static_cast<double>(preview_h) / frame.rows;
    cv::resize(frame, result.preview, cv::Size(kPreviewWidth, preview_h), 0, 0, cv::INTER_AREA);
    for (const auto& face : faces) {
        cv::Rect scaled(static_cast<int>(face.x * sx), static_cast<int>(face.y * sy),
                        static_cast<int>(face.width * sx), static_cast<int>(face.height * sy));
        cv::rectangle(result.preview, scaled, cv::Scalar(0, 255, 0), 1);
    }
    cv::cvtColor(result.preview, result.preview, cv::COLOR_BGR2RGB);
    return result;
}

PresenceDetector::~PresenceDetector() {
    stop_stream();
}

void PresenceDetector::start_stream(std::function<void()> on_frame) {
    std::lock_guard<std::mutex> lock(stream_lifecycle_mtx);
    if (streaming || !cascade_loaded) return;
    {
        std::lock_guard<std::mutex> latest_lock(latest_mtx);
        latest_result = {};
        latest_result.error = "ligando a webcam...";
        latest_frame.release();
    }
    on_new_frame = std::move(on_frame);
    streaming = true;
    stream_thread = std::thread(&PresenceDetector::stream_loop, this);
}

void PresenceDetector::stop_stream() {
    std::lock_guard<std::mutex> lock(stream_lifecycle_mtx);
    streaming = false;
    if (stream_thread.joinable()) stream_thread.join();
    on_new_frame = nullptr;
}

PresenceResult PresenceDetector::latest() {
    std::lock_guard<std::mutex> lock(latest_mtx);
    return latest_result;
}

void PresenceDetector::publish(const PresenceResult& result, const cv::Mat& frame) {
    {
        std::lock_guard<std::mutex> lock(latest_mtx);
        latest_result = result;
        if (!frame.empty()) latest_frame = frame.clone();
    }
    if (on_new_frame) on_new_frame();
}

void PresenceDetector::stream_loop() {
    cv::VideoCapture cap;
    cv::Mat frame;
    int frame_counter = 0;
    std::vector<cv::Rect> last_faces;
    double last_brightness = 0.0;
    bool have_detection = false;

    while (streaming) {
        if (!cap.isOpened()) {
            open_camera(cap, camera_index);
            if (cap.isOpened()) {
                configure_capture(cap);
                std::cout << "[Presence] Stream em "
                          << static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)) << "x"
                          << static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)) << " @ "
                          << static_cast<int>(cap.get(cv::CAP_PROP_FPS)) << " FPS" << std::endl;
            } else {
                PresenceResult fail;
                fail.error = "webcam indisponível (índice " + std::to_string(camera_index) + ")";
                publish(fail, {});
                // Tenta de novo em 2s, dormindo em passos curtos p/ stop responsivo
                for (int i = 0; i < 20 && streaming; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        if (!cap.read(frame) || frame.empty()) {
            cap.release(); // câmera caiu (desplugada?) — força reabrir no próximo ciclo
            continue;
        }

        // Preview publicado em TODOS os frames (~30 FPS de fluidez); a
        // detecção Haar (a parte cara) roda a cada 3 frames e os retângulos
        // são reutilizados nos frames intermediários.
        if (frame_counter++ % 3 == 0 || !have_detection) {
            last_faces = detect_faces(frame, last_brightness);
            have_detection = true;
        }
        publish(compose_result(frame, last_faces, last_brightness), frame);
    }
    // cap fecha no destrutor → LED apaga
}

std::string PresenceDetector::capture_to_file(const std::string& filename) {
    cv::Mat frame;
    if (streaming) {
        // Stream é dono da webcam: usa o frame mais recente dele.
        std::lock_guard<std::mutex> lock(latest_mtx);
        frame = latest_frame.clone();
        if (frame.empty()) return "ERRO: stream ativo mas ainda sem frame capturado";
    } else {
        std::lock_guard<std::mutex> lock(webcam_mtx);
        std::string error;
        frame = grab_frame(error);
        if (frame.empty()) {
            return "ERRO: " + error;
        }
    }
    if (!cv::imwrite(filename, frame)) {
        return "ERRO: falha ao salvar imagem da webcam em " + filename;
    }
    return "Foto da webcam salva em '" + filename + "' (" +
           std::to_string(frame.cols) + "x" + std::to_string(frame.rows) + ")";
}

} // namespace alyssa_vision
