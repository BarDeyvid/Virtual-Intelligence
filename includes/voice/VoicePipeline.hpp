#ifndef VOICE_PIPELINE_HPP
#define VOICE_PIPELINE_HPP

#include "whisper.h"
#include "portaudio.h"

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <iostream>
#include <numeric> // Para std::inner_product
#include <cmath>   // Para std::sqrt

// Define a 16kHz sample rate
#define SAMPLE_RATE 16000

/**
 * @brief Class representing the voice processing pipeline.
 * 
 * This class handles audio capture, Voice Activity Detection (VAD), and transcription using Whisper model.
 */
class VoicePipeline {
public:
    // --- Configurações ---

    /**
     * @brief Options structure to hold configuration parameters for the VoicePipeline.
     */
    struct Options {
        int n_threads = 4;          ///< Number of threads for Whisper processing
        std::string language = "pt"; ///< Language setting

        // VAD (Voice Activity Detection) settings
        float vad_rms_threshold = 0.02f;       ///< RMS threshold for detecting speech (fallback se o Silero não carregar)
        int vad_silence_ms = 450;              ///< Silence duration in ms to consider end of a speech segment
        int vad_min_duration_ms = 200;         ///< Minimum speech duration in ms to process

        // Silero VAD (streaming, via whisper.cpp). Se o arquivo não existir o
        // pipeline cai no RMS antigo e alarga vad_silence_ms para >= 1000ms
        // (o RMS não é confiável o bastante para janela curta).
        std::string vad_model_path = "models/ggml-silero-v5.1.2.bin";
        float vad_threshold = 0.5f;            ///< Probabilidade mínima do Silero para considerar fala
        int vad_silence_extended_ms = 1500;    ///< Janela alargada quando o endpoint probe diz "frase incompleta"
        int vad_preroll_ms = 300;              ///< Áudio pré-fala prepended ao segmento (não corta a 1ª sílaba)

        // Decodificação Whisper
        int beam_size = 5;                     ///< Beam search (mais robusto a sotaque que greedy)
        /// Prompt de condicionamento: puxa o decoder para pt-BR informal
        /// falado no Sul do Brasil (sotaque de colônia, influência de
        /// imigração alemã/francesa). Vazio = desliga.
        std::string initial_prompt =
            "Transcrição de uma conversa informal em português brasileiro, "
            "falada no Sul do Brasil com sotaque de influência alemã e "
            "francesa. Falamos sobre tecnologia, programação, jogos e o "
            "dia a dia.";
    };

    /**
     * @brief Constructor. Loads the Whisper model and initializes PortAudio.
     *
     * @param model_path Path to the GGML model file (e.g., "models/ggml-base.bin")
     * @param options Configuration options for VoicePipeline
     * @param defer_model_load true = não carrega o modelo agora (o
     *        VRAMResourceManager decide quando, via load_model/unload_model).
     */
    VoicePipeline(const std::string& model_path, Options options = {},
                  bool defer_model_load = false);

    /**
     * @brief Destructor. Stops all processes and releases resources.
     */
    ~VoicePipeline();

    /**
     * @brief Starts audio capture and processing threads.
     * 
     * @return true if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops audio capture and processing threads.
     */
    void stop();

    /**
     * @brief Gets the last transcription result from the output queue (non-blocking).
     * 
     * @param result String to store the transcription result
     * @return true if a new result was obtained, false if the queue was empty
     */
    bool get_last_result(std::string& result);

    /**
     * @brief Pauses audio processing (VAD and callback).
     */
    void pause();

    /**
     * @brief Resumes audio processing.
     */
    void resume();

    // --- Residência do modelo (scheduler de VRAM, plano_scheduler/) ---------

    /**
     * @brief Carrega o modelo Whisper para a VRAM (bloqueante e idempotente).
     * @return true se o modelo está carregado ao retornar.
     * @details Chamado pelo WhisperResident em thread própria. Seguro contra
     *          transcrição concorrente (mutex do modelo).
     */
    bool load_model();

    /**
     * @brief Descarrega o modelo Whisper da VRAM (bloqueante e idempotente).
     * @details Espera qualquer transcrição em andamento terminar. A captura
     *          de áudio e o VAD continuam rodando — só a transcrição passa a
     *          exigir um novo load (JIT).
     */
    void unload_model();

    /// true quando o contexto Whisper está carregado.
    bool model_loaded() const { return m_ctx != nullptr; }

    /**
     * @brief Callback disparado quando o VAD detecta INÍCIO de fala.
     * @details Gancho do scheduler: request_state(LISTENING) pode começar o
     *          load do Whisper enquanto o usuário ainda está falando.
     *          Chamado na thread do VAD — deve ser barato/não bloqueante.
     */
    void set_on_speech_start(std::function<void()> cb) { m_on_speech_start = std::move(cb); }

    /**
     * @brief Probe de endpoint semântico (turn detection adaptativo).
     * @details Chamado na thread do VAD quando o silêncio passa de
     *          vad_silence_ms. Recebe o áudio do turno até aqui e responde:
     *          true  = frase completa → commita o turno já;
     *          false = usuário provavelmente vai continuar ("eu tava pensando
     *          em...") → espera vad_silence_extended_ms antes de commitar.
     *          Sem probe registrado, commita sempre em vad_silence_ms.
     *          Gancho para um modelo tipo smart-turn (ONNX/CPU) no futuro.
     */
    void set_endpoint_probe(std::function<bool(const std::vector<float>&)> probe) {
        m_endpoint_probe = std::move(probe);
    }

private:
    // --- Estruturas Internas ---

