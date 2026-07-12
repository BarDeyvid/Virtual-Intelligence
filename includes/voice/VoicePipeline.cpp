#include "VoicePipeline.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>

// --- Construtor e Destrutor ---

/**
 * @brief Constructor implementation.
 * 
 * Loads the Whisper model and initializes PortAudio with the provided configuration options.
 */
VoicePipeline::VoicePipeline(const std::string& model_path, Options options,
                             bool defer_model_load)
    : m_options(options), m_model_path(model_path) {

    // Log do whisper.cpp é global e por default cospe INFO/DEBUG no stderr —
    // o Silero VAD loga 3 linhas POR CHUNK (20x/s) e afoga o terminal.
    // Só WARN pra cima passa (mesmo padrão do llama_log_set no AlyssaNet).
    whisper_log_set([](enum ggml_log_level level, const char* text, void*) {
        if (level >= GGML_LOG_LEVEL_WARN) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);

    m_vad_min_samples = (size_t)(m_options.vad_min_duration_ms / 1000.0 * SAMPLE_RATE);

    // 1. Carregar Modelo Whisper (a menos que o scheduler de VRAM vá decidir,
    //    ou que o pipeline seja só VAD — gameplay por voz não transcreve)
    if (m_options.vad_only) {
        std::cout << "[Whisper] Modo vad_only: sem transcrição, sem modelo." << std::endl;
    } else if (!defer_model_load) {
        if (!load_model()) {
            throw std::runtime_error("ERRO: Falha ao carregar o modelo Whisper.");
        }
    } else {
        std::cout << "[Whisper] Load adiado (gerenciado pelo VRAMResourceManager)." << std::endl;
    }

    // 2. Inicializar PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        unload_model();
        throw std::runtime_error("ERRO: Falha ao inicializar PortAudio.");
    }

    // 3. Preparar buffer de áudio
    m_audio_data.buffer.resize(m_buffer_size_samples);
    std::fill(m_audio_data.buffer.begin(), m_audio_data.buffer.end(), 0.0f);

    // 4. Silero VAD (streaming). Sem ele o RMS não distingue fala de ruído,
    //    então a janela curta de silêncio vira commit prematuro — alarga.
    if (!_load_vad_model() && m_options.vad_silence_ms < 1000) {
        std::cout << "[VAD] Silero indisponível — fallback RMS com "
                     "vad_silence_ms alargado de " << m_options.vad_silence_ms
                  << "ms para 1000ms." << std::endl;
        m_options.vad_silence_ms = 1000;
    }
}

/**
 * @brief Destructor implementation.
 * 
 * Stops the pipeline, frees resources, and terminates PortAudio.
 */
VoicePipeline::~VoicePipeline() {
    stop(); // Garante que tudo parou

    unload_model();
    if (m_vad_ctx) {
        whisper_vad_free(m_vad_ctx);
        m_vad_ctx = nullptr;
    }
    Pa_Terminate();
    std::cout << "VoicePipeline encerrada." << std::endl;
}

// --- Residência do modelo (scheduler de VRAM) --------------------------------

/**
 * @brief Loads the Whisper model into VRAM (blocking, idempotent).
 */
bool VoicePipeline::load_model() {
    std::lock_guard<std::mutex> lock(m_model_mtx);
    if (m_ctx) return true; // idempotente

    std::cout << "[Whisper] Carregando modelo de: " << m_model_path << std::endl;
    struct whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(m_model_path.c_str(), cparams);
    if (m_ctx == nullptr) {
        std::cerr << "[Whisper] ERRO: falha ao carregar " << m_model_path << std::endl;
        return false;
    }
    std::cout << "[Whisper] Modelo carregado." << std::endl;
    return true;
}

/**
 * @brief Unloads the Whisper model from VRAM (blocking, idempotent).
 */
void VoicePipeline::unload_model() {
    std::lock_guard<std::mutex> lock(m_model_mtx); // espera transcrição em curso
    if (!m_ctx) return;
    whisper_free(m_ctx);
    m_ctx = nullptr;
    std::cout << "[Whisper] Modelo descarregado da VRAM." << std::endl;
}

// --- Controles Públicos ---

/**
 * @brief Starts the VoicePipeline.
 * 
 * Initializes PortAudio stream, starts audio capture and processing threads.
 */
