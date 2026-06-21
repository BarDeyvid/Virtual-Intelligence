#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include "EndocrineSystem.hpp"

#include "CoreLLM.hpp" 
#include "llama.h"
#include "includes/log.hpp"

using namespace ftxui;

struct EndocrineState {
    float cortisol = 0.0f;
    float dopamine = 0.0f;
    float oxytocin = 0.0f;
    float serotonin = 0.0f;
    float adrenaline = 0.0f;
    std::string current_state = "neutral";
};

struct AppState {
    std::vector<std::pair<std::string, std::string>> chat_history;
    std::vector<std::string> system_logs;
    EndocrineState hormones;
    bool is_processing = false;
    int tab_selected = 0;
    std::mutex mtx;
};

AppState g_state;

void ui_log_callback(ggml_log_level level, const char* text, void* user_data) {
    auto* screen = static_cast<ScreenInteractive*>(user_data);
    if (!text) return;
    std::string line(text);
    if (line.empty() || line == "\n") return;

    std::lock_guard<std::mutex> lock(g_state.mtx);
    g_state.system_logs.push_back(line);
    if (g_state.system_logs.size() > 500) g_state.system_logs.erase(g_state.system_logs.begin());

    screen->PostEvent(Event::Custom);
}

Element RenderHormone(std::string name, float val, Color col) {
    return hbox({
        text(" " + name) | size(WIDTH, EQUAL, 12),
        gauge(val) | color(col) | flex,
        text(" " + std::to_string(val).substr(0, 4)) | dim
    });
}

int main() {
    auto screen = ScreenInteractive::Fullscreen();
    llama_log_set(ui_log_callback, &screen);
    ggml_backend_load_all();

    CoreIntegration alyssa_brain;
    alyssa_brain.set_user_name("Deyvid");
    alyssa_brain.initialize("models/gemma-3-4b-it-q4_0.gguf");

    std::string input_buffer;
    Component input_box = Input(&input_buffer, " Escreva para Alyssa...");
    
    auto on_enter = [&]() {
        if (input_buffer.empty() || g_state.is_processing) return;
        std::string msg = input_buffer;
        input_buffer = "";
        {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.is_processing = true;
            g_state.chat_history.push_back({"You", msg});
        }

        std::thread([&, msg]() {
            std::string resp = alyssa_brain.think_with_fusion_ttsless(msg);
            auto profile = alyssa_brain.get_endocrine_system()->get_hormone_profile();

            {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                g_state.hormones.cortisol = (float)profile.cortisol;
                g_state.hormones.dopamine = (float)profile.dopamine;
                g_state.hormones.oxytocin = (float)profile.oxytocin;
                g_state.hormones.serotonin = (float)profile.serotonin;
                g_state.hormones.adrenaline = (float)profile.adrenaline;
                g_state.hormones.current_state = profile.get_emotional_state();
                
                g_state.chat_history.push_back({"Alyssa", resp});
                g_state.is_processing = false;
                screen.PostEvent(Event::Custom);
            }
        }).detach();
    };

    std::vector<std::string> tab_values = {" 💬 Chat ", " 🧠 Endocrine ", " 📝 Logs "};
    auto tab_menu = Menu(&tab_values, &g_state.tab_selected);

    auto main_container = Container::Vertical({
        tab_menu,
        input_box
    });

    auto renderer = Renderer(main_container, std::function<Element()>([&] {
        Element content;
        
        if (g_state.tab_selected == 0) { 
            Elements msgs;
            std::lock_guard<std::mutex> lock(g_state.mtx);
            for (auto& m : g_state.chat_history) {
                bool is_alyssa = m.first == "Alyssa";
                msgs.push_back(vbox({
                    text(" " + m.first) | bold | color(is_alyssa ? Color::Cyan : Color::Blue),
                    paragraph(" " + m.second) | color(is_alyssa ? Color::CyanLight : Color::White),
                    separator() | dim
                }));
            }
            if (g_state.is_processing) msgs.push_back(text(" ⏳ Alyssa está pensando...") | dim | italic);
            content = vbox(std::move(msgs)) | vscroll_indicator | frame | flex;
        } 
        else if (g_state.tab_selected == 1) { 
            content = vbox({
                window(text(" System Hormones "), vbox({
                    text(" Current State: " + g_state.hormones.current_state) | bold | color(Color::Magenta),
                    separator(),
                    RenderHormone("Cortisol", g_state.hormones.cortisol, Color::Red),
                    RenderHormone("Dopamine", g_state.hormones.dopamine, Color::Yellow),
                    RenderHormone("Oxytocin", g_state.hormones.oxytocin, Color::Green),
                    RenderHormone("Serotonin", g_state.hormones.serotonin, Color::Cyan),
                    RenderHormone("Adrenaline", g_state.hormones.adrenaline, Color::Orange1),
                }))
            }) | flex;
        } 
        else { 
            Elements log_lines;
            std::lock_guard<std::mutex> lock(g_state.mtx);
            for (auto& l : g_state.system_logs) log_lines.push_back(text(l));
            content = vbox(std::move(log_lines)) | vscroll_indicator | frame | flex;
        }

        return vbox({
            hbox({
                text(" 🌌 ALYSSA AI ") | bold | color(Color::Black) | bgcolor(Color::Cyan),
                tab_menu->Render() | flex,
                filler(), 
                text(" v2.0-MoE ") | dim
            }) | border,
            content | flex,
            hbox({
                text(" ❯ ") | bold | color(Color::Cyan),
                input_box->Render() | flex,
            }) | borderRounded | color(g_state.is_processing ? Color::GrayDark : Color::Cyan)
        });
    }));

    auto final_ui = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Return) {
            on_enter();
            return true;
        }
        return false;
    });

    screen.Loop(final_ui);
    return 0;
}
