// alyssad.cpp — cérebro da Alyssa headless atrás de um socket TCP local.
//
// NDJSON em 127.0.0.1 (protocolo em docs/alyssad-protocol.md): frontends
// (TUI Ink, web, o que vier) conectam aqui em vez de linkar o brain no
// próprio processo. Mesmo idioma do minecraft-bridge, papéis invertidos.
//
// --echo  : não carrega modelo nenhum — desenvolvimento de frontend sem
//           pagar 5GB de load por iteração.
// --voice : Kokoro TTS disponível (load lazy na primeira frase falada).
// --port N: porta TCP (default 8377, ou env ALYSSAD_PORT).

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define ALYSSAD_CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define ALYSSAD_CLOSESOCK close
#endif

#include "AlyssaNet.hpp"
#include "voice/KokoroTTS.hpp"
#include "voice/VoicePipeline.hpp"
#include "json.hpp"

#include <filesystem>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using json = nlohmann::json;

namespace {

constexpr int DEFAULT_PORT = 8377;

std::mutex g_send_mtx;

/// Uma linha NDJSON pro cliente. Falha de envio (cliente caiu no meio de um
/// turno) é ignorada de propósito: o turno termina e os eventos se perdem.
void send_line(socket_t sock, const json& j) {
    // error_handler replace: UTF-8 inválido vira U+FFFD em vez de exceção —
    // uma string malformada nunca pode derrubar um turno inteiro.
    std::string line = j.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
    std::lock_guard<std::mutex> lk(g_send_mtx);
#ifdef MSG_NOSIGNAL
    send(sock, line.c_str(), (int)line.size(), MSG_NOSIGNAL);
#else
    send(sock, line.c_str(), (int)line.size(), 0);
#endif
}

void send_event(socket_t sock, const std::string& event, const json& data) {
    send_line(sock, {{"type", "event"}, {"event", event}, {"data", data}});
}

void send_ok(socket_t sock, const json& id, const json& data = json::object()) {
    send_line(sock, {{"type", "res"}, {"id", id}, {"ok", true}, {"data", data}});
}

void send_err(socket_t sock, const json& id, const std::string& error) {
    send_line(sock, {{"type", "res"}, {"id", id}, {"ok", false}, {"error", error}});
}

json hormones_json(CoreIntegration& brain) {
    auto profile = brain.get_endocrine_system()->get_hormone_profile();
    return {
        {"cortisol", profile.cortisol},
        {"dopamine", profile.dopamine},
        {"oxytocin", profile.oxytocin},
        {"serotonin", profile.serotonin},
        {"adrenaline", profile.adrenaline},
        {"emotional_state", profile.get_emotional_state()},
    };
}

/**
 * @brief Tamanho do prefixo de `s` que termina em caractere UTF-8 completo.
 * @details Um token do llama.cpp pode cortar um caractere multibyte no meio
 *          ("ã" = 2 bytes pode vir metade em cada token). O streaming emite
 *          só o prefixo completo e carrega o resto pro próximo pedaço.
 */
size_t utf8_complete_prefix_len(const std::string& s) {
    const size_t n = s.size();
    for (size_t back = 1; back <= 3 && back <= n; ++back) {
        unsigned char c = (unsigned char)s[n - back];
        if ((c & 0xC0) == 0x80) continue; // continuation byte: segue voltando
        size_t need;
        if ((c & 0x80) == 0x00) need = 1;
        else if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else return n - back;             // byte inválido: descarta
        return (back >= need) ? n : n - back;
    }
    return n; // 3+ continuations no fim = sequência de 4 bytes completa
}

/// "[AMBIENTE] ...\n" → "..." (o cliente não precisa do envelope do prompt)
std::string ambient_line(CoreIntegration& brain) {
    std::string amb = brain.build_ambient_context();
    const std::string prefix = "[AMBIENTE] ";
    if (amb.rfind(prefix, 0) == 0) amb.erase(0, prefix.size());
    while (!amb.empty() && (amb.back() == '\n' || amb.back() == '\r')) amb.pop_back();
    return amb;
}

struct DaemonState {
    bool echo_mode = false;
    CoreIntegration* brain = nullptr;   // nullptr no modo echo
    ITTS* tts = nullptr;                // nullptr sem --voice
    std::atomic<bool> busy{false};
    std::atomic<bool> running{true};
    std::atomic<long long> last_activity{0};  // epoch do último say (gate de idle da consolidação)
    std::thread worker;                 // no máximo um turno em andamento
    std::mutex worker_mtx;              // v0.2: vários clientes despacham — join/assign serializado