bool VoicePipeline::start() {
    if (m_running) {
        std::cerr << "AVISO: VoicePipeline já está em execução." << std::endl;
        return true;
    }

    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cerr << "ERRO: Nenhum dispositivo de entrada de áudio encontrado." << std::endl;
        return false;
    }

    // Sempre dizer QUAL mic abriu: "não tá me escutando" quase sempre é o
    // Windows apontando o default pra outro dispositivo (webcam, virtual).
    if (const PaDeviceInfo* dev = Pa_GetDeviceInfo(inputParameters.device)) {
        std::cout << "[Audio] Microfone: \"" << dev->name << "\"" << std::endl;
    }
    
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paInt16; 
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    PaError err = Pa_OpenStream(
              &m_stream,
              &inputParameters,
              NULL,
              SAMPLE_RATE,
              m_pa_frames_per_buffer,
              paClipOff,
              _pa_callback, // Callback estático
              this );        // Passa 'this' como userData
              
    if (err != paNoError) {
        std::cerr << "ERRO: Falha ao abrir stream PortAudio. " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        std::cerr << "ERRO: Falha ao iniciar stream PortAudio. " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(m_stream);
        return false;
    }

    m_running = true;
    m_audio_data.stream_ready = true;
    
    // Inicia as threads
    m_worker_thread = std::thread(&VoicePipeline::_whisper_worker_func, this);
    m_vad_thread = std::thread(&VoicePipeline::_vad_loop_func, this);

    std::cout << "--- VoicePipeline INICIADA. Escutando... ---" << std::endl;
    return true;
}

/**
 * @brief Stops the VoicePipeline.
 * 
 * Signals all threads to stop and waits for them to finish.
 */
void VoicePipeline::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;
    m_audio_data.stream_ready = false;

    // Sinaliza para as filas pararem
    m_input_queue.stop();
    m_output_queue.stop();

    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }

    // Espera as threads terminarem
    if (m_vad_thread.joinable()) {
        m_vad_thread.join();
    }
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }
    
    std::cout << "\n--- VoicePipeline PARADA. ---" << std::endl;
}

/**
 * @brief Gets the last transcription result.
 * 
 * Attempts to retrieve the latest transcription from the output queue without blocking.
 */
bool VoicePipeline::get_last_result(std::string& result) {
    return m_output_queue.try_pop(result);
}

// --- Lógica Interna (Threads) ---

/**
 * @brief Whisper worker thread function.
 * 
 * Consumes audio segments, transcribes them using the Whisper model, and enqueues results.
 */
void VoicePipeline::_whisper_worker_func() {
    std::vector<float> audio_segment;
    std::cout << "🧠 Whisper worker thread iniciada." << std::endl;

    while (m_running) {
        if (m_input_queue.pop(audio_segment)) { // Bloqueia até ter um item
            if (!m_running) break;

            // vad_only: o consumidor quer o ÁUDIO (gameplay por voz), não texto.
            if (m_options.vad_only) {
                if (m_on_segment) m_on_segment(audio_segment, "", 0);
                continue;
            }

            auto t0 = std::chrono::steady_clock::now();
            std::string transcription = _process_transcription(audio_segment);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();

            if (!transcription.empty()) {
                m_output_queue.push(transcription);
            }
            // Mesmo com texto vazio: o A/B quer o áudio de todo segmento.
            if (m_on_segment) {
                m_on_segment(audio_segment, transcription, ms);
            }
        }
    }
    std::cout << "🛑 Whisper worker thread encerrando." << std::endl;
}

/**
 * @brief Pauses the VoicePipeline.
 * 
 * Stops processing of audio data (both VAD and callback).
 */
void VoicePipeline::pause() {
    std::cout << "[VAD] Pausado." << std::endl;
    m_is_paused = true;
}

/**
 * @brief Resumes the VoicePipeline.
 * 
 * Continues processing after a pause, clears any buffered audio data.
 */
void VoicePipeline::resume() {
    std::cout << "[VAD] Retomado." << std::endl;
    // Não mexe no write_pos: o last_read_pos da thread de VAD ficaria stale
    // e o primeiro poll pós-resume leria o buffer circular INTEIRO (até 30s
    // de áudio velho, incluindo a própria voz do TTS) como um chunk só.
    // Em vez disso a thread de VAD re-sincroniza o cursor dela.
    m_sync_read_pos = true;
    m_is_paused = false;
}

