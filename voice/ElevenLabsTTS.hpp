#ifndef ELEVENLABS_TTS_HPP
#define ELEVENLABS_TTS_HPP

#include <string>
#include <vector>
#include <portaudio.h>
#include <iostream>
#include <stdexcept>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/channel_layout.h> 
    #include <libswresample/swresample.h>
}

class ElevenLabsTTS {
public:
    // Construtor
    ElevenLabsTTS(const std::string& api_key = "",
                  const std::string& voice_id = "T3ZeSw265kJ0jRIeLTFw",
                  int sample_rate = 22050);
    
    // Destrutor
    ~ElevenLabsTTS();

    // Método principal para síntese e reprodução
    void synthesizeAndPlay(const std::string& text);

private:
    // Configuração
    std::string api_key_;
    std::string voice_id_;
    int sample_rate_;

    // PortAudio
    PaStream* stream_ = nullptr;
    
    // FFmpeg
    bool ffmpeg_initialized_ = false;
    
    // Constantes de áudio
    static const int CHANNELS = 1;
    static const PaSampleFormat PA_SAMPLE_TYPE = paFloat32;
    static const int FRAMES_PER_BUFFER = 256;

    // Métodos de gerenciamento
    void initializePortAudio();
    void terminatePortAudio();
    void openAudioStream();
    void closeAudioStream();
    void initializeFFmpeg();
    void cleanupFFmpeg();

    // Processamento de texto
    std::string cleanText(const std::string& text);

    // Geração e reprodução de áudio
    std::vector<float> generateAudio(const std::string& text);
    std::vector<float> decodeAudioWithFFmpeg(const std::vector<char>& audio_data);
    void playAudio(const std::vector<float>& audio_data);
};

#endif // ELEVENLABS_TTS_HPP