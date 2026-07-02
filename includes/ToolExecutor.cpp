// ToolExecutor.cpp
// Implementation of the registry-driven tool system (TODO Phase 1.1 / 1.2).

#include "ToolExecutor.hpp"
#include "TicTacToe.hpp"
#include "UserPrefs.hpp"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace alyssa_tools {

// =============================================================================
// Helpers
// =============================================================================

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string now_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

// =============================================================================
// Registry loading
// =============================================================================

bool ToolExecutor::load_registry(const std::string& registry_path) {
    std::ifstream file(registry_path);
    if (!file.is_open()) {
        std::cerr << "[Tools] Registro não encontrado: " << registry_path
                  << " (sistema de tools desativado)" << std::endl;
        return false;
    }

    try {
        json j = json::parse(file);

        if (j.contains("settings") && j["settings"].is_object()) {
            const auto& s = j["settings"];
            if (s.contains("max_rounds"))          max_rounds_          = s["max_rounds"].get<int>();
            if (s.contains("max_calls_per_round")) max_calls_per_round_ = s["max_calls_per_round"].get<int>();
            if (s.contains("default_timeout_s"))   default_timeout_s_   = s["default_timeout_s"].get<int>();
        }

        specs.clear();
        if (j.contains("tools") && j["tools"].is_array()) {
            for (const auto& tool_json : j["tools"]) {
                ToolSpec spec;
                if (!tool_json.contains("name")) continue;
                spec.name = tool_json["name"].get<std::string>();
                if (tool_json.contains("description")) {
                    spec.description = tool_json["description"].get<std::string>();
                }
                if (tool_json.contains("timeout_s")) {
                    spec.timeout_s = tool_json["timeout_s"].get<int>();
                }
                if (tool_json.contains("params") && tool_json["params"].is_array()) {
                    for (const auto& p : tool_json["params"]) {
                        ToolParam param;
                        if (!p.contains("name")) continue;
                        param.name = p["name"].get<std::string>();
                        if (p.contains("type"))     param.type     = p["type"].get<std::string>();
                        if (p.contains("required")) param.required = p["required"].get<bool>();
                        if (p.contains("default")) {
                            // Aceita string ou número no JSON; normaliza para string
                            if (p["default"].is_string()) {
                                param.default_value = p["default"].get<std::string>();
                            } else {
                                param.default_value = p["default"].dump();
                            }
                            param.has_default = true;
                        }
                        spec.params.push_back(param);
                    }
                }
                specs.push_back(spec);
            }
        }

        std::cout << "[Tools] Registro carregado: " << specs.size()
                  << " ferramenta(s) de " << registry_path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Tools] ERRO ao parsear registro: " << e.what() << std::endl;
        specs.clear();
        return false;
    }
}

void ToolExecutor::register_handler(const std::string& tool_name, ToolHandler handler) {
    handlers[tool_name] = std::move(handler);
}

bool ToolExecutor::has_tool(const std::string& name) const {
    return find_spec(name) != nullptr && handlers.count(name) > 0;
}

const ToolSpec* ToolExecutor::find_spec(const std::string& name) const {
    for (const auto& spec : specs) {
        if (spec.name == name) return &spec;
    }
    return nullptr;
}

// =============================================================================
// Prompt generation
// =============================================================================

std::string ToolExecutor::get_tools_prompt() const {
    std::string body;
    std::string example;

    for (const auto& spec : specs) {
        if (handlers.find(spec.name) == handlers.end()) continue; // sem handler, não expõe

        std::string sig = spec.name + "(";
        for (size_t i = 0; i < spec.params.size(); ++i) {
            sig += spec.params[i].name;
            if (!spec.params[i].required) sig += "?";
            if (i + 1 < spec.params.size()) sig += ", ";
        }
        sig += ")";
        body += "- " + sig + ": " + spec.description + "\n";

        // Primeira tool utilizável vira o exemplo concreto (modelos pequenos
        // seguem exemplo, não descrição de formato)
        if (example.empty()) {
            example = spec.name + "(";
            bool first = true;
            for (const auto& p : spec.params) {
                if (!p.required) continue;
                if (!first) example += ", ";
                example += p.name + "=" + (p.type == "int" ? "10" : "valor");
                first = false;
            }
            example += ")";
        }
    }

    if (body.empty()) return "";

    return
        "[FERRAMENTAS]\n"
        "Quando precisar de dados REAIS (arquivos, hora, tela, hardware, web), "
        "não invente: chame uma ferramenta escrevendo APENAS o bloco abaixo e PARE. "
        "O resultado chega em seguida e aí você responde ao usuário.\n"
        "Formato: [TOOL_CALL] nome(param=valor) [/TOOL_CALL]\n"
        "Exemplo: [TOOL_CALL] " + example + " [/TOOL_CALL]\n"
        + body +
        "Não use ferramenta se a resposta não precisar de dados externos.\n"
        "[/FERRAMENTAS]\n";
}

