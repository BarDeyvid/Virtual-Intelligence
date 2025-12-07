#include "voice/ElevenLabsTTS.hpp"
#include <iostream>

int main() {
    try {
        
        ElevenLabsTTS tts;
        
        std::string text_pt = "Olá! Eu sou a Alyssa, sua assistente virtual. (Ela pisca os olhos) Estou muito feliz em poder te ajudar hoje.";
        std::string text_en = "Hello! I am Alyssa, your virtual assistant. (She blinks her eyes) I am very happy to help you today.";
        
        std::cout << "--- Teste em Português ---" << std::endl;
        tts.synthesizeAndPlay(text_pt);
        
        std::cout << "\n--- Teste em Inglês ---" << std::endl;
        tts.synthesizeAndPlay(text_en);
        
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}