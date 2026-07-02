#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include "EndocrineSystem.hpp"

#include "AlyssaNet.hpp"
#include "ProactivityEngine.hpp"
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
    std::string personality_state;   ///< Linha "energia alta, de bom humor..." (Fase 5.3)
    std::vector<EndocrineState> hormone_history; ///< Amostras a cada 10s, cap 180 = 30min (Fase 5.1)
    bool is_processing = false;
    int tab_selected = 0;
    std::mutex mtx;          ///< Guards chat_history, system_logs, hormones, is_processing
    std::mutex brain_mtx;    ///< Serialises all CoreIntegration calls (prevents concurrent inference)
};

AppState g_state;

/**
 * @class UILogStreambuf
 * @brief Captura std::cout/std::cerr linha a linha para a aba Logs do FTXUI.
 *
 * Sem isso, os logs do CoreIntegration ([Tools], [MoE], [Fast Path]...) vazam
 * por baixo da UI fullscreen e corrompem o desenho. printf() não passa por
 * aqui (poucos usos, aceitável).
 *
 * O próprio FTXUI desenha a tela escrevendo em std::cout a partir da thread
 * da UI (main). Capturar essas escritas mata a renderização, então escritas
 * vindas da thread da UI são repassadas ao streambuf original (passthrough);
 * só as threads de trabalho (inferência, proatividade) são capturadas.
 * std::cerr não é usado pelo FTXUI, então é capturado sem passthrough.
 */
class UILogStreambuf : public std::streambuf {
public:
    UILogStreambuf(ftxui::ScreenInteractive* s, std::streambuf* passthrough)
        : screen(s), passthrough(passthrough), ui_thread(std::this_thread::get_id()) {}

protected:
    int overflow(int c) override {
        if (is_ui_thread()) {
            if (c == EOF) return traits_type::not_eof(c);
            return passthrough->sputc(traits_type::to_char_type(c));
        }
        std::lock_guard<std::mutex> lock(line_mtx);
        if (c == '\n') {
            flush_line();
        } else if (c != EOF) {
            line += static_cast<char>(c);
        }
        return c;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        if (is_ui_thread()) return passthrough->sputn(s, n);
        std::lock_guard<std::mutex> lock(line_mtx);
        for (std::streamsize i = 0; i < n; ++i) {
            if (s[i] == '\n') flush_line();
            else line += s[i];
        }
        return n;
    }

    int sync() override {
        if (is_ui_thread()) return passthrough->pubsync();
        return 0;
    }

private:
    bool is_ui_thread() const {
        return passthrough && std::this_thread::get_id() == ui_thread;
    }

    void flush_line() {
        if (!line.empty()) {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.system_logs.push_back(line);
            if (g_state.system_logs.size() > 500) {
                g_state.system_logs.erase(g_state.system_logs.begin());
            }
        }
        line.clear();
        if (screen) screen->PostEvent(ftxui::Event::Custom);
    }