// =============================================================================
// Parsing
// =============================================================================

// Divide "a=1, b=\"x, y\"" em pares chave=valor respeitando aspas.
static std::map<std::string, std::string> parse_args(const std::string& args_str) {
    std::map<std::string, std::string> args;
    size_t i = 0;
    const size_t n = args_str.size();

    while (i < n) {
        // pula espaços e vírgulas
        while (i < n && (std::isspace(static_cast<unsigned char>(args_str[i])) || args_str[i] == ',')) ++i;
        if (i >= n) break;

        // chave até '='
        size_t eq = args_str.find('=', i);
        if (eq == std::string::npos) break; // resto malformado, ignora
        std::string key = trim(args_str.substr(i, eq - i));
        i = eq + 1;
        while (i < n && std::isspace(static_cast<unsigned char>(args_str[i]))) ++i;

        std::string value;
        if (i < n && (args_str[i] == '"' || args_str[i] == '\'')) {
            char quote = args_str[i];
            ++i;
            while (i < n && args_str[i] != quote) {
                if (args_str[i] == '\\' && i + 1 < n && args_str[i + 1] == quote) ++i; // \" escapado
                value += args_str[i];
                ++i;
            }
            if (i < n) ++i; // fecha aspas
        } else {
            size_t comma = args_str.find(',', i);
            if (comma == std::string::npos) comma = n;
            value = trim(args_str.substr(i, comma - i));
            i = comma;
        }

        if (!key.empty()) args[key] = value;
    }
    return args;
}

std::vector<ToolCall> ToolExecutor::parse_tool_calls(const std::string& llm_output) {
    std::vector<ToolCall> calls;
    const std::string open_tag  = "[TOOL_CALL]";
    const std::string close_tag = "[/TOOL_CALL]";

    size_t pos = 0;
    while ((pos = llm_output.find(open_tag, pos)) != std::string::npos) {
        size_t body_start = pos + open_tag.size();
        size_t end = llm_output.find(close_tag, body_start);
        if (end == std::string::npos) break; // bloco não fechado, ignora o resto

        std::string body = trim(llm_output.substr(body_start, end - body_start));
        pos = end + close_tag.size();

        if (body.empty()) continue;

        ToolCall call;
        size_t paren = body.find('(');
        if (paren == std::string::npos) {
            // Forma sem parênteses: apenas o nome da tool
            call.name = trim(body);
        } else {
            call.name = trim(body.substr(0, paren));
            size_t close_paren = body.rfind(')');
            if (close_paren == std::string::npos || close_paren < paren) {
                std::cerr << "[Tools] Bloco TOOL_CALL malformado ignorado: " << body << std::endl;
                continue;
            }
            call.args = parse_args(body.substr(paren + 1, close_paren - paren - 1));
        }

        if (!call.name.empty()) calls.push_back(call);
    }
    return calls;
}

std::string ToolExecutor::strip_tool_calls(const std::string& text) {
    const std::string open_tag  = "[TOOL_CALL]";
    const std::string close_tag = "[/TOOL_CALL]";

    std::string result = text;
    size_t pos;
    while ((pos = result.find(open_tag)) != std::string::npos) {
        size_t end = result.find(close_tag, pos);
        if (end == std::string::npos) {
            result.erase(pos); // bloco não fechado: corta até o fim
        } else {
            result.erase(pos, end + close_tag.size() - pos);
        }
    }
    return trim(result);
}

// =============================================================================
// Execution
// =============================================================================