/**
 * @brief VAD loop thread function.
 * 
 * Detects speech segments using VAD and enqueues them for processing.
 */
void VoicePipeline::_vad_loop_func() {
    size_t last_read_pos = 0;
    std::vector<float> speech_buffer;
    // Pré-rolo: últimos vad_preroll_ms de áudio ANTES do VAD disparar — vão
    // prepended ao segmento, senão a primeira sílaba chega cortada ao Whisper.
    std::vector<float> preroll;
    const size_t preroll_max = (size_t)(m_options.vad_preroll_ms / 1000.0 * SAMPLE_RATE);

    auto last_speech_time = std::chrono::steady_clock::now();
    const auto silence_fast = std::chrono::milliseconds(m_options.vad_silence_ms);
    const auto silence_extended = std::chrono::milliseconds(
        std::max(m_options.vad_silence_extended_ms, m_options.vad_silence_ms));

    // Medidor de nível: uma linha a cada ~5s dizendo se o mic entrega áudio
    // e se o VAD está vendo fala. Diagnóstico do "não tá me escutando" —
    // se o rms fica ~0.000x, o problema é dispositivo/nível, não o VAD.
    auto last_meter_time = std::chrono::steady_clock::now();
    double meter_peak = 0.0;

    // Endpoint adaptativo: o probe é consultado UMA vez por episódio de
    // silêncio; se disser "incompleto", a janela alarga para silence_extended.
    bool probe_asked = false;
    bool probe_says_incomplete = false;

    std::cout << "🎤 VAD loop thread iniciada." << std::endl;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Poll 20x/s

        if (m_is_paused) {
            continue;
        }

        // Pós-resume: pula direto pro presente e descarta o que ficou no
        // buffer de antes/durante a pausa (inclusive um segmento de fala
        // que tenha ficado pela metade).
        if (m_sync_read_pos.exchange(false)) {
            last_read_pos = m_audio_data.write_pos;
            speech_buffer.clear();
            preroll.clear();
            if (m_vad_ctx) whisper_vad_reset_state(m_vad_ctx);
            continue;
        }

        size_t current_write_pos = m_audio_data.write_pos;
        if (current_write_pos == last_read_pos) {
            continue; // Nada de novo
        }

        std::vector<float> chunk;
        if (current_write_pos > last_read_pos) {
            chunk.assign(m_audio_data.buffer.begin() + last_read_pos, m_audio_data.buffer.begin() + current_write_pos);
        } else { // Wrap-around
            chunk.resize(m_buffer_size_samples - last_read_pos + current_write_pos);
            std::copy(m_audio_data.buffer.begin() + last_read_pos, m_audio_data.buffer.end(), chunk.begin());
            std::copy(m_audio_data.buffer.begin(), m_audio_data.buffer.begin() + current_write_pos, chunk.begin() + (m_buffer_size_samples - last_read_pos));
        }
        last_read_pos = current_write_pos;

        // Atualiza o medidor (pico do chunk) e loga a cada ~5s
        for (float s : chunk) meter_peak = std::max(meter_peak, (double)std::fabs(s));
        if (std::chrono::steady_clock::now() - last_meter_time > std::chrono::seconds(5)) {
            std::cout << "[Audio] nível: pico " << std::fixed << std::setprecision(3)
                      << meter_peak << (speech_buffer.empty() ? "" : " (capturando fala)")
                      << std::endl;
            meter_peak = 0.0;
            last_meter_time = std::chrono::steady_clock::now();
        }

        // Lógica VAD
        if (_is_speech(chunk)) {
            // Início de fala: avisa o scheduler ANTES da transcrição existir —
            // o load JIT do Whisper corre em paralelo com o usuário falando.
            if (speech_buffer.empty()) {
                if (m_on_speech_start) {
                    m_on_speech_start();
                }
                speech_buffer = std::move(preroll);
                preroll.clear();
            }
            speech_buffer.insert(speech_buffer.end(), chunk.begin(), chunk.end());
            last_speech_time = std::chrono::steady_clock::now();
            probe_asked = false;
            probe_says_incomplete = false;
        } else if (!speech_buffer.empty()) {
            auto silence = std::chrono::steady_clock::now() - last_speech_time;

            auto required = silence_fast;
            if (m_endpoint_probe) {
                if (!probe_asked && silence >= silence_fast) {
                    probe_says_incomplete = !m_endpoint_probe(speech_buffer);
                    probe_asked = true;
                }
                if (probe_says_incomplete) {
                    required = silence_extended;
                }
            }

            if (silence > required) {
                if (speech_buffer.size() > m_vad_min_samples) {
                    std::cout << "[VAD] Segmento de " << (speech_buffer.size() / (float)SAMPLE_RATE) << "s detectado. Enviando...\n";
                    m_input_queue.push(speech_buffer);
                }
                // Segmentos curtos demais são falso-gatilho: descarta em vez
                // de deixar grudar no próximo turno.
                speech_buffer.clear();
                probe_asked = false;
                probe_says_incomplete = false;
                if (m_vad_ctx) {
                    whisper_vad_reset_state(m_vad_ctx); // novo enunciado, LSTM zerado
                }
            }
        } else {
            // Fora de fala: mantém o pré-rolo deslizante.
            preroll.insert(preroll.end(), chunk.begin(), chunk.end());
            if (preroll.size() > preroll_max) {
                preroll.erase(preroll.begin(), preroll.end() - preroll_max);
            }
        }
    }
    std::cout << "🛑 VAD loop thread encerrando." << std::endl;
}

