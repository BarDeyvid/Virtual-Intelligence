// Inclui a biblioteca principal do whisper
#include "whisper.h"
// Inclui o PortAudio para captura de microfone
#include "portaudio.h"

// Bibliotecas padrão
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric> // Para std::inner_product

// Bibliotecas para threading e filas
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Definições de áudio
#define SAMPLE_RATE 16000
#define FRAMES_PER_BUFFER 512
#define CHANNELS 1

// Duração do buffer de áudio (30 segundos * 16000 amostras/segundo)
#define WHISPER_MAX_AUDIO_SAMPLES 30 * 16000

// --- VAD (Voice Activity Detection) Simples ---
const float VAD_RMS_THRESHOLD = 0.02f;     // Limiar de energia (RMS) para considerar "fala"
const int VAD_SILENCE_MS = 1000;          // Tempo de silêncio para "fechar" um segmento de fala
const size_t VAD_MIN_SAMPLES = (size_t)(0.2 * SAMPLE_RATE); // Segmento mínimo (200ms)

// --- Estrutura para o Stream de Áudio ---
struct AudioData {
    std::vector<float> buffer;
    size_t write_pos = 0;
    bool stream_ready = false;
};

// --- Fila Thread-Safe ---
template <typename T>
class ThreadSafeQueue {
    public:
        void push(T value) {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(std::move(value));
            cv.notify_one();
        }

        // Pop bloqueante
        bool pop(T& value) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]{ return !q.empty() || !running; });
            if (!running && q.empty()) return false;
            value = std::move(q.front());
            q.pop();
            return true;
        }

        // Tenta pop
        bool try_pop(T& value) {
            std::lock_guard<std::mutex> lock(mtx);
            if (q.empty()) {
                return false;
            }
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

// --- Globais (Filas e Controle) ---
ThreadSafeQueue<std::vector<float>> whisper_input_queue;
ThreadSafeQueue<std::string> whisper_output_queue;
std::atomic<bool> g_running(true); // Flag global para parar threads

// --- Callback de Captura de Áudio (PortAudio) ---
static int paCallback(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void *userData)
{
    const int16_t *input = (const int16_t*)inputBuffer;
    AudioData *data = (AudioData*)userData;

    if (!data->stream_ready) {
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float sample = (float)input[i] / 32768.0f; 
        
        if (data->write_pos < data->buffer.size()) {
            data->buffer[data->write_pos++] = sample;
        } else {
            data->write_pos = 0;
            data->buffer[data->write_pos++] = sample;
        }
    }

    return paContinue;
}

// --- Função de Transcrição ---
std::string process_transcription(struct whisper_context * ctx, const std::vector<float>& audio_buffer, size_t n_samples) {
    if (n_samples == 0) return "";

    // 1. Configurar Opções de Transcrição
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false; // Desativado para ser mais limpo (como no Python)
    params.n_threads        = 12; 
    params.language         = "pt"; // Configurado para Português
    // params.suppress_non_speech_tokens = true; // Útil

    std::cout << "\n[PROCESSANDO " << (n_samples / (float)SAMPLE_RATE) << "s...]\n";

    // 2. Transcrever o buffer
    if (whisper_full(ctx, params, audio_buffer.data(), n_samples) != 0) {
        std::cerr << "ERRO: Falha na execução da transcrição (whisper_full)." << std::endl;
        return "";
    }

    // 3. Obter Resultados
    std::string full_text = "";
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; i++) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        full_text += text;
    }
    
    // Remove espaços extras (como o .strip() do Python)
    full_text.erase(0, full_text.find_first_not_of(" \t\n\r\f\v"));
    full_text.erase(full_text.find_last_not_of(" \t\n\r\f\v") + 1);

    return full_text;
}


// --- Função Worker (equivalente ao whisper_worker do Python) ---
void whisper_worker_thread(struct whisper_context * ctx) {
    std::vector<float> audio_segment;

    std::cout << "🧠 Whisper worker thread iniciada." << std::endl;

    while (g_running) {
        // Tenta pegar um segmento da fila (bloqueia até ter um)
        if (whisper_input_queue.pop(audio_segment)) {
            if (!g_running) break;

            // Transcreve o segmento
            std::string transcription = process_transcription(ctx, audio_segment, audio_segment.size());
            
            // Envia o resultado para a fila de saída
            if (!transcription.empty()) {
                whisper_output_queue.push(transcription);
            }
        } else if (!g_running) {
            break;
        }
    }
    std::cout << "🛑 Whisper worker thread encerrando." << std::endl;
}

// --- Função VAD Simples (Baseada em RMS) ---
bool is_speech(const std::vector<float>& audio_chunk) {
    if (audio_chunk.empty()) return false;
    double sum_sq = std::inner_product(audio_chunk.begin(), audio_chunk.end(), audio_chunk.begin(), 0.0);
    double rms = std::sqrt(sum_sq / audio_chunk.size());
    return rms > VAD_RMS_THRESHOLD;
}

