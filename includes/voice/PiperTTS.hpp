#ifndef PIPER_TTS_HPP
#define PIPER_TTS_HPP

#include <iostream>
#include <string>
#include <stdexcept>

// Inclui as bibliotecas necessárias
#include <piper.h>
#include <portaudio.h>

class PiperTTS {
public:
    // Construtor: Inicializa PortAudio e o Sintetizador Piper
    PiperTTS(const std::string& model_path,
             const std::string& config_path,
             const std::string& espeak_data_path);
    
    // Destrutor: Libera o Sintetizador Piper e o PortAudio
    ~PiperTTS();

    // Método principal: Sintetiza o texto e reproduz o áudio
    void synthesizeAndPlay(const std::string& text);

private:
    piper_synthesizer* synth_ = nullptr; // O sintetizador Piper
    PaStream* stream_ = nullptr;        // O stream de áudio do PortAudio

    // Constantes de áudio
    static constexpr int SAMPLE_RATE = 22050;
    static constexpr int CHANNELS = 1;
    static constexpr PaSampleFormat PA_SAMPLE_TYPE = paFloat32;
    static constexpr int FRAMES_PER_BUFFER = 0; // Buffer padrão
    
    // Métodos privados para gerenciamento de recursos
    void initializePortAudio();
    void terminatePortAudio();
    void openAudioStream();
    void closeAudioStream();
};

#endif // PIPER_TTS_HPP