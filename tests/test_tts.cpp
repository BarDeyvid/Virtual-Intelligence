#include "voice/PiperTTS.hpp"
#include <iostream>

int main() {
    const std::string MODEL = "models/en_US-ljspeech-high.onnx";
    const std::string CONFIG = "models/en_US-ljspeech-high.onnx.json";
    const std::string ESPEAK_DATA = "piper/libpiper/build/espeak_ng-install/share/espeak-ng-data"; 
    
    try {
        // O objeto TTS é criado e inicializa todos os recursos (PortAudio, Piper)
        PiperTTS tts(MODEL, CONFIG, ESPEAK_DATA);
        
        // Chamada simples para reprodução
        tts.synthesizeAndPlay("Hello, my dear user. This is a local AI speaking to you through Piper TTS.");
        
        // Chamada posterior (reutilização)
        tts.synthesizeAndPlay("This demonstrates how easy it is to reuse the TTS object for multiple phrases.");

        tts.synthesizeAndPlay("Esse demonstra o sotaque do modelo"); // A.K.A fica zuado

    } catch (const std::runtime_error& e) {
        std::cerr << "ERRO FATAL: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}