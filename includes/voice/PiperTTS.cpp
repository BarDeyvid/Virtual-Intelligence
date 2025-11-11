#include "includes/voice/PiperTTS.hpp"
#include <vector>


// --- Construtor e Destrutor ---

PiperTTS::PiperTTS(const std::string& model_path,
                   const std::string& config_path,
                   const std::string& espeak_data_path) {
    
    std::cout << "Inicializando PiperTTS..." << std::endl;

    // 1. Inicializar PortAudio
    initializePortAudio();

    // 2. Criar o Sintetizador Piper
    synth_ = piper_create(model_path.c_str(),
                          config_path.c_str(),
                          espeak_data_path.c_str());

    if (!synth_) {
        // Se o Piper falhar (ex: modelo ou config inválidos)
        terminatePortAudio();
        throw std::runtime_error("Falha ao criar o sintetizador Piper.");
    }

    // 3. Abrir o Stream de Áudio
    openAudioStream();

    std::cout << "PiperTTS inicializado com sucesso." << std::endl;
}

PiperTTS::~PiperTTS() {
    std::cout << "Finalizando PiperTTS..." << std::endl;
    
    // 1. Fechar o Stream de Áudio
    closeAudioStream();

    // 2. Liberar o Sintetizador Piper
    if (synth_) {
        piper_free(synth_);
        synth_ = nullptr;
    }

    // 3. Finalizar PortAudio
    terminatePortAudio();
}

// --- Métodos Privados de Gerenciamento ---

void PiperTTS::initializePortAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("Erro ao inicializar PortAudio: " + std::string(Pa_GetErrorText(err)));
    }
}

void PiperTTS::terminatePortAudio() {
    Pa_Terminate();
}

void PiperTTS::openAudioStream() {
    PaError err = Pa_OpenDefaultStream(&stream_,
                                       0, 
                                       CHANNELS, 
                                       PA_SAMPLE_TYPE, 
                                       SAMPLE_RATE, 
                                       FRAMES_PER_BUFFER, 
                                       NULL, 
                                       NULL);
    
    if (err != paNoError) {
        terminatePortAudio();
        throw std::runtime_error("Erro ao abrir o stream de áudio: " + std::string(Pa_GetErrorText(err)));
    }

    // Iniciar o stream imediatamente, ele pausará se não houver dados.
    // err = Pa_StartStream(stream_);
    if (err != paNoError) {
        Pa_CloseStream(stream_);
        terminatePortAudio();
        throw std::runtime_error("Erro ao iniciar o stream de áudio: " + std::string(Pa_GetErrorText(err)));
    }
}

void PiperTTS::closeAudioStream() {
    if (stream_) {
        // Pausar o stream antes de fechar
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

// --- Método Principal ---

void PiperTTS::synthesizeAndPlay(const std::string& text) {
    if (!synth_ || !stream_) {
        std::cerr << "Erro: PiperTTS não inicializado corretamente." << std::endl;
        return;
    }

    std::cout << "\n TTS: \"" << text << "\"" << std::endl;

    piper_synthesize_options options = piper_default_synthesize_options(synth_);

    // 1. Iniciar a síntese
    piper_synthesize_start(synth_, text.c_str(), &options);

    // 2. Criar um buffer para armazenar o áudio COMPLETO
    std::vector<float> audio_buffer;
    piper_audio_chunk chunk;

    // 3. Loop de síntese: PREENCHE o buffer na memória
    while (piper_synthesize_next(synth_, &chunk) != PIPER_DONE) {
        // Adiciona os samples do chunk ao nosso buffer principal
        audio_buffer.insert(audio_buffer.end(), 
                            chunk.samples, 
                            chunk.samples + chunk.num_samples);
    }
    
    // 4. Agora que temos o áudio completo, TOCA TUDO DE UMA VEZ
    if (audio_buffer.empty()) {
        std::cerr << "TTS: Nenhum áudio foi sintetizado." << std::endl;
        return; // Não faz nada se não houver áudio
    }
    
    // 5. Inicia o stream de áudio
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Erro ao iniciar stream: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    // 6. Escreve o buffer COMPLETO no stream
    err = Pa_WriteStream(stream_, audio_buffer.data(), audio_buffer.size()); 
    if (err != paNoError) {
        std::cerr << "Erro durante a escrita no stream de áudio: " << Pa_GetErrorText(err) << std::endl;
    }

    // 7. Para o stream (ele vai esperar o buffer terminar de tocar)
    err = Pa_StopStream(stream_);
    if (err != paNoError) {
        std::cerr << "Erro ao parar stream: " << Pa_GetErrorText(err) << std::endl;
    }
        
    std::cout << "Reprodução concluída." << std::endl;
}