// --- Funções de Implementação ---

/**
 * @brief Processes audio buffer for transcription.
 * 
 * Transcribes the given audio samples using the Whisper model and returns the text result.
 */
std::string VoicePipeline::_process_transcription(const std::vector<float>& audio_buffer) {
    if (audio_buffer.empty()) return "";

    // Segura o modelo durante toda a transcrição: o scheduler não consegue
    // descarregar no meio (unload_model espera este lock).
    std::lock_guard<std::mutex> model_lock(m_model_mtx);

    // Fallback JIT: se o scheduler ainda não carregou (ou já descarregou),
    // carrega aqui mesmo — latência maior neste turno, mas nunca perde fala.
    if (!m_ctx) {
        std::cout << "[Whisper] Modelo fora da VRAM no momento da transcrição "
                     "— load JIT síncrono (fallback)." << std::endl;
        struct whisper_context_params cparams = whisper_context_default_params();
        m_ctx = whisper_init_from_file_with_params(m_model_path.c_str(), cparams);
        if (!m_ctx) {
            std::cerr << "[Whisper] ERRO: fallback de load falhou." << std::endl;
            return "";
        }
    }

    // Beam search: mais tolerante a sotaque que greedy (mantém hipóteses
    // concorrentes até o fim do segmento). Custo extra é pequeno com VAD
    // limitando os segmentos a fala real.
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    params.beam_search.beam_size = m_options.beam_size;
    params.greedy.best_of        = m_options.beam_size; // usado nos fallbacks de temperatura
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    params.n_threads        = m_options.n_threads;
    params.language         = m_options.language.c_str();
    // Cada segmento do VAD é um enunciado independente: sem no_context o
    // transcript anterior vira prompt do próximo e uma alucinação se
    // auto-alimenta por vários turnos. Também garante que o initial_prompt
    // (sotaque) não seja diluído pelo histórico.
    params.no_context       = true;
    if (!m_options.initial_prompt.empty()) {
        params.initial_prompt = m_options.initial_prompt.c_str();
    }

    // std::cout << "\n[PROCESSANDO " << (audio_buffer.size() / (float)SAMPLE_RATE) << "s...]\n";

    if (whisper_full(m_ctx, params, audio_buffer.data(), audio_buffer.size()) != 0) {
        std::cerr << "ERRO: Falha na execução da transcrição (whisper_full)." << std::endl;
        return "";
    }

    std::string full_text = "";
    const int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; i++) {
        const char * text = whisper_full_get_segment_text(m_ctx, i);
        full_text += text;
    }
    
    full_text.erase(0, full_text.find_first_not_of(" \t\n\r\f\v"));
    full_text.erase(full_text.find_last_not_of(" \t\n\r\f\v") + 1);

    // Alucinações clássicas do Whisper em segmentos de ruído/quase-silêncio
    // (vêm do treino em legendas de vídeo). Se a transcrição INTEIRA é uma
    // dessas, é lixo — melhor turno perdido que a Alyssa respondendo a
    // "não se esqueça de se inscrever no canal".
    static const char* kHallucinations[] = {
        "legendas pela comunidade amara.org",
        "legendas pela comunidade",
        "amara.org",
        "obrigado por assistir",
        "obrigada por assistir",
        "não se esqueça de se inscrever",
        "inscreva-se no canal",
        "curta e compartilhe",
        "tchau, tchau!",
        "até o próximo vídeo",
    };
    std::string lowered = full_text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    for (const char* h : kHallucinations) {
        if (lowered == h) {
            std::cout << "[Whisper] Alucinação conhecida descartada: \""
                      << full_text << "\"" << std::endl;
            return "";
        }
    }

    return full_text;
}