ToolResult ToolExecutor::execute(const ToolCall& call) {
    ToolResult result;
    auto start = std::chrono::steady_clock::now();

    const ToolSpec* spec = find_spec(call.name);
    auto handler_it = handlers.find(call.name);

    std::map<std::string, std::string> args = call.args;

    if (!spec) {
        result.output = "ERRO: ferramenta desconhecida '" + call.name + "'";
    } else if (handler_it == handlers.end()) {
        result.output = "ERRO: ferramenta '" + call.name + "' não tem implementação registrada";
    } else {
        // Valida obrigatórios e preenche defaults
        std::string validation_error;
        for (const auto& param : spec->params) {
            auto it = args.find(param.name);
            if (it == args.end() || it->second.empty()) {
                if (param.required) {
                    validation_error = "ERRO: parâmetro obrigatório '" + param.name +
                                       "' ausente na chamada de '" + call.name + "'";
                    break;
                }
                if (param.has_default) args[param.name] = param.default_value;
            }
        }

        if (!validation_error.empty()) {
            result.output = validation_error;
        } else {
            int timeout_s = spec->timeout_s > 0 ? spec->timeout_s : default_timeout_s_;

            // Roda o handler em thread separada com timeout. Se estourar, a
            // thread é abandonada (detach) — pode continuar rodando em
            // background. Gambiarra consciente: aceitável para PoC.
            auto promise = std::make_shared<std::promise<std::string>>();
            std::future<std::string> future = promise->get_future();
            ToolHandler handler = handler_it->second;

            std::thread([handler, args, promise]() {
                try {
                    promise->set_value(handler(args));
                } catch (const std::exception& e) {
                    try { promise->set_value(std::string("ERRO: ") + e.what()); } catch (...) {}
                } catch (...) {
                    try { promise->set_value("ERRO: exceção desconhecida no handler"); } catch (...) {}
                }
            }).detach();

            if (future.wait_for(std::chrono::seconds(timeout_s)) == std::future_status::ready) {
                result.output = future.get();
                result.success = result.output.rfind("ERRO:", 0) != 0;
            } else {
                result.output = "ERRO: timeout (" + std::to_string(timeout_s) +
                                "s) executando '" + call.name + "'";
            }
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // Registra no log de chamadas (base para a aba de UI da Fase 5.2)
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        ToolCallRecord record;
        record.tool_name = call.name;
        record.args = call.args;
        record.success = result.success;
        record.output_preview = result.output.substr(0, 200);
        record.duration_ms = elapsed;
        record.timestamp = now_timestamp();
        call_log.push_back(record);
    }

    std::cout << "[Tools] " << call.name << " → "
              << (result.success ? "OK" : "FALHA") << " (" << elapsed << "ms): "
              << (result.output.length() > 80 ? result.output.substr(0, 80) + "..." : result.output)
              << std::endl;

    return result;
}

std::vector<ToolCallRecord> ToolExecutor::get_call_log() const {
    std::lock_guard<std::mutex> lock(log_mtx);
    return call_log;
}

// =============================================================================
// Default handlers (sem dependências pesadas)
// =============================================================================

static std::string home_dir() {
#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
#else
    const char* h = std::getenv("HOME");
#endif
    return h ? h : "";
}

// Compara prefixo de caminho (case-insensitive no Windows).
static bool path_starts_with(const std::string& path, const std::string& base) {
    if (base.empty() || path.size() < base.size()) return false;
    for (size_t i = 0; i < base.size(); ++i) {
        char a = path[i], b = base[i];
#ifdef _WIN32
        a = static_cast<char>(std::tolower(static_cast<unsigned char>(a)));
        b = static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
        if (a == '\\') a = '/';
        if (b == '\\') b = '/';
#endif
        if (a != b) return false;
    }
    return true;
}

// Expande '~' e valida a política de acesso: só a pasta do projeto (CWD) e a
// pasta do usuário (home). Sistema (C:\Windows etc.) continua bloqueado.
// Retorna o caminho expandido, ou "" quando não permitido.
static std::string resolve_user_path(std::string path) {
    if (path.empty()) return "";

    if (path == "~") {
        path = home_dir();
    } else if (path.rfind("~/", 0) == 0 || path.rfind("~\\", 0) == 0) {
        path = home_dir() + path.substr(1);
    }
    if (path.empty()) return "";

    std::error_code ec;
    fs::path abs = fs::weakly_canonical(fs::absolute(fs::path(path), ec), ec);
    if (ec) return "";

    fs::path cwd  = fs::weakly_canonical(fs::current_path(ec), ec);
    fs::path home = home_dir().empty() ? fs::path() : fs::weakly_canonical(fs::path(home_dir()), ec);

    std::string abs_s = abs.generic_string();
    if (path_starts_with(abs_s, cwd.generic_string())) return abs_s;
    if (!home.empty() && path_starts_with(abs_s, home.generic_string())) return abs_s;
    return "";
}

void ToolExecutor::register_default_handlers() {
    register_handler("get_datetime", [](const std::map<std::string, std::string>&) {
        return now_timestamp();
    });

    register_handler("read_file", [](const std::map<std::string, std::string>& args) -> std::string {
        std::string path = resolve_user_path(args.at("path"));
        if (path.empty()) {
            return "ERRO: caminho não permitido (acesso só à pasta do projeto e à sua pasta de usuário). "
                   "Dica: use path=arquivo.txt ou path=~/arquivo.txt";
        }

        int max_lines = 100;
        auto it = args.find("max_lines");
        if (it != args.end()) {
            try { max_lines = std::stoi(it->second); } catch (...) {}
        }
        if (max_lines <= 0) max_lines = 100;

        std::ifstream file(path);
        if (!file.is_open()) {
            return "ERRO: não foi possível abrir o arquivo: " + path;
        }

        constexpr size_t MAX_BYTES = 16 * 1024;
        std::string content;
        std::string line;
        int lines_read = 0;
        while (lines_read < max_lines && std::getline(file, line)) {
            content += line + "\n";
            ++lines_read;
            if (content.size() > MAX_BYTES) {
                content += "[... truncado em 16KB ...]\n";
                break;
            }
        }
        if (content.empty()) return "(arquivo vazio)";
        return content;
    });

    // --- jogo da velha: estado vive aqui, o LLM só transporta jogadas ---
    register_handler("jogo_da_velha", [](const std::map<std::string, std::string>& args) -> std::string {
        static alyssa_games::TicTacToe game;
        static std::mutex game_mtx;
        std::lock_guard<std::mutex> lock(game_mtx);

        const std::string& jogada = args.at("jogada");

        if (jogada == "nova" || jogada == "novo" || jogada == "reset") {
            game.reset();
            return "Novo jogo! Usuário é X, Alyssa é O. Tabuleiro:\n" + game.render() +
                   "Peça pro usuário escolher uma casa (1-9).";
        }

        int cell = 0;
        try { cell = std::stoi(jogada); } catch (...) {
            return "ERRO: jogada inválida '" + jogada + "'. Use jogada=nova ou jogada=1 a 9.";
        }

        std::string error = game.play(cell);
        if (!error.empty()) return "ERRO: " + error;

        return "Tabuleiro atual:\n" + game.render() + game.status_text();
    });

    // --- preferências do usuário: memória de gostos persistente ---
    register_handler("save_preference", [](const std::map<std::string, std::string>& args) -> std::string {
        const std::string& categoria = args.at("categoria");
        const std::string& valor = args.at("valor");
        if (!alyssa_prefs::add_preference(categoria, valor)) {
            return "ERRO: falha ao salvar preferência";
        }
        return "Anotado: usuário gosta de '" + valor + "' (" + categoria + ")";
    });

    register_handler("list_preferences", [](const std::map<std::string, std::string>&) -> std::string {
        std::string line = alyssa_prefs::render_preferences_line();
        if (line.empty()) return "Nenhuma preferência aprendida ainda.";
        return line;
    });

    register_handler("list_dir", [](const std::map<std::string, std::string>& args) -> std::string {
        std::string path = ".";
        auto it = args.find("path");
        if (it != args.end() && !it->second.empty()) path = it->second;

        // Normaliza descrições vagas que o LLM adora passar ("essa pasta",
        // "aqui", "atual"...) para o diretório de trabalho.
        {
            std::string lower = path;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            static const char* aliases[] = {
                "essa pasta", "esta pasta", "dessa pasta", "desta pasta",
                "pasta atual", "diretorio atual", "diretório atual",
                "aqui", "atual", "cwd", "current", "./"
            };
            for (const char* alias : aliases) {
                if (lower == alias) { path = "."; break; }
            }
        }

        path = resolve_user_path(path);
        if (path.empty()) {
            return "ERRO: caminho não permitido (acesso só à pasta do projeto e à sua pasta de usuário). "
                   "Dica: use path=. para a pasta atual ou path=~ para a pasta do usuário.";
        }

        std::error_code ec;
        if (!fs::is_directory(path, ec)) {
            return "ERRO: diretório não encontrado: " + path +
                   ". Dica: use path=. para a pasta atual ou path=~ para a pasta do usuário.";
        }

        std::string listing;
        int count = 0;
        constexpr int MAX_ENTRIES = 50;
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (count >= MAX_ENTRIES) {
                listing += "[... mais entradas omitidas ...]\n";
                break;
            }
            listing += entry.path().filename().string();
            if (entry.is_directory(ec)) listing += "/";
            listing += "\n";
            ++count;
        }
        if (listing.empty()) return "(diretório vazio)";
        return listing;
    });
}

} // namespace alyssa_tools
