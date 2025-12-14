// ElevenLabsTTS.hpp
#ifndef ELEVENLABS_TTS_HPP
#define ELEVENLABS_TTS_HPP

#include <string>
#include <vector>
#include <portaudio.h>
#include <iostream>
#include <stdexcept>
#include <curl/curl.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include "json.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cstdio>
#include <functional>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/channel_layout.h> 
    #include <libswresample/swresample.h>
}

/**
 * @brief Class for text-to-speech synthesis using ElevenLabs API.
 */
class ElevenLabsTTS {
public:
    // Construtor
    /**
     * @brief Constructor for ElevenLabsTTS class.
     * @param api_key API key for ElevenLabs API.
     * @param voice_id Voice ID to use for TTS.
     * @param sample_rate Sample rate for the audio output.
     */
    ElevenLabsTTS(const std::string& api_key = "",
                  const std::string& voice_id = "T3ZeSw265kJ0jRIeLTFw",
                  int sample_rate = 22050);
    
    // Destrutor
    /**
     * @brief Destructor for ElevenLabsTTS class.
     */
    ~ElevenLabsTTS();

    // Método principal para síntese e reprodução
    /**
     * @brief Synthesizes and plays the given text using TTS.
     * @param text Text to be synthesized and played.
     */
    void synthesizeAndPlay(const std::string& text);

private:
    // Configuração
    /**
     * @brief API key for ElevenLabs API.
     */
    std::string api_key_;
    
    /**
     * @brief Voice ID to use for TTS.
     */
    std::string voice_id_;
    
    /**
     * @brief Sample rate for the audio output.
     */
    int sample_rate_;

    // PortAudio
    /**
     * @brief PortAudio stream pointer.
     */
    PaStream* stream_ = nullptr;
    
    // FFmpeg
    /**
     * @brief Flag indicating if FFmpeg is initialized.
     */
    bool ffmpeg_initialized_ = false;
    
    // Constantes de áudio
    static const int CHANNELS = 1; /**< Number of audio channels. */
    static const PaSampleFormat PA_SAMPLE_TYPE = paFloat32; /**< Sample type for PortAudio. */
    static const int FRAMES_PER_BUFFER = 256; /**< Frames per buffer for PortAudio stream. */

    // Métodos de gerenciamento
    /**
     * @brief Initializes PortAudio.
     */
    void initializePortAudio();
    
    /**
     * @brief Terminates PortAudio.
     */
    void terminatePortAudio();
    
    /**
     * @brief Opens the audio stream with PortAudio.
     */
    void openAudioStream();
    
    /**
     * @brief Closes the audio stream with PortAudio.
     */
    void closeAudioStream();
    
    /**
     * @brief Initializes FFmpeg.
     */
    void initializeFFmpeg();
    
    /**
     * @brief Cleans up FFmpeg resources.
     */
    void cleanupFFmpeg();

    // Processamento de texto
    /**
     * @brief Cleans the input text by removing unwanted characters and spaces.
     * @param text The input text to be cleaned.
     * @return Cleaned text as a string.
     */
    std::string cleanText(const std::string& text);

    // Geração e reprodução de áudio
    /**
     * @brief Generates audio from the given text using ElevenLabs API.
     * @param text The input text to be converted into audio.
     * @return Audio data as a vector of floats.
     */
    std::vector<float> generateAudio(const std::string& text);
    
    /**
     * @brief Decodes audio data with FFmpeg.
     * @param audio_data Raw audio data from the API response.
     * @return Decoded PCM audio data as a vector of floats.
     */
    std::vector<float> decodeAudioWithFFmpeg(const std::vector<char>& audio_data);
    
    /**
     * @brief Plays the given audio data using PortAudio.
     * @param audio_data Audio data to be played as a vector of floats.
     */
    void playAudio(const std::vector<float>& audio_data);
};

#endif // ELEVENLABS_TTS_HPP