    /**
     * @brief Thread-safe queue for communication between threads.
     * 
     * Template class to handle thread-safe operations on a queue.
     */
    template <typename T>
    class ThreadSafeQueue {
    public:
        /**
         * @brief Pushes an item into the queue and notifies waiting consumers.
         * * @param value Item to push
         */
        void push(T value) {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(std::move(value));
            cv.notify_one();
        }

        /**
         * @brief Pops an item from the queue (blocking).
         * * Waits until an item is available or the queue is stopped.
         * * @param value Item popped from the queue
         * @return true if an item was successfully obtained, false if the queue is stopped and empty
         */
        bool pop(T& value) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return !running || !q.empty(); });

            if (!running && q.empty()) {
                return false;
            }

            value = std::move(q.front());
            q.pop();
            return true;
        }

        /**
         * @brief Pops an item from the queue (non-blocking).
         * * Does not wait if no items are available.
         * * @param value Item popped from the queue
         * @return true if an item was successfully obtained, false if the queue is empty
         */
        bool try_pop(T& value) {
            std::lock_guard<std::mutex> lock(mtx);
            if (q.empty()) {
                return false;
            }
            value = std::move(q.front());
            q.pop();
            return true;
        }

        /**
         * @brief Stops all operations and notifies waiting consumers to exit.
         */
        void stop() {
            std::lock_guard<std::mutex> lock(mtx);
            running = false;
            cv.notify_all(); // Acorda todas as threads esperando em pop()
        }
    private:
        std::queue<T> q;                 ///< Internal queue storage
        std::mutex mtx;                  ///< Mutex for thread-safe access
        std::condition_variable cv;     ///< Condition variable for synchronization
        bool running = true;            ///< Flag indicating if the queue is still running
    };

    /**
     * @brief Structure to hold audio data from PortAudio.
     */
    struct AudioData {
        std::vector<float> buffer;      ///< Audio buffer storage
        size_t write_pos = 0;           ///< Write position in the buffer
        bool stream_ready = false;       ///< Flag indicating if the audio stream is ready
    };

    // --- Funções de Thread ---

    /**
     * @brief Function for the Whisper worker thread.
     * 
     * Consumes audio segments from the input queue, processes them for transcription, and pushes results to the output queue.
     */
    void _whisper_worker_func();
    
    /**
     * @brief Function for the VAD loop thread.
     * 
     * Detects speech using Voice Activity Detection (VAD) and enqueues audio segments for processing.
     */
    void _vad_loop_func();

    // --- Funções de Implementação ---

    /**
     * @brief PortAudio callback implementation.
     * 
     * Captures raw audio data from the microphone and writes it to the internal buffer.
     * 
     * @param input Pointer to input buffer
     * @param frameCount Number of frames in the input buffer
     * @return paContinue if successful, error code otherwise
     */
    int _pa_callback_impl(const void* input, unsigned long frameCount);
    
    /**
     * @brief Processes an audio buffer for transcription.
     * 
     * Uses the Whisper model to transcribe the given audio buffer and returns the resulting text.
     * 
     * @param audio_buffer Audio samples
     * @return Transcription result as a string
     */
    std::string _process_transcription(const std::vector<float>& audio_buffer);
    
    /**
     * @brief Voice Activity Detection (VAD) logic.
     *
     * Usa o Silero VAD (streaming) quando carregado; senão cai no limiar RMS.
     *
     * @param audio_chunk Audio samples
     * @return true if speech is detected, false otherwise
     */
    bool _is_speech(const std::vector<float>& audio_chunk);

    /**
     * @brief Carrega o modelo Silero VAD (CPU, ~2MB).
     * @return true se o contexto VAD está pronto.
     */
    bool _load_vad_model();

    /**
     * @brief Static PortAudio callback function.
     * 
     * Wraps the non-static `_pa_callback_impl` method to be used by PortAudio.
     * 
     * @param inputBuffer Pointer to input buffer
     * @param outputBuffer Pointer to output buffer (not used here)
     * @param framesPerBuffer Number of frames in the input/output buffers
     * @param timeInfo Time information about the audio stream
     * @param statusFlags Status flags from PortAudio
     * @param userData User-defined data (VoicePipeline instance)
     * @return paContinue if successful, error code otherwise
     */
    static int _pa_callback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData);

    Options m_options;                 ///< Configuration options
    std::string m_model_path;          ///< Caminho do .bin (para load JIT do scheduler)
    struct whisper_context* m_ctx = nullptr;  ///< Whisper model context
    mutable std::mutex m_model_mtx;    ///< Serializa load/unload/transcrição do modelo
    std::function<void()> m_on_speech_start; ///< Gancho do scheduler (VAD thread)
    std::function<bool(const std::vector<float>&)> m_endpoint_probe; ///< Endpoint semântico (VAD thread)
    struct whisper_vad_context* m_vad_ctx = nullptr; ///< Silero VAD (CPU); nullptr = fallback RMS
    PaStream* m_stream = nullptr;      ///< PortAudio stream

    AudioData m_audio_data;            ///< Internal audio buffer storage
    size_t m_vad_min_samples;          ///< Minimum samples for VAD to consider speech
    const size_t m_buffer_size_samples = 30 * SAMPLE_RATE; // Circular buffer of 30s
    const int m_pa_frames_per_buffer = 512;  /// PortAudio frames per buffer

    // Comunicação
    ThreadSafeQueue<std::vector<float>> m_input_queue;  ///< Queue for audio segments
    ThreadSafeQueue<std::string> m_output_queue;     ///< Queue for transcription results

    // Controle de Threads
    std::atomic<bool> m_running{false};                ///< Atomic flag indicating if the pipeline is running
    std::atomic<bool> m_is_paused{false};              ///< Atomic flag indicating if processing is paused
    std::thread m_worker_thread;                      ///< Thread for Whisper processing
    std::thread m_vad_thread;                         ///< Thread for VAD and audio segmenting
};

#endif 