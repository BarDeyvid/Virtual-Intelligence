#ifndef VOICE_PIPELINE_HPP
#define VOICE_PIPELINE_HPP

#include "whisper.h"
#include "portaudio.h"

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <atomic>

// Define a 16kHz
#define SAMPLE_RATE 16000

class VoicePipeline {
public:
    // --- Configurações ---
    struct Options {
        int n_threads = 4;
        std::string language = "pt";
        
        // VAD
        float vad_rms_threshold = 0.02f;
        int vad_silence_ms = 1000;      // 1s de silêncio para fechar o segmento
        int vad_min_duration_ms = 200; // 200ms de fala mínima
    };

    /**
     * @brief Construtor. Carrega o modelo e inicializa o PortAudio.
     * @param model_path Caminho para o modelo GGML (ex: "models/ggml-base.bin")
     */
    VoicePipeline(const std::string& model_path, Options options = {4, "en", 0.02f, 1000, 200});

    /**
     * @brief Destrutor. Para tudo e libera todos os recursos.
     */
    ~VoicePipeline();

    /**
     * @brief Inicia a captura de áudio e as threads de processamento.
     * @return true se iniciou com sucesso, false se falhou.
     */
    bool start();

    /**
     * @brief Para a captura e as threads.
     */
    void stop();

    /**
     * @brief Obtém o último resultado de transcrição da fila (não-bloqueante).
     * @param result A string onde o resultado será armazenado.
     * @return true se um novo resultado foi obtido, false se a fila estava vazia.
     */
    bool get_last_result(std::string& result);

    /** @brief Pausa o processamento de áudio (VAD e Callback). */
    void pause();

    /** @brief Retoma o processamento de áudio. */
    void resume();


private:
    // --- Estruturas Internas ---

    // Fila thread-safe para comunicação entre threads
    template <typename T>
    class ThreadSafeQueue {
    public:
        void push(T value) {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(std::move(value));
            cv.notify_one();
        }
        bool pop(T& value) { // Bloqueante
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]{ return !q.empty() || !running; });
            if (!running && q.empty()) return false;
            value = std::move(q.front());
            q.pop();
            return true;
        }
        bool try_pop(T& value) { // Não-bloqueante
            std::lock_guard<std::mutex> lock(mtx);
            if (q.empty()) return false;
            value = std::move(q.front());
            q.pop();
            return true;
        }
        void stop() {
            std::lock_guard<std::mutex> lock(mtx);
            running = false;
            cv.notify_all();
        }
    private:
        std::queue<T> q;
        std::mutex mtx;
        std::condition_variable cv;
        bool running = true;
    };

    // Buffer de áudio do PortAudio
    struct AudioData {
        std::vector<float> buffer;
        size_t write_pos = 0;
        bool stream_ready = false;
    };

    // --- Funções de Thread ---

    /** A thread que executa o modelo Whisper (Consumidor) */
    void _whisper_worker_func();
    
    /** A thread que executa o VAD e enfileira segmentos (Produtor) */
    void _vad_loop_func();

    // --- Funções de Implementação ---

    /** A implementação do callback do PortAudio (chamada pelo C-style) */
    int _pa_callback_impl(const void* input, unsigned long frameCount);
    
    /** Processa um buffer de áudio e retorna o texto */
    std::string _process_transcription(const std::vector<float>& audio_buffer);
    
    /** Lógica VAD simples */
    bool _is_speech(const std::vector<float>& audio_chunk);

    static int _pa_callback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData);

    Options m_options;
    struct whisper_context* m_ctx = nullptr;
    PaStream* m_stream = nullptr;
    
    AudioData m_audio_data;
    size_t m_vad_min_samples;
    const size_t m_buffer_size_samples = 30 * SAMPLE_RATE; // Buffer circular de 30s
    const int m_pa_frames_per_buffer = 512;

    // Comunicação
    ThreadSafeQueue<std::vector<float>> m_input_queue;
    ThreadSafeQueue<std::string> m_output_queue;

    // Controle de Threads
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_is_paused{false}; 
    std::thread m_worker_thread; // Thread do Whisper
    std::thread m_vad_thread;    // Thread do VAD
};

#endif 