// --- Função Principal (Reestruturada) ---
int main(int argc, char **argv) {
    std::cout.setf(std::ios::unitbuf); 

    // 1. Definições
    const std::string model_path = "models/ggml-large-v3-turbo.bin"; 

    // 2. Carregar o Modelo
    std::cout << "Tentando carregar o modelo GGML/Whisper: " << model_path << std::endl;
    struct whisper_context_params cparams = whisper_context_default_params();
    struct whisper_context * ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    // ... (tratamento de erro do modelo) ...
    if (ctx == nullptr) {
        std::cerr << "ERRO: Falha ao carregar o modelo." << std::endl;
        return 1;
    }
    std::cout << "Modelo carregado com sucesso.\n" << std::endl;

    // 3. Inicializar PortAudio
    PaStream *stream;
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "ERRO PortAudio: Falha ao inicializar." << std::endl;
        whisper_free(ctx);
        return 1;
    }

    // 4. Configuração do Stream
    AudioData audio_data;
    audio_data.buffer.resize(WHISPER_MAX_AUDIO_SAMPLES);
    std::fill(audio_data.buffer.begin(), audio_data.buffer.end(), 0.0f); 

    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cerr << "ERRO: Nenhum dispositivo de entrada de áudio padrão encontrado." << std::endl;
        Pa_Terminate();
        whisper_free(ctx);
        return 1;
    }
    inputParameters.channelCount = CHANNELS;
    inputParameters.sampleFormat = paInt16; 
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
              &stream, &inputParameters, NULL, 
              SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, 
              paCallback, &audio_data);
    if (err != paNoError) {
        std::cerr << "ERRO PortAudio: Falha ao abrir stream." << std::endl;
        Pa_Terminate();
        whisper_free(ctx);
        return 1;
    }

    // 5. Loop Principal (MODIFICADO)
    
    // Inicia o worker (como no Python)
    std::thread worker(whisper_worker_thread, ctx);

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "ERRO PortAudio: Falha ao iniciar stream." << std::endl;
        g_running = false;
        worker.join();
        Pa_CloseStream(stream);
        Pa_Terminate();
        whisper_free(ctx);
        return 1;
    }
    
    std::cout << "--- ESCUTANDO... (Arquitetura Python) ---" << std::endl;
    std::cout << "Pressione Ctrl+C para sair." << std::endl;
    
    audio_data.stream_ready = true;
    
    // Variáveis para VAD e buffering (como no Python)
    size_t last_read_pos = 0;
    std::vector<float> speech_buffer; // (self.segment_buffer do Python)
    auto last_speech_time = std::chrono::steady_clock::now();
    const auto silence_duration = std::chrono::milliseconds(VAD_SILENCE_MS);

    while (Pa_IsStreamActive(stream) == 1) {
        // Poll com mais frequência (como o stream do Python)
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Poll 10x por segundo

        // --- 1. Ler novos samples do buffer circular ---
        size_t current_write_pos = audio_data.write_pos;
        size_t n_new_samples = 0;
        
        if (current_write_pos != last_read_pos) {
            std::vector<float> chunk;
            if (current_write_pos > last_read_pos) {
                n_new_samples = current_write_pos - last_read_pos;
                chunk.assign(audio_data.buffer.begin() + last_read_pos, audio_data.buffer.begin() + current_write_pos);
            } else { // Wrap-around
                n_new_samples = (audio_data.buffer.size() - last_read_pos) + current_write_pos;
                chunk.resize(n_new_samples);
                std::copy(audio_data.buffer.begin() + last_read_pos, audio_data.buffer.end(), chunk.begin());
                std::copy(audio_data.buffer.begin(), audio_data.buffer.begin() + current_write_pos, chunk.begin() + (audio_data.buffer.size() - last_read_pos));
            }
            last_read_pos = current_write_pos;

            // --- 2. Lógica VAD/Buffering (como o _handle_speaker_segment) ---
            if (is_speech(chunk)) {
                speech_buffer.insert(speech_buffer.end(), chunk.begin(), chunk.end());
                last_speech_time = std::chrono::steady_clock::now();
            } else if (!speech_buffer.empty()) {
                // É silêncio, mas temos um buffer
                auto now = std::chrono::steady_clock::now();
                if ((now - last_speech_time > silence_duration) && (speech_buffer.size() > VAD_MIN_SAMPLES)) {
                    // Passou tempo suficiente de silêncio, enviar o buffer
                    std::cout << "[VAD] Segmento de " << (speech_buffer.size() / (float)SAMPLE_RATE) << "s detectado. Enviando...\n";
                    whisper_input_queue.push(speech_buffer);
                    speech_buffer.clear(); // Limpa o buffer (como no Python)
                }
            }
        }

        // --- 3. Consumir resultados (como o get_last_result() do Python) ---
        std::string result_text;
        if (whisper_output_queue.try_pop(result_text)) {
            // (O Python tem "speaker", nós só temos o texto)
            std::cout << "🗣️ [TRANSCRIÇÃO]: " << result_text << std::endl;
        }
    }

    // 6. Limpeza
    std::cout << "\nEncerrando..." << std::endl;
    g_running = false;
    whisper_input_queue.stop(); // Sinaliza para a fila parar
    
    if (worker.joinable()) {
        worker.join(); // Espera a thread worker terminar
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    whisper_free(ctx);
    
    std::cout << "Programa encerrado." << std::endl;
    return 0;
}