    ftxui::ScreenInteractive* screen;
    std::streambuf* passthrough;   ///< Streambuf original do terminal (nullptr = captura tudo)
    std::thread::id ui_thread;     ///< Thread que roda screen.Loop() (main)
    std::string line;
    std::mutex line_mtx;
};

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

    // Redireciona cout/cerr para a aba Logs (restaurado antes de sair).
    // cout precisa de passthrough na thread da UI: o FTXUI desenha via std::cout.
    std::streambuf* old_cout = std::cout.rdbuf();
    std::streambuf* old_cerr = std::cerr.rdbuf();
    UILogStreambuf ui_cout_buf(&screen, old_cout);
    UILogStreambuf ui_cerr_buf(&screen, nullptr);
    std::cout.rdbuf(&ui_cout_buf);
    std::cerr.rdbuf(&ui_cerr_buf);

    CoreIntegration alyssa_brain;
    alyssa_brain.set_user_name("Deyvid");
    alyssa_brain.initialize("models/gemma-3-4b-it-q4_0.gguf");

    // Proactivity Engine (Fase 2.2): Alyssa puxa assunto sozinha
    alyssa_proactivity::ProactivityEngine proactivity(
        alyssa_proactivity::load_proactivity_config());

    std::string input_buffer;
    Component input_box = Input(&input_buffer, " Escreva para Alyssa...");
    
    auto on_enter = [&]() {
        if (input_buffer.empty() || g_state.is_processing) return;
        std::string msg = input_buffer;
        input_buffer = "";
        proactivity.note_user_activity();
        {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.is_processing = true;
            g_state.chat_history.push_back({"You", msg});
        }

        std::thread([&, msg]() {
            std::string resp;
            {
                // Serialise all CoreIntegration calls: a second Enter before the
                // first finishes will block here instead of corrupting shared state.
                std::lock_guard<std::mutex> brain_lock(g_state.brain_mtx);
                resp = alyssa_brain.think_with_fusion_ttsless(msg);
            }
            auto profile = alyssa_brain.get_endocrine_system()->get_hormone_profile();

            {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                g_state.hormones.cortisol = (float)profile.cortisol;
                g_state.hormones.dopamine = (float)profile.dopamine;
                g_state.hormones.oxytocin = (float)profile.oxytocin;
                g_state.hormones.serotonin = (float)profile.serotonin;
                g_state.hormones.adrenaline = (float)profile.adrenaline;
                g_state.hormones.current_state = profile.get_emotional_state();
                g_state.personality_state = alyssa_brain.get_current_personality_state();

                g_state.chat_history.push_back({"Alyssa", resp});
                g_state.is_processing = false;
                screen.PostEvent(Event::Custom);
            }
        }).detach();
    };

    std::vector<std::string> tab_values = {" 💬 Chat ", " 🧠 Endocrine ", " 📝 Logs ", " 🛠 Tools "};
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
            // Timeline (Fase 5.1): copia o histórico sob lock e desenha graphs
            std::vector<EndocrineState> history;
            {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                history = g_state.hormone_history;
            }

            auto make_graph = [&history](float EndocrineState::* field, Color col, std::string label) {
                std::vector<float> data;
                data.reserve(history.size());
                for (const auto& s : history) data.push_back(s.*field);

                return hbox({
                    text(" " + label) | size(WIDTH, EQUAL, 12) | color(col),
                    graph([data](int width, int height) {
                        std::vector<int> out(width, 0);
                        int n = static_cast<int>(data.size());
                        for (int i = 0; i < width; ++i) {
                            int idx = n - width + i;
                            if (idx >= 0 && idx < n) {
                                out[i] = static_cast<int>(data[idx] * (height - 1)) + 1;
                            }
                        }
                        return out;
                    }) | color(col) | flex,
                }) | size(HEIGHT, EQUAL, 4);
            };

            content = vbox({
                window(text(" System Hormones "), vbox({
                    text(" Current State: " + g_state.hormones.current_state) | bold | color(Color::Magenta),
                    text(" Personality: " + (g_state.personality_state.empty()
                                                 ? std::string("(aguardando primeiro turno)")
                                                 : g_state.personality_state)) | color(Color::CyanLight),
                    separator(),
                    RenderHormone("Cortisol", g_state.hormones.cortisol, Color::Red),
                    RenderHormone("Dopamine", g_state.hormones.dopamine, Color::Yellow),
                    RenderHormone("Oxytocin", g_state.hormones.oxytocin, Color::Green),
                    RenderHormone("Serotonin", g_state.hormones.serotonin, Color::Cyan),
                    RenderHormone("Adrenaline", g_state.hormones.adrenaline, Color::Orange1),
                })),
                window(text(" Timeline (últimos 30 min, amostra a cada 10s) "), vbox({
                    make_graph(&EndocrineState::cortisol,   Color::Red,     "Cortisol"),
                    make_graph(&EndocrineState::dopamine,   Color::Yellow,  "Dopamine"),
                    make_graph(&EndocrineState::oxytocin,   Color::Green,   "Oxytocin"),
                    make_graph(&EndocrineState::serotonin,  Color::Cyan,    "Serotonin"),
                    make_graph(&EndocrineState::adrenaline, Color::Orange1, "Adrenaline"),
                })) | flex,
            }) | flex;
        }
        else if (g_state.tab_selected == 2) {
            Elements log_lines;
            std::lock_guard<std::mutex> lock(g_state.mtx);
            for (auto& l : g_state.system_logs) log_lines.push_back(text(l));
            content = vbox(std::move(log_lines)) | vscroll_indicator | frame | flex;
        }
        else {
            // Aba Tools (Fase 5.2): log de chamadas, mais recente primeiro
            Elements tool_lines;
            auto* executor = alyssa_brain.get_tool_executor();
            if (!executor) {
                tool_lines.push_back(text(" Sistema de tools desativado.") | dim);
            } else {
                auto log = executor->get_call_log(); // cópia thread-safe
                if (log.empty()) {
                    tool_lines.push_back(text(" Nenhuma ferramenta chamada ainda.") | dim);
                }
                for (auto it = log.rbegin(); it != log.rend(); ++it) {
                    std::string args_str;
                    for (const auto& [k, v] : it->args) {
                        if (!args_str.empty()) args_str += ", ";
                        args_str += k + "=" + v;
                    }
                    tool_lines.push_back(hbox({
                        text(" " + it->timestamp + " ") | dim,
                        text(it->success ? " OK " : " ERR ") | bold
                            | color(Color::Black)
                            | bgcolor(it->success ? Color::Green : Color::Red),
                        text(" " + it->tool_name + "(" + args_str + ") ") | bold | color(Color::Cyan),
                        text(std::to_string(it->duration_ms) + "ms") | dim,
                    }));
                    tool_lines.push_back(paragraph("   → " + it->output_preview) | color(Color::GrayLight));
                    tool_lines.push_back(separator() | dim);
                }
            }
            content = vbox(std::move(tool_lines)) | vscroll_indicator | frame | flex;
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

    // =========================================================================
    // Proactivity thread (Fase 2.2)
    // =========================================================================
    // Dorme em passos de 1s (shutdown responsivo) e só faz o check completo a
    // cada check_interval_s. Usa try_lock no brain_mtx: se o cérebro estiver
    // ocupado com o usuário, a Alyssa simplesmente não interrompe.
    std::atomic<bool> proactivity_running{true};
    std::thread proactivity_thread([&]() {
        const int interval = proactivity.config().check_interval_s;
        const bool use_presence = proactivity.config().presence_detection &&
                                  alyssa_brain.get_presence_detector() != nullptr;
        const int presence_interval = proactivity.config().presence_check_interval_s;
        int elapsed = 0;
        int presence_elapsed = 0;

        int hormone_sample_elapsed = 0;

        while (proactivity_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!proactivity_running) break;

            // Amostragem da timeline de hormônios (Fase 5.1): a cada 10s
            if (++hormone_sample_elapsed >= 10) {
                hormone_sample_elapsed = 0;
                auto sample = alyssa_brain.get_endocrine_system()->get_hormone_profile();
                std::lock_guard<std::mutex> lock(g_state.mtx);
                EndocrineState s;
                s.cortisol   = (float)sample.cortisol;
                s.dopamine   = (float)sample.dopamine;
                s.oxytocin   = (float)sample.oxytocin;
                s.serotonin  = (float)sample.serotonin;
                s.adrenaline = (float)sample.adrenaline;
                g_state.hormone_history.push_back(s);
                if (g_state.hormone_history.size() > 180) {
                    g_state.hormone_history.erase(g_state.hormone_history.begin());
                }
            }

            // Checagem de presença via webcam (independente do check de gatilhos)
            if (use_presence && ++presence_elapsed >= presence_interval) {
                presence_elapsed = 0;
                auto presence = alyssa_brain.get_presence_detector()->check();
                if (presence.available) {
                    bool was_present = proactivity.user_present();
                    proactivity.note_presence(presence.present);
                    if (presence.present && !was_present) {
                        // Usuário voltou: oxitocina sobe (rever quem se gosta)
                        alyssa_brain.get_endocrine_system()->trigger_social_response(0.3);
                    }
                    {
                        std::lock_guard<std::mutex> lock(g_state.mtx);
                        g_state.system_logs.push_back(
                            std::string("[Presence] ") +
                            (presence.present ? "usuário presente (" + std::to_string(presence.face_count) + " rosto(s))"
                                              : "ninguém na frente do PC"));
                    }
                }
            }

            if (++elapsed < interval) continue;
            elapsed = 0;

            if (g_state.is_processing) continue;

            auto profile = alyssa_brain.get_endocrine_system()->get_hormone_profile();
            auto trigger = proactivity.check(profile);
            if (trigger.type == alyssa_proactivity::TriggerType::None) continue;

            std::string msg;
            {
                std::unique_lock<std::mutex> brain_lock(g_state.brain_mtx, std::try_to_lock);
                if (!brain_lock.owns_lock()) continue; // cérebro ocupado, tenta no próximo ciclo

                {
                    std::lock_guard<std::mutex> lock(g_state.mtx);
                    if (g_state.is_processing) continue; // corrida: usuário chegou primeiro
                    g_state.is_processing = true;
                    screen.PostEvent(Event::Custom);
                }

                msg = alyssa_brain.generate_proactive_message(trigger.reason);
            }
            proactivity.note_proactive_message();

            auto updated = alyssa_brain.get_endocrine_system()->get_hormone_profile();
            {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                g_state.hormones.cortisol = (float)updated.cortisol;
                g_state.hormones.dopamine = (float)updated.dopamine;
                g_state.hormones.oxytocin = (float)updated.oxytocin;
                g_state.hormones.serotonin = (float)updated.serotonin;
                g_state.hormones.adrenaline = (float)updated.adrenaline;
                g_state.hormones.current_state = updated.get_emotional_state();
                g_state.personality_state = alyssa_brain.get_current_personality_state();

                if (!msg.empty()) {
                    g_state.chat_history.push_back({"Alyssa", msg});
                }
                g_state.is_processing = false;
                screen.PostEvent(Event::Custom);
            }
        }
    });

    screen.Loop(final_ui);

    proactivity_running = false;
    if (proactivity_thread.joinable()) proactivity_thread.join();

    // Restaura os streams antes da destruição (os destrutores logam)
    std::cout.rdbuf(old_cout);
    std::cerr.rdbuf(old_cerr);
    return 0;
}
