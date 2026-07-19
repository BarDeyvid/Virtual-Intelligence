// ActionExecutor.cpp
#include "minecraft/ActionExecutor.hpp"
#include "minecraft/GameplayLog.hpp"

#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>
#include <iostream>
#include <cstdio>

namespace alyssa_minecraft {

using json = nlohmann::json;

namespace {

std::vector<std::string> split_args(const std::string& args_str) {
    std::vector<std::string> args;
    std::istringstream stream(args_str);
    std::string token;
    while (stream >> token) {
        args.push_back(token);
    }
    return args;
}

// The GBNF grammar bounds every field's length now, so a legitimate signal
// is always short — but this is defense in depth, not trust in the model:
// regex-matching an unbounded string is a real risk (a stuck/misbehaving
// generation could hand us kilobytes of garbage), so cap it hard before the
// regex ever sees it rather than relying solely on grammar-side guarantees.
constexpr size_t kMaxSignalLength = 1000;

// Replaces any C0/DEL control byte with a visible \xNN escape so a raw ESC
// (observed once in model output) can never be printed straight into a
// terminal/log and be interpreted as part of an escape sequence.
std::string sanitize_for_display(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7F) {
            char buf[5];
            std::snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// Verbs whose first argument is a label (per gameplay_action.gbnf's "label"
// rule) rather than a literal value, and how many trailing args follow the
// label unchanged (colocar's block name).
struct LabeledVerb { const char* verb; size_t trailing_args; };
constexpr LabeledVerb kLabeledVerbs[] = {
    {"mover", 0},
    {"minerar", 0},
    {"colocar", 1},
};

std::vector<std::string> to_string_vec(const std::array<int, 3>& coord) {
    return {std::to_string(coord[0]), std::to_string(coord[1]), std::to_string(coord[2])};
}

} // namespace

ActionExecutor::ActionExecutor(MinecraftBridge& bridge_ref, alyssa_endocrine::EndocrineSystem& endocrine_ref)
    : bridge(bridge_ref), endocrine(endocrine_ref) {}

json ActionExecutor::execute(const std::string& gameplay_signal_raw, const LabelMap& labels,
                              const EntityLabelMap& entity_names,
                              const std::string& banned_signature) {
    static const std::regex pattern(
        R"(\[AÇÃO\]\s*(\w+)\s*(.*?)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[CONTEXTO\]\s*(.+))");

    const std::string gameplay_signal = gameplay_signal_raw.size() > kMaxSignalLength
        ? gameplay_signal_raw.substr(0, kMaxSignalLength)
        : gameplay_signal_raw;

    std::smatch matches;
    if (!std::regex_search(gameplay_signal, matches, pattern) || matches.size() < 5) {
        std::cerr << "[ActionExecutor] Sinal fora do formato esperado, ignorando: "
                  << sanitize_for_display(gameplay_signal) << std::endl;
        GameplayLog::instance().log("signal_malformed", {{"raw", gameplay_signal}});
        return json{{"ok", false}, {"message", "signal did not match the gameplay grammar"}};
    }

    const std::string verb = matches[1];
    std::vector<std::string> args = split_args(matches[2]);
    const std::string confidence = matches[3];
    const std::string context = matches[4];

    // mover/minerar/colocar carry a label (B1, E2...) instead of raw
    // coordinates — resolve it to real integer coordinates here, before
    // anything reaches the sidecar (which still only understands numbers).
    for (const auto& labeled : kLabeledVerbs) {
        if (verb != labeled.verb) continue;
        if (args.empty()) break;

        // gameplayModel currently runs with no grammar constraining its
        // output (removed for latency — see docs/proximos-passos.md), so it
        // routinely tacks the block/entity name on alongside the label
        // ("minerar spruce_log B2" instead of "minerar B2") — assuming the
        // label is always args[0] failed ~25% of mining attempts live on
        // 2026-07-12. Scan every arg for the one that's actually a known
        // label instead.
        auto label_it = std::find_if(args.begin(), args.end(),
            [&labels](const std::string& a) { return labels.count(a) > 0; });
        if (label_it == args.end()) {
            std::ostringstream joined;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) joined << ' ';
                joined << args[i];
            }
            std::cerr << "[ActionExecutor] Nenhum rótulo conhecido em '" << sanitize_for_display(joined.str())
                      << "' para ação '" << verb << "', ignorando." << std::endl;
            GameplayLog::instance().log("unknown_label", {{"verb", verb}, {"args", args}});
            return json{{"ok", false}, {"message", "no known label in args: " + joined.str()},
                        {"verb", verb}, {"args", args}};
        }

