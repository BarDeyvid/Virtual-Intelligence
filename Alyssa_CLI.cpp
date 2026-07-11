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
#include <ctime>
#include "EndocrineSystem.hpp"

#include "AlyssaNet.hpp"
#include "ProactivityEngine.hpp"
#include "llama.h"
#include "includes/log.hpp"
#include "minecraft/MinecraftSession.hpp"
#include "TerminalSafety.hpp"

// wingdi.h (via OpenCV) define RGB(r,g,b) como macro e quebra ftxui::Color::RGB
#ifdef RGB
#undef RGB
#endif

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
    cv::Mat presence_preview;        ///< Último frame RGB da checagem de presença (aba Vision)
    std::string presence_status;     ///< Linha de status da última checagem de presença
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

std::string now_hms() {
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return buf;
}

/**
 * @brief Desenha o preview RGB da webcam no terminal (aba Vision).
 *
 * Meia-célula por pixel: cada '▀' usa foreground = pixel de cima e
 * background = pixel de baixo, dobrando a resolução vertical. Como a célula
 * do terminal é ~1:2 (largura:altura), o pixel resultante fica ~quadrado.
 */
Element RenderWebcamPreview(const cv::Mat& img) {
    Elements rows;
    for (int y = 0; y + 1 < img.rows; y += 2) {
        Elements row;
        row.reserve(img.cols);
        for (int x = 0; x < img.cols; ++x) {
            const auto& top = img.at<cv::Vec3b>(y, x);
            const auto& bot = img.at<cv::Vec3b>(y + 1, x);
            row.push_back(text("▀")
                | color(Color::RGB(top[0], top[1], top[2]))
                | bgcolor(Color::RGB(bot[0], bot[1], bot[2])));
        }
        rows.push_back(hbox(std::move(row)));
    }
    return vbox(std::move(rows));
}

