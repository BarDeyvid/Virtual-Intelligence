// TTSBase.hpp — interface mínima que o AlyssaNet enxerga de um backend TTS.
//
// O streaming por sentença do run_expert() só precisa disto: recebe um
// pedaço de texto e fala. Quem implementa hoje: ElevenLabsTTS (API) e
// KokoroTTS (local/ONNX). O CoreIntegration não deve saber qual é qual.
#ifndef TTS_BASE_HPP
#define TTS_BASE_HPP

#include <string>

class ITTS {
public:
    virtual ~ITTS() = default;

    /// Sintetiza e reproduz o texto (bloqueante até o fim do playback).
    virtual void synthesizeAndPlay(const std::string& text) = 0;
};

#endif // TTS_BASE_HPP