    // v0.2 (F5.0): múltiplos clientes simultâneos. Turnos continuam
    // serializados pelo `busy`; eventos são BROADCAST pra todos (a TUI e o
    // celular veem a mesma conversa). `res` continua indo só pro requisitante.
    std::mutex clients_mtx;
    std::vector<socket_t> clients;
    std::string auth_token;             // env ALYSSAD_TOKEN; vazio = sem auth (loopback)
    socket_t listener = INVALID_SOCKET; // shutdown fecha pra destravar o accept

    // Voice-in (`listen`, v0.2): mic → VAD → Whisper → turno normal.
    // O modelo carrega LAZY no primeiro listen on e SAI da VRAM no off.
    std::unique_ptr<VoicePipeline> stt;
    std::atomic<bool> listening{false};
    std::thread listen_poll;            // thread única de polling dos transcripts
    std::mutex stt_mtx;                 // serializa liga/desliga

    void start_worker(std::function<void()> fn) {
        std::lock_guard<std::mutex> lk(worker_mtx);
        if (worker.joinable()) worker.join();
        worker = std::thread(std::move(fn));
    }

    void join_worker() {
        std::lock_guard<std::mutex> lk(worker_mtx);
        if (worker.joinable()) worker.join();
    }
};

/// Evento pra TODOS os clientes conectados (v0.2). Envio é best-effort:
/// cliente morto falha em silêncio e sai da lista no próprio recv loop.
void broadcast_event(DaemonState& st, const std::string& event, const json& data) {
    std::lock_guard<std::mutex> lk(st.clients_mtx);
    for (socket_t s : st.clients) send_event(s, event, data);
}

/// Hora local (0-23) e AAAAMMDD numa tacada (pro trigger da consolidação).
void local_clock(int& hour, int& yyyymmdd) {
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    hour = tm_buf.tm_hour;
    yyyymmdd = (tm_buf.tm_year + 1900) * 10000 + (tm_buf.tm_mon + 1) * 100 + tm_buf.tm_mday;
}

void start_say_turn(DaemonState& st, const std::string& text, bool want_tts);

void handle_say(DaemonState& st, socket_t sock, const json& id, const json& params) {
    std::string text = params.value("text", "");
    if (text.empty()) {
        send_err(sock, id, "params.text vazio ou ausente");
        return;
    }

    if (st.busy.exchange(true)) {
        send_err(sock, id, "busy");
        return;
    }
    st.last_activity = static_cast<long long>(std::time(nullptr));

    if (st.echo_mode) {
        send_ok(sock, id, {{"accepted", true}});
        // Mesma coreografia do modo real (worker + busy): o frontend consegue
        // testar o erro `busy` e a UI de streaming sem carregar modelo nenhum.
        st.start_worker([&st, text]() {
            broadcast_event(st, "state", {{"phase", "thinking"}});
            std::string full = "[eco] " + text;
            size_t start = 0;
            while (start < full.size()) {
                size_t sp = full.find(' ', start);
                size_t end = (sp == std::string::npos) ? full.size() : sp + 1;
                broadcast_event(st, "token", {{"text", full.substr(start, end - start)}});
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                start = end;
            }
            broadcast_event(st, "response", {{"text", full}, {"latency_ms", 0}, {"tts", false}});
            broadcast_event(st, "state", {{"phase", "idle"}});
            st.busy = false;
        });
        return;
    }

    bool want_tts = params.value("tts", st.tts != nullptr);
    send_ok(sock, id, {{"accepted", true}});

    // user_text: os OUTROS clientes veem o que este cliente falou (a TUI
    // mostra a conversa do celular e vice-versa). O originador se identifica
    // em params.client e ignora o próprio eco.
    broadcast_event(st, "user_text",
                    {{"text", text}, {"client", params.value("client", "cliente")}});

    start_say_turn(st, text, want_tts);
}

/// Corpo do turno (compartilhado entre `say` e o voice-in do `listen`).
/// Pré-condição: st.busy já está true (guard do chamador).
void start_say_turn(DaemonState& st, const std::string& text, bool want_tts) {
    st.start_worker([&st, text, want_tts]() {
        broadcast_event(st, "state", {{"phase", "thinking"}});

        // Sink de streaming: cada pedaço gerado vira um evento `token`
        // (broadcast: TUI e celular veem o mesmo streaming). Roda nesta
        // mesma thread (dentro do think_*) — turnos serializados pelo busy.
        // O carry segura bytes de um caractere UTF-8 cortado entre tokens.
        auto utf8_carry = std::make_shared<std::string>();
        st.brain->on_response_chunk = [&st, utf8_carry](const std::string& piece) {
            std::string out = *utf8_carry + piece;
            size_t complete = utf8_complete_prefix_len(out);
            *utf8_carry = out.substr(complete);
            out.resize(complete);
            if (!out.empty()) broadcast_event(st, "token", {{"text", out}});
        };

        auto t0 = std::chrono::steady_clock::now();
        try {
            std::string resp;
            bool used_tts = want_tts && st.tts != nullptr;
            if (used_tts) {
                resp = st.brain->think_with_fusion(text, *st.tts);
            } else {
                resp = st.brain->think_with_fusion_ttsless(text);
            }
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();

            broadcast_event(st, "response",
                            {{"text", resp}, {"latency_ms", ms}, {"tts", used_tts}});
            broadcast_event(st, "hormones", hormones_json(*st.brain));
        } catch (const std::exception& e) {
            broadcast_event(st, "error", {{"message", e.what()}});
        }
        st.brain->on_response_chunk = nullptr;
        broadcast_event(st, "state", {{"phase", "idle"}});
        st.busy = false;
    });
}

void handle_status(DaemonState& st, socket_t sock, const json& id) {
    json data = {
        {"echo", st.echo_mode},
        {"busy", st.busy.load()},
        {"voice_available", st.tts != nullptr},
    };
    if (st.brain) {
        data["hormones"] = hormones_json(*st.brain);
        data["emotional_state"] = data["hormones"]["emotional_state"];
        // Fonte de CPU tem estado (delta entre chamadas) e não é thread-safe
        // contra a inferência — só amostra com o cérebro ocioso.
        if (!st.busy.load()) data["ambient"] = ambient_line(*st.brain);
        // Self persistente (F2/F3): visão rápida pro frontend, com os itens
        // em si (a aba Self da TUI mostra opiniões/metas/agenda de verdade).
        const auto& self = st.brain->get_self_state();
        json opinions = json::array();
        for (const auto& o : self.opinions) {
            opinions.push_back({{"topic", o.topic}, {"stance", o.stance},
                                {"confidence", o.confidence}});
        }
        json goals = json::array();
        for (const auto& g : self.goals) {
            goals.push_back({{"desc", g.desc}, {"progress", g.progress},
                             {"priority", g.priority}});
        }
        json agenda = json::array();
        for (const auto& a : self.agenda) {
            agenda.push_back({{"bring_up", a.bring_up}, {"reason", a.reason}});
        }
        data["self"] = {
            {"opinions", opinions},
            {"goals", goals},
            {"agenda", agenda},
            {"yesterday_summary", self.yesterday_summary},
            {"last_consolidation_date", self.last_consolidation_date},
        };
        data["voice_in"] = st.listening.load();
    }
    send_ok(sock, id, data);
}

/// Voice-in (`listen`, v0.2): liga/desliga mic → VAD (Silero) → Whisper.
/// Transcript vira turno normal via start_say_turn; o modelo Whisper só
/// ocupa VRAM enquanto ligado (unload no off — mesmo espírito do gameplay).
void handle_listen(DaemonState& st, socket_t sock, const json& id, const json& params) {
    if (st.echo_mode || !st.brain) {
        send_err(sock, id, "sem cérebro (modo echo)");
        return;
    }
    const bool enable = params.value("enabled", true);
    std::lock_guard<std::mutex> lk(st.stt_mtx);

    if (enable == st.listening.load()) {
        send_ok(sock, id, {{"listening", st.listening.load()}});
        return;
    }

    if (enable) {
        const char* WHISPER_PATH = "models/ggml-large-v3-turbo-q8_0.bin";
        if (!std::filesystem::exists(WHISPER_PATH)) {
            send_err(sock, id, std::string("modelo de voz ausente: ") + WHISPER_PATH);
            return;
        }
        if (!st.stt) {
            VoicePipeline::Options opts; // defaults já são pt-BR + Silero + initial_prompt
            // Um tico mais conservador que o default: ruído marginal que o
            // Silero deixa passar é exatamente o que vira "Legenda por..."
            opts.vad_threshold = 0.6f;
            opts.vad_min_duration_ms = 300;
            st.stt = std::make_unique<VoicePipeline>(WHISPER_PATH, opts, /*defer=*/true);
        }
        if (!st.stt->model_loaded() && !st.stt->load_model()) {
            send_err(sock, id, "falha ao carregar o Whisper");
            return;
        }
        if (!st.stt->start()) {
            st.stt->unload_model();
            send_err(sock, id, "falha ao abrir o microfone");
            return;
        }
        st.listening = true;
        std::cout << "[alyssad] voice-in LIGADO (mic → Whisper)\n";
        broadcast_event(st, "listening", {{"enabled", true}});
        send_ok(sock, id, {{"listening", true}});
    } else {
        st.listening = false;
        if (st.stt) {
            st.stt->stop();
            st.stt->unload_model(); // devolve a VRAM do Whisper
        }
        std::cout << "[alyssad] voice-in desligado\n";
        broadcast_event(st, "listening", {{"enabled", false}});
        send_ok(sock, id, {{"listening", false}});
    }
}

/// Thread única de polling do voice-in: transcript pronto → evento `heard`
/// + turno normal (mesmo guard busy; ocupada = descarta com aviso).
void listen_poll_loop(DaemonState& st) {
    while (st.running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (!st.listening.load() || !st.stt) continue;

        std::string text;
        if (!st.stt->get_last_result(text)) continue;
        if (text.empty()) continue;

        broadcast_event(st, "heard", {{"text", text}});

        if (st.busy.exchange(true)) {
            broadcast_event(st, "error",
                            {{"message", "ouvi \"" + text + "\" mas estava no meio de um turno — repete?"}});
            continue;
        }
        st.last_activity = static_cast<long long>(std::time(nullptr));
        broadcast_event(st, "user_text", {{"text", text}, {"client", "voz"}});
        start_say_turn(st, text, /*want_tts=*/st.tts != nullptr);
    }
}

/// Consolidação manual (`consolidate`): mesmo guard `busy` dos turnos.
/// O trigger automático (04:00+ com 30min de idle) vive na thread scheduler.
void handle_consolidate(DaemonState& st, socket_t sock, const json& id) {
    if (st.echo_mode || !st.brain) {
        send_err(sock, id, "sem cérebro (modo echo)");
        return;
    }
    if (st.busy.exchange(true)) {
        send_err(sock, id, "busy");
        return;
    }
    send_ok(sock, id, {{"accepted", true}});

    st.start_worker([&st]() {
        broadcast_event(st, "state", {{"phase", "consolidating"}});
        try {
            json stats = st.brain->run_consolidation();
            broadcast_event(st, "consolidation", stats);
        } catch (const std::exception& e) {
            broadcast_event(st, "error", {{"message", e.what()}});
        }
        broadcast_event(st, "state", {{"phase", "idle"}});
        st.busy = false;
    });
}

void dispatch(DaemonState& st, socket_t sock, const std::string& raw, bool& authed) {
    json msg;
    try {
        msg = json::parse(raw);
    } catch (const std::exception&) {
        send_event(sock, "error", {{"message", "linha não é JSON válido"}});
        return;
    }

    json id = msg.value("id", json());
    std::string method = msg.value("method", "");
    json params = msg.value("params", json::object());

    // v0.2: com ALYSSAD_TOKEN setado, tudo além de ping/auth exige auth
    // (preparo pro bind no tailnet da F5 — no loopback puro é opcional).
    if (method == "auth") {
        if (st.auth_token.empty()) {
            // Sem token configurado: auth é no-op de sucesso (cliente pode
            // sempre mandar auth sem se importar com o modo do daemon).
            send_ok(sock, id, {{"authed", true}, {"required", false}});
        } else if (params.value("token", "") == st.auth_token) {
            authed = true;
            send_ok(sock, id, {{"authed", true}});
        } else {
            send_err(sock, id, "token inválido");
        }
        return;
    }
    if (!authed && method != "ping") {
        send_err(sock, id, "não autenticado (mande {method:\"auth\"} primeiro)");
        return;
    }

    if (method == "ping") {
        send_ok(sock, id, {{"pong", true}});
    } else if (method == "status") {
        handle_status(st, sock, id);
    } else if (method == "say") {
        handle_say(st, sock, id, params);
    } else if (method == "listen") {
        handle_listen(st, sock, id, params);
    } else if (method == "consolidate") {
        handle_consolidate(st, sock, id);
    } else if (method == "shutdown") {
        send_ok(sock, id);
        st.running = false;
        if (st.listener != INVALID_SOCKET) ALYSSAD_CLOSESOCK(st.listener); // destrava o accept
        // Fecha os sockets dos clientes: destrava os recv() das threads
        // (senão o join do encerramento espera todo mundo desconectar).
        std::lock_guard<std::mutex> lk(st.clients_mtx);
        for (socket_t s : st.clients) ALYSSAD_CLOSESOCK(s);
    } else {
        send_err(sock, id, "método desconhecido: " + method);
    }
}

/// Loop de leitura de UM cliente (v0.2: uma thread por cliente): registra no
/// broadcast, recv em blocos, quebra em linhas, despacha, desregistra.
void serve_client(DaemonState& st, socket_t sock) {
    {
        std::lock_guard<std::mutex> lk(st.clients_mtx);
        st.clients.push_back(sock);
    }
    bool authed = st.auth_token.empty(); // sem token configurado = liberado

    std::string buffer;
    char chunk[4096];

    while (st.running) {
        int n = recv(sock, chunk, sizeof(chunk), 0);
        if (n <= 0) break; // desconectou (ou erro)

        buffer.append(chunk, n);
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) dispatch(st, sock, line, authed);
            if (!st.running) break;
        }
    }

    {
        std::lock_guard<std::mutex> lk(st.clients_mtx);
        st.clients.erase(std::remove(st.clients.begin(), st.clients.end(), sock),
                         st.clients.end());
    }
    // NÃO espera o worker aqui (v0.2): outros clientes continuam recebendo
    // os eventos do turno em andamento via broadcast.
}

} // namespace