int main() {
    // Must run before ScreenInteractive touches the terminal (alt-screen,
    // hidden cursor): if any thread later throws uncaught, this makes sure
    // the terminal gets restored before the process dies instead of leaving
    // raw escape sequences bled into the console.
    alyssa_safety::install_crash_guard();

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

    // Minecraft (Fase 6, PoC): gameplayModel roda em loop próprio; "/mc start"
    // e "/mc stop" no chat ligam/desligam. Não existe até o primeiro start.
    std::unique_ptr<alyssa_minecraft::MinecraftSession> minecraft_session;

    std::string input_buffer;
    Component input_box = Input(&input_buffer, " Escreva para Alyssa...");

    auto on_enter = [&]() {
        if (input_buffer.empty() || g_state.is_processing) return;
        std::string msg = input_buffer;
        input_buffer = "";

        if (msg == "/mc start" || msg == "/mc stop") {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            if (msg == "/mc start") {
                if (minecraft_session && minecraft_session->is_running()) {
                    g_state.system_logs.push_back("[Minecraft] Já está rodando.");
                } else {
                    minecraft_session = std::make_unique<alyssa_minecraft::MinecraftSession>(
                        alyssa_brain, g_state.brain_mtx);
                    bool ok = minecraft_session->start();
                    g_state.system_logs.push_back(ok
                        ? "[Minecraft] Loop iniciado."
                        : "[Minecraft] Falha ao conectar no sidecar (rode `node index.js` em minecraft-bridge/).");
                    if (!ok) minecraft_session.reset();
                }
            } else {
                if (minecraft_session) {
                    minecraft_session->stop();
                    minecraft_session.reset();
                }
                g_state.system_logs.push_back("[Minecraft] Loop parado.");
            }
            return;
        }

        proactivity.note_user_activity();
        {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.is_processing = true;
            g_state.chat_history.push_back({"You", msg});
        }

        std::thread([&, msg]() {
            // Detached thread: an uncaught exception here calls std::terminate()
            // immediately, with no chance for FTXUI to restore the terminal.
            try {
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
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                g_state.system_logs.push_back(std::string("[Chat] Falha ao processar mensagem: ") + e.what());
                g_state.is_processing = false;
                screen.PostEvent(Event::Custom);
            }
        }).detach();
    };

    // ========================================================================
    // Vision preview (aba 👁): guarda o que a Alyssa viu na última checagem
    // ========================================================================
    auto store_presence_result = [&](const alyssa_vision::PresenceResult& presence) {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        if (!presence.available) {
            g_state.presence_status = "Falha na checagem às " + now_hms() + ": " + presence.error;
            return;
        }
        g_state.presence_preview = presence.preview;
        g_state.presence_status = (presence.present
            ? "Te vejo! " + std::to_string(presence.face_count) + " rosto(s) detectado(s)"
            : "Ninguém à vista")
            + " · " + now_hms()
            + " · brilho " + std::to_string(static_cast<int>(presence.brightness)) + "/255";
    };

    // Dispara um gatilho de proatividade agora, se houver um pendente e o
    // cérebro estiver livre. Usado tanto pelo poll periódico (check_interval_s,
    // gatilhos de humor) quanto pelo reconhecimento instantâneo de presença
    // (on_stream_frame) — responsividade em primeiro lugar: o welcome-back
    // não espera o próximo tick do poll.
    auto fire_proactive = [&]() {
        if (g_state.is_processing) return;

        auto profile = alyssa_brain.get_endocrine_system()->get_hormone_profile();
        auto trigger = proactivity.check(profile);
        if (trigger.type == alyssa_proactivity::TriggerType::None) return;

        std::string msg;
        {
            std::unique_lock<std::mutex> brain_lock(g_state.brain_mtx, std::try_to_lock);
            if (!brain_lock.owns_lock()) return; // cérebro ocupado, tenta na próxima chance

            {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                if (g_state.is_processing) return; // corrida: usuário chegou primeiro
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
    };

    // Streaming ao vivo always-on: cada frame publicado pelo stream cai aqui
    // (thread do stream). Atualiza a aba Vision a cada frame e alimenta a
    // proatividade (throttled a 1x/s). O welcome-back dispara na hora, direto
    // daqui, em vez de esperar o próximo tick do poll de check_interval_s.
    auto on_stream_frame = [&]() {
        auto presence = alyssa_brain.get_presence_detector()->latest();
        store_presence_result(presence);
        screen.PostEvent(Event::Custom);

        if (!proactivity.config().presence_detection || !presence.available) return;

        static auto last_feed = std::chrono::steady_clock::time_point{};
        auto now = std::chrono::steady_clock::now();
        if (now - last_feed < std::chrono::seconds(1)) return;
        last_feed = now;

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
        // Chama sempre que presente (não só na transição): user_present()
        // retorna true por padrão antes da 1ª leitura, então o "primeiro
        // avistamento" nunca bateria como transição ausente->presente.
        // check() é barato quando não há nada pendente — só gera/fala quando
        // um gatilho real (welcome-back ou 1º avistamento) está armado.
        if (presence.present) fire_proactive();
    };

    std::vector<std::string> tab_values = {" 💬 Chat ", " 🧠 Endocrine ", " 👁 Vision ", " 📝 Logs ", " 🛠 Tools "};
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
            // Aba Vision: o frame exato da última checagem de presença
            Elements v;
            std::lock_guard<std::mutex> lock(g_state.mtx);
            auto* detector = alyssa_brain.get_presence_detector();
            if (!detector || !detector->is_ready()) {
                v.push_back(text(" Detecção de presença indisponível (webcam ou cascade ausente).") | dim);
            } else {
                const bool live = detector->is_streaming();
                if (g_state.presence_preview.empty()) {
                    v.push_back(text(live ? " Ligando a webcam..." : " Iniciando o stream...") | dim);
                } else if (live) {
                    v.push_back(hbox({
                        text(" ● AO VIVO ") | bold | color(Color::Red),
                        text(g_state.presence_status) | bold | color(Color::Cyan),
                    }));
                } else {
                    v.push_back(text(" " + g_state.presence_status) | bold | color(Color::Cyan));
                }
                v.push_back(text(" A webcam fica ligada só enquanto esta aba está aberta (720p, ~30 FPS)") | dim);
                if (!g_state.presence_preview.empty()) {
                    v.push_back(separator() | dim);
                    v.push_back(RenderWebcamPreview(g_state.presence_preview) | hcenter);
                }
            }
            content = vbox(std::move(v)) | vscroll_indicator | frame | flex;
        }
        else if (g_state.tab_selected == 3) {
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
        int elapsed = 0;

        int hormone_sample_elapsed = 0;

        while (proactivity_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!proactivity_running) break;

            // A single bad iteration must not reach std::terminate() — this
            // thread is detached-equivalent by the time main() cleans it up
            // (joined, but only after screen.Loop() returns), so an uncaught
            // exception here would kill the process before that ever happens.
            try {
                // Streaming da webcam always-on: fica ligado o tempo todo, não só
                // com a aba Vision aberta. Se o throttle de VRAM (LISTENING/
                // SPEAKING) ou uma falha momentânea derrubou o stream, este tick
                // religa sozinho.
                if (auto* det = alyssa_brain.get_presence_detector(); det && det->is_ready()) {
                    if (!det->is_streaming()) det->start_stream(on_stream_frame);
                }

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

                // Presença via webcam agora é alimentada em tempo real pelo
                // on_stream_frame (throttled a 1x/s) — não precisa mais de um
                // poll separado aqui.

                if (++elapsed < interval) continue;
                elapsed = 0;

                fire_proactive(); // gatilhos de humor (tédio/estresse/empolgação); presença já é reativa
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                g_state.system_logs.push_back(std::string("[Proactivity] Falha no tick: ") + e.what());
            }
        }
    });

    try {
        screen.Loop(final_ui);
    } catch (const std::exception& e) {
        std::cerr << "[main] Exceção não tratada na UI, encerrando de forma controlada: "
                  << e.what() << std::endl;
    }

    proactivity_running = false;
    if (proactivity_thread.joinable()) proactivity_thread.join();

    // Se o usuário saiu com a aba Vision aberta, o stream ainda está rodando
    if (auto* det = alyssa_brain.get_presence_detector()) det->stop_stream();

    // Restaura os streams antes da destruição (os destrutores logam)
    std::cout.rdbuf(old_cout);
    std::cerr.rdbuf(old_cerr);
    return 0;
}
