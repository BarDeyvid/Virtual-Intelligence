#include "VoicePipeline.hpp"

// --- Construtor e Destrutor ---

/**
 * @brief Constructor implementation.
 * 
 * Loads the Whisper model and initializes PortAudio with the provided configuration options.
 */
VoicePipeline::VoicePipeline(const std::string& model_path, Options options)
    : m_options(options) {
        
    m_vad_min_samples = (size_t)(m_options.vad_min_duration_ms / 1000.0 * SAMPLE_RATE);
    
    // 1. Carregar Modelo Whisper
    std::cout << "Carregando modelo de: " << model_path << std::endl;
    struct whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (m_ctx == nullptr) {
        throw std::runtime_error("ERRO: Falha ao carregar o modelo Whisper.");
    }
    std::cout << "Modelo carregado com sucesso.\n" << std::endl;

    // 2. Inicializar PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        whisper_free(m_ctx);
        throw std::runtime_error("ERRO: Falha ao inicializar PortAudio.");
    }

    // 3. Preparar buffer de áudio
    m_audio_data.buffer.resize(m_buffer_size_samples);
    std::fill(m_audio_data.buffer.begin(), m_audio_data.buffer.end(), 0.0f);
}

/**
 * @brief Destructor implementation.
 * 
 * Stops the pipeline, frees resources, and terminates PortAudio.
 */
VoicePipeline::~VoicePipeline() {
    stop(); // Garante que tudo parou
    
    if (m_ctx) {
        whisper_free(m_ctx);
    }
    Pa_Terminate();
    std::cout << "VoicePipeline encerrada." << std::endl;
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

            std::string transcription = _process_transcription(audio_segment);
            
            if (!transcription.empty()) {
                m_output_queue.push(transcription);
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
    // Limpa qualquer áudio capturado durante a pausa
    if (m_running) {
        m_audio_data.write_pos = 0; 
    }
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
    auto last_speech_time = std::chrono::steady_clock::now();
    const auto silence_duration = std::chrono::milliseconds(m_options.vad_silence_ms);

    std::cout << "🎤 VAD loop thread iniciada." << std::endl;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Poll 10x/s

        if (m_is_paused) { 
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

        // Lógica VAD
        if (_is_speech(chunk)) {
            speech_buffer.insert(speech_buffer.end(), chunk.begin(), chunk.end());
            last_speech_time = std::chrono::steady_clock::now();
        } else if (!speech_buffer.empty()) {
            auto now = std::chrono::steady_clock::now();
            if ((now - last_speech_time > silence_duration) && (speech_buffer.size() > m_vad_min_samples)) {
                // Silêncio detectado, enviar o buffer
                std::cout << "[VAD] Segmento de " << (speech_buffer.size() / (float)SAMPLE_RATE) << "s detectado. Enviando...\n";
                m_input_queue.push(speech_buffer);
                speech_buffer.clear();
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

    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    params.n_threads        = m_options.n_threads;
    params.language         = m_options.language.c_str();

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

    return full_text;
}

/**
 * @brief Performs simple Voice Activity Detection.
 * 
 * Determines if the given audio chunk contains speech based on RMS threshold.
 */
bool VoicePipeline::_is_speech(const std::vector<float>& audio_chunk) {
    if (audio_chunk.empty()) return false;
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

    if (!m_audio_data.stream_ready) {
        return paContinue;
    }

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