        std::vector<std::string> resolved = to_string_vec(labels.at(*label_it));
        // Any other (non-label) args are trailing values (colocar's block
        // name) — keep them in order, capped at how many this verb expects.
        size_t trailing_added = 0;
        for (auto trailing_it = args.begin();
             trailing_it != args.end() && trailing_added < labeled.trailing_args; ++trailing_it) {
            if (trailing_it == label_it) continue;
            resolved.push_back(*trailing_it);
            ++trailing_added;
        }
        args = std::move(resolved);
        break;
    }

    // "atacar" targets by name, not coordinate — resolve an entity label
    // (E1...) to the name bot.attack() actually needs. Falls back to the raw
    // arg unchanged if it's not a known label, so a model that writes the
    // name directly (e.g. "atacar rabbit") still works exactly as before.
    if (verb == "atacar" && !args.empty()) {
        if (auto it = entity_names.find(args[0]); it != entity_names.end()) {
            args[0] = it->second;
        }
    }

    GameplayLog::instance().log("action_parsed",
        {{"verb", verb}, {"args", args}, {"confidence", confidence}, {"context", context}});

    // Ação proibida (falhou 3+ vezes idênticas seguidas)? Recusa determinística
    // sem tocar o sidecar — ver o doc do parâmetro no header.
    if (!banned_signature.empty()) {
        std::string sig = verb;
        for (const auto& a : args) sig += " " + a;
        if (sig == banned_signature) {
            json refusal = {{"ok", false}, {"banned", true}, {"verb", verb}, {"args", args},
                            {"message", "PROIBIDO: essa exata acao ja falhou varias vezes seguidas - "
                                        "escolha OUTRO alvo ou OUTRO verbo"}};
            GameplayLog::instance().log("action_banned", refusal);
            return refusal;
        }
    }

    const auto exec_start = std::chrono::steady_clock::now();
    json result = bridge.send_action(verb, args);
    const auto exec_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - exec_start).count();
    const bool ok = result.value("ok", false);
    // Echoed back so the caller (MinecraftSession) can tell "the same action
    // against the same target" apart from just "another failure", without
    // re-parsing the raw signal itself.
    result["verb"] = verb;
    result["args"] = args;
    result["execution_time_ms"] = exec_ms;
    GameplayLog::instance().log("action_result", result);

    // Small, per-action endocrine nudge — not meant to dominate the profile,
    // just to make repeated success/failure feel like something over time.
    if (ok) {
        endocrine.trigger_reward_response(0.15);
    } else {
        endocrine.trigger_stress_response(0.1);
        std::cerr << "[ActionExecutor] Ação '" << sanitize_for_display(verb) << "' falhou ("
                  << sanitize_for_display(context) << "): "
                  << result.value("message", "") << std::endl;
    }

    return result;
}

std::vector<json> ActionExecutor::process_pending_events() {
    auto events = bridge.drain_events();
    for (const auto& event : events) {
        const std::string kind = event.value("event", "");
        const json data = event.value("data", json::object());
        GameplayLog::instance().log("sidecar_event", {{"kind", kind}, {"data", data}});

        if (kind == "death") {
            endocrine.trigger_stress_response(0.9);
        } else if (kind == "damage") {
            double amount = data.value("amount", 0.0);
            endocrine.trigger_stress_response(std::min(1.0, amount / 20.0));
        } else if (kind == "chat") {
            endocrine.trigger_social_response(0.3);
        } else if (kind == "reflex") {
            // Reflexo disparou = ameaça/urgência real — mas o loop de reflexo
            // roda a 250ms e um combate dispara VÁRIOS eventos por segundo;
            // sem rate-limit o cortisol crava em ~0.96 em segundos (visto ao
            // vivo em 2026-07-12). Um nudge a cada 5s no máximo.
            static auto last_reflex_nudge = std::chrono::steady_clock::time_point{};
            auto now = std::chrono::steady_clock::now();
            if (now - last_reflex_nudge > std::chrono::seconds(5)) {
                last_reflex_nudge = now;
                endocrine.trigger_stress_response(0.15);
            }
        }
    }
    return events;
}

} // namespace alyssa_minecraft