int main(int argc, char** argv) {
    bool echo_mode = false;
    bool voice = false;
    int port = DEFAULT_PORT;

    if (const char* env_port = std::getenv("ALYSSAD_PORT")) {
        port = std::atoi(env_port);
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--echo") echo_mode = true;
        else if (arg == "--voice") voice = true;
        else if (arg == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else {
            std::cout << "uso: alyssad [--echo] [--voice] [--port N]\n";
            return 1;
        }
    }

    std::cout << "[alyssad] iniciando" << (echo_mode ? " (modo echo, sem modelos)" : "")
              << (voice ? " com voz" : "") << "...\n";

    // Cérebro + TTS sobem ANTES do socket: cliente que conecta já fala com
    // um sistema pronto (o load de 5GB não acontece no meio de uma conexão).
    std::unique_ptr<CoreIntegration> brain;
    std::unique_ptr<KokoroTTS> tts;
    if (!echo_mode) {
        brain = std::make_unique<CoreIntegration>();
        brain->set_user_name("Deyvid");
        if (!brain->initialize("models/gemma-3-4b-it-q4_0.gguf")) {
            std::cerr << "[alyssad] falha ao inicializar o CoreIntegration\n";
            return 1;
        }
        if (voice) {
            // Mesmo blend do Alyssa_CLI_WITH_VOICE (audição de 2026-07-03).
            KokoroTTS::Config tts_cfg;
            tts_cfg.voice_blend = {{"models/kokoro/voices/pf_dora.bin", 0.5f},
                                   {"models/kokoro/voices/af_bella.bin", 0.5f}};

            // Guarda de assets (2026-07-20): sem os arquivos do Kokoro, a
            // síntese entra no espeak-ng com o data dir faltando e a lib C
            // DERRUBA o processo (abort não-capturável) na PRIMEIRA frase —
            // a Alyssa "morre" no meio do primeiro turno falado. Como o
            // load é lazy, o boot não acusa nada. Então checamos AQUI: se
            // faltar qualquer peça, roda texto-puro (voice_available:false)
            // em vez de prometer voz e cair. O celular nem usa este TTS
            // (fala com a voz do próprio Android), então nada se perde lá.
            const char* required[] = {
                tts_cfg.model_path.c_str(),        // models/kokoro/model.onnx
                tts_cfg.espeak_data_path.c_str(),  // models/kokoro/espeak-ng-data
                "models/kokoro/voices/pf_dora.bin",
                "models/kokoro/voices/af_bella.bin",
            };
            std::string missing;
            for (const char* p : required) {
                if (!std::filesystem::exists(p)) missing += std::string("\n  - ") + p;
            }
            if (missing.empty()) {
                tts = std::make_unique<KokoroTTS>(tts_cfg);
            } else {
                std::cerr << "[alyssad] --voice pedido, mas faltam assets do Kokoro:"
                          << missing << "\n[alyssad] rodando TEXTO-PURO (voz desativada; "
                          << "reponha models/kokoro/ pra falar).\n";
            }
        }
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[alyssad] WSAStartup falhou\n";
        return 1;
    }
#endif

    socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) {
        std::cerr << "[alyssad] socket() falhou\n";
        return 1;
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    // Loopback SEMPRE: o daemon nunca escuta a rede.
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[alyssad] bind falhou na porta " << port
                  << " (já tem um alyssad rodando?)\n";
        return 1;
    }
    if (listen(listener, 4) == SOCKET_ERROR) { // v0.2: TUI + celular + folga
        std::cerr << "[alyssad] listen falhou\n";
        return 1;
    }

    std::cout << "[alyssad] pronto — escutando em 127.0.0.1:" << port << "\n";

    DaemonState st;
    st.echo_mode = echo_mode;
    st.brain = brain.get();
    st.tts = tts.get();
    st.last_activity = static_cast<long long>(std::time(nullptr));
    st.listener = listener;
    if (const char* tok = std::getenv("ALYSSAD_TOKEN")) {
        st.auth_token = tok;
        std::cout << "[alyssad] auth por token LIGADA (ALYSSAD_TOKEN)\n";
    }

    // Thread de polling do voice-in (dorme enquanto listening=false)
    if (!echo_mode) {
        st.listen_poll = std::thread([&st]() { listen_poll_loop(st); });
    }

    // Scheduler da consolidação noturna (v2/F3): checa a cada minuto se
    // (a) hoje ainda não consolidou, (b) já passou das 04:00 e (c) tem 30min
    // de silêncio — aí "dorme e digere o dia" com o mesmo guard dos turnos.
    std::thread consolidation_scheduler;
    if (!echo_mode) {
        consolidation_scheduler = std::thread([&st]() {
            while (st.running) {
                for (int i = 0; i < 60 && st.running; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (!st.running || !st.brain || st.busy.load()) continue;

                int hour, today;
                local_clock(hour, today);
                if (hour < 4) continue; // madrugada cedo demais / dia anterior
                if (st.brain->get_self_state().last_consolidation_date == today) continue;

                long long idle_s =
                    static_cast<long long>(std::time(nullptr)) - st.last_activity.load();
                if (idle_s < 30 * 60) continue;

                if (st.busy.exchange(true)) continue; // turno chegou no meio: tenta no próximo minuto
                std::cout << "[alyssad] 04:00+ e " << idle_s / 60
                          << "min de silêncio — consolidando o dia...\n";
                try {
                    st.brain->run_consolidation();
                } catch (const std::exception& e) {
                    std::cerr << "[alyssad] consolidação falhou: " << e.what() << "\n";
                }
                st.busy = false;
            }
        });
    }

    // v0.2: uma thread por cliente — TUI e celular conectados ao mesmo tempo.
    std::vector<std::thread> client_threads;
    while (st.running) {
        socket_t client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!st.running) break;
            continue;
        }
        std::cout << "[alyssad] cliente conectado\n";
        client_threads.emplace_back([&st, client]() {
            serve_client(st, client);
            ALYSSAD_CLOSESOCK(client);
            std::cout << "[alyssad] cliente desconectado\n";
        });
    }

    for (auto& t : client_threads) {
        if (t.joinable()) t.join();
    }
    st.join_worker(); // turno em andamento termina antes de derrubar tudo
    st.listening = false;
    if (st.stt) st.stt->stop();
    if (st.listen_poll.joinable()) st.listen_poll.join();
    if (consolidation_scheduler.joinable()) consolidation_scheduler.join();
#ifdef _WIN32
    WSACleanup();
#endif
    std::cout << "[alyssad] encerrado\n";
    return 0;
}
