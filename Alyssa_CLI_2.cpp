#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "CoreLLM.hpp" 
#include "llama.h"
#include "includes/log.hpp"

using namespace ftxui;

struct AppState {
    std::string user_input;
    std::string alyssa_response;
    std::vector<std::pair<std::string, std::string>> chat_history;
    bool is_processing = false;
    std::mutex mtx;
};

void log_callback(ggml_log_level level, const char * text, void * user_data) {
    (void)level;
    (void)user_data;
    fputs(text, stderr);
    fflush(stderr);
}

int main() {
    try {      
        llama_log_set(log_callback, nullptr);
        ggml_backend_load_all(); 

        Log::init("alyssa_cli.log");
        auto& logger = Log::getLogger();

        CoreIntegration alyssa_brain;
        logger->debug("Inicializando CoreIntegration FTXUI...");
        alyssa_brain.set_user_name("Deyvid");
        
        if (!alyssa_brain.initialize("models/gemma-3-4b-it-q4_0.gguf")) {
            logger->critical("Falha Crítica ao inicializar o CoreIntegration.");
            return 1;
        }

        AppState state;
        auto screen = ScreenInteractive::Fullscreen();

        std::string input_buffer;
        Component input_box = Input(&input_buffer, "Digite sua mensagem...");

        auto on_enter = [&]() {
            if (input_buffer.empty() || state.is_processing) return;

            std::string user_msg = input_buffer;
            input_buffer = ""; 

            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.is_processing = true;
                state.chat_history.push_back({"You", user_msg});
            }

            std::thread([&, user_msg]() {
                try {
                    std::string response = alyssa_brain.think_with_fusion_ttsless(user_msg);
                    
                    std::lock_guard<std::mutex> lock(state.mtx);
                    state.chat_history.push_back({"Alyssa", response});
                    state.is_processing = false;
                    screen.PostEvent(Event::Custom); 
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(state.mtx);
                    state.chat_history.push_back({"Error", e.what()});
                    state.is_processing = false;
                    screen.PostEvent(Event::Custom);
                }
            }).detach();

            screen.PostEvent(Event::Custom);
        };

        // Correção no Renderer: usando a assinatura explícita
        auto renderer = Renderer(input_box, [&] {
            Elements chat_elements;
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                for (const auto& msg : state.chat_history) {
                    if (msg.first == "You") {
                        chat_elements.push_back(vbox({
                            hbox({ text(" 👤 You: ") | color(Color::Blue) | bold }),
                            paragraph("   " + msg.second) | color(Color::White)
                        }));
                    } else if (msg.first == "Alyssa") {
                        chat_elements.push_back(vbox({
                            hbox({ text(" ✨ Alyssa: ") | color(Color::Cyan) | bold }),
                            paragraph("   " + msg.second) | color(Color::Cyan)
                        }));
                    } else if (msg.first == "Error") {
                         chat_elements.push_back(vbox({
                            hbox({ text(" ❌ Error: ") | color(Color::Red) | bold }),
                            paragraph("   " + msg.second) | color(Color::Red)
                        }));
                    }
                    chat_elements.push_back(separator() | dim);
                }
                if (state.is_processing) {
                    chat_elements.push_back(hbox({ text(" ⏳ Alyssa está pensando...") | color(Color::Yellow) | dim }));
                }
            }

            return vbox({
                hbox({
                    text(" 🌌 ALYSSA AI ") | bold | color(Color::Cyan),
                    separator(),
                    text(" Terminal Interface ") | dim,
                }) | bgcolor(Color::Blue) | color(Color::Black), // Corrigido Border::Black para Color::Black
                separator(),
                vbox(std::move(chat_elements)) | flex | vscroll_indicator | frame | border,
                separator(),
                hbox({
                    text(" 💬 Input: ") | color(Color::White) | bold,
                    input_box->Render() | flex,
                }) | border,
                hbox({
                    text(" [Enter] Enviar  ") | dim,
                    text(" [Ctrl+C] Sair ") | dim,
                }) | center
            });
        });

        auto final_component = CatchEvent(renderer, [&](Event event) {
            if (event == Event::Return) {
                on_enter();
                return true;
            }
            return false;
        });

        screen.Loop(final_component);

    } catch (const std::exception& e) {
        std::cerr << "Erro Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}