/**
 * @brief Loads the Silero VAD model (CPU-only).
 *
 * Roda fora do orçamento do VRAMResourceManager de propósito: o modelo tem
 * ~2MB e em CPU custa ~1ms por chunk — não vale um resident.
 */
bool VoicePipeline::_load_vad_model() {
    if (m_options.vad_model_path.empty()) return false;

    struct whisper_vad_context_params vparams = whisper_vad_default_context_params();
    vparams.use_gpu = false;
    m_vad_ctx = whisper_vad_init_from_file_with_params(m_options.vad_model_path.c_str(), vparams);
    if (!m_vad_ctx) {
        std::cerr << "[VAD] Silero não carregou de '" << m_options.vad_model_path
                  << "' (baixe ggml-silero-v5.1.2.bin para models/)." << std::endl;
        return false;
    }
    std::cout << "[VAD] Silero VAD carregado (CPU, streaming)." << std::endl;
    return true;
}

/**
 * @brief Performs Voice Activity Detection.
 *
 * Silero (probabilidade por janela de 512 samples, estado LSTM preservado
 * entre chunks) quando carregado; senão o limiar RMS antigo.
 */
bool VoicePipeline::_is_speech(const std::vector<float>& audio_chunk) {
    if (audio_chunk.empty()) return false;

    if (m_vad_ctx) {
        if (whisper_vad_detect_speech_no_reset(m_vad_ctx, audio_chunk.data(),
                                               (int)audio_chunk.size())) {
            const int    n_probs = whisper_vad_n_probs(m_vad_ctx);
            const float* probs   = whisper_vad_probs(m_vad_ctx);
            float max_p = 0.0f;
            for (int i = 0; i < n_probs; ++i) {
                max_p = std::max(max_p, probs[i]);
            }
            return max_p >= m_options.vad_threshold;
        }
        // inferência do Silero falhou neste chunk — decide pelo RMS abaixo
    }

    double sum_sq = std::inner_product(audio_chunk.begin(), audio_chunk.end(), audio_chunk.begin(), 0.0);
    double rms = std::sqrt(sum_sq / audio_chunk.size());
    return rms > m_options.vad_rms_threshold;
}

/**
 * @brief PortAudio callback function.
 * 
 * Static wrapper around the non-static `_pa_callback_impl` method.
 */
int VoicePipeline::_pa_callback(const void *inputBuffer, void *outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData)
{
    VoicePipeline* pipeline = static_cast<VoicePipeline*>(userData);
    
    return pipeline->_pa_callback_impl(inputBuffer, framesPerBuffer);
}

/**
 * @brief PortAudio callback implementation.
 * 
 * Captures audio samples from the microphone and writes them to the internal buffer.
 */
int VoicePipeline::_pa_callback_impl(const void* input, unsigned long frameCount) {
    const int16_t *input_i16 = (const int16_t*)input;

    if (!m_audio_data.stream_ready || m_is_paused) {
        return paContinue;
    }

    for (unsigned long i = 0; i < frameCount; ++i) {
        float sample = (float)input_i16[i] / 32768.0f; 
        
        m_audio_data.buffer[m_audio_data.write_pos] = sample;
        m_audio_data.write_pos = (m_audio_data.write_pos + 1) % m_buffer_size_samples;
    }
    return paContinue;
}
