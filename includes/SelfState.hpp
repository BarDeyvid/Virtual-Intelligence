/**
 * @file SelfState.hpp
 * @brief Self persistente da Alyssa (v2/F2 — docs/plano-alyssa-v2.md).
 *
 * O que acontece hoje muda quem ela é amanhã: opiniões, metas, piadas
 * internas, agenda e o snapshot hormonal sobrevivem ao restart em
 * state/self.json (relativo ao diretório de trabalho, como user_prefs.json).
 *
 * Header-only, seguindo o idioma do PersonalityCore: load não-fatal (arquivo
 * ausente/corrompido → estado vazio, o save reescreve), deliberadamente
 * DESACOPLADO do EndocrineSystem — o decaimento offline trabalha via
 * callback get/set, então testes compilam sem linkar EndocrineSystem.cpp.
 * As baselines duplicam as constantes de EndocrineSystem.hpp de propósito
 * (mesmo trade-off do PersonalityCore).
 */

#pragma once

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace alyssa_self {

inline const char* SELF_FILE = "state/self.json";

/// Meia-vida efetiva do desvio hormonal offline: TAU horas para o desvio
/// cair a ~37%. 8h ≈ uma noite de sono acalma; 3 dias sem conversar ≈ baseline.
inline constexpr double OFFLINE_DECAY_TAU_HOURS = 8.0;

/// Baselines espelhadas de EndocrineSystem.hpp (BASELINE_*). Duplicadas
/// de propósito — ver comentário do arquivo.
inline const std::map<std::string, double>& hormone_baselines() {
    static const std::map<std::string, double> b = {
        {"cortisol", 0.2}, {"dopamine", 0.5}, {"oxytocin", 0.3},
        {"serotonin", 0.4}, {"adrenaline", 0.1},
    };
    return b;
}

struct Opinion {
    std::string topic;
    std::string stance;
    double confidence = 0.6;
    long long formed_at = 0;        ///< epoch segundos
    long long last_reinforced = 0;  ///< epoch segundos
};

struct Goal {
    std::string desc;
    std::string progress;
    double priority = 0.5;
    long long updated_at = 0;
};

struct AgendaItem {
    std::string bring_up;
    std::string reason;
    long long expires_at = 0;  ///< epoch segundos; 0 = sem validade
};

struct SelfState {
    int version = 1;
    long long saved_at = 0;      ///< epoch do último save (base do decay offline)
    int last_daily_seed = 0;     ///< AAAAMMDD do último humor-do-dia aplicado
    std::map<std::string, double> hormones;  ///< snapshot no save
    std::vector<Opinion> opinions;
    std::vector<Goal> goals;
    std::vector<std::string> inside_jokes;
    nlohmann::json people = nlohmann::json::object();  ///< formato livre até a F3
    std::vector<AgendaItem> agenda;
    bool loaded = false;  ///< false = primeira vida (arquivo ausente/corrompido)
};

inline long long now_epoch() {
    return static_cast<long long>(std::time(nullptr));
}

// =========================================================================
// Load / Save
// =========================================================================

inline SelfState load_self(const std::string& path = SELF_FILE) {
    SelfState s;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[Self] " << path << " ausente — primeira vida, self vazio." << std::endl;
        return s;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);

        s.version         = j.value("version", 1);
        s.saved_at        = j.value("saved_at", 0LL);
        s.last_daily_seed = j.value("last_daily_seed", 0);

        if (j.contains("hormones") && j["hormones"].is_object()) {
            for (auto& [k, v] : j["hormones"].items()) {
                if (v.is_number()) s.hormones[k] = v.get<double>();
            }
        }
        if (j.contains("opinions") && j["opinions"].is_array()) {
            for (const auto& o : j["opinions"]) {
                Opinion op;
                op.topic           = o.value("topic", "");
                op.stance          = o.value("stance", "");
                op.confidence      = o.value("confidence", 0.6);
                op.formed_at       = o.value("formed_at", 0LL);
                op.last_reinforced = o.value("last_reinforced", 0LL);
                if (!op.topic.empty()) s.opinions.push_back(std::move(op));
            }
        }
        if (j.contains("goals") && j["goals"].is_array()) {
            for (const auto& g : j["goals"]) {
                Goal goal;
                goal.desc       = g.value("desc", "");
                goal.progress   = g.value("progress", "");
                goal.priority   = g.value("priority", 0.5);
                goal.updated_at = g.value("updated_at", 0LL);
                if (!goal.desc.empty()) s.goals.push_back(std::move(goal));
            }
        }
        if (j.contains("inside_jokes") && j["inside_jokes"].is_array()) {
            for (const auto& jk : j["inside_jokes"]) {
                if (jk.is_string()) s.inside_jokes.push_back(jk.get<std::string>());
            }
        }
        if (j.contains("people") && j["people"].is_object()) {
            s.people = j["people"];
        }
        if (j.contains("agenda") && j["agenda"].is_array()) {
            for (const auto& a : j["agenda"]) {
                AgendaItem item;
                item.bring_up   = a.value("bring_up", "");
                item.reason     = a.value("reason", "");
                item.expires_at = a.value("expires_at", 0LL);
                if (!item.bring_up.empty()) s.agenda.push_back(std::move(item));
            }
        }

        s.loaded = true;
        std::cout << "[Self] Carregado: " << s.opinions.size() << " opinião(ões), "
                  << s.goals.size() << " meta(s), " << s.agenda.size()
                  << " item(ns) de agenda." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Self] ERRO ao parsear " << path << ": " << e.what()
                  << " (começando self vazio; o save reescreve)" << std::endl;
        s = SelfState{};
    }

    return s;
}

/// Atualiza saved_at e grava. Cria o diretório state/ se preciso.
inline bool save_self(SelfState& s, const std::string& path = SELF_FILE) {
    s.saved_at = now_epoch();

    nlohmann::json j;
    j["version"]         = s.version;
    j["saved_at"]        = s.saved_at;
    j["last_daily_seed"] = s.last_daily_seed;
    j["hormones"]        = s.hormones;

    j["opinions"] = nlohmann::json::array();
    for (const auto& o : s.opinions) {
        j["opinions"].push_back({
            {"topic", o.topic}, {"stance", o.stance}, {"confidence", o.confidence},
            {"formed_at", o.formed_at}, {"last_reinforced", o.last_reinforced},
        });
    }
    j["goals"] = nlohmann::json::array();
    for (const auto& g : s.goals) {
        j["goals"].push_back({
            {"desc", g.desc}, {"progress", g.progress},
            {"priority", g.priority}, {"updated_at", g.updated_at},
        });
    }
    j["inside_jokes"] = s.inside_jokes;
    j["people"]       = s.people;
    j["agenda"] = nlohmann::json::array();
    for (const auto& a : s.agenda) {
        j["agenda"].push_back({
            {"bring_up", a.bring_up}, {"reason", a.reason}, {"expires_at", a.expires_at},
        });
    }

    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Self] Falha ao abrir " << path << " para escrita." << std::endl;
            return false;
        }
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Self] ERRO ao salvar " << path << ": " << e.what() << std::endl;
        return false;
    }
}

// =========================================================================
// Mutações (usadas pelos tool handlers e, na F3, pela consolidação)
// =========================================================================

inline void upsert_opinion(SelfState& s, const std::string& topic,
                           const std::string& stance, double confidence = 0.6) {
    confidence = std::clamp(confidence, 0.0, 1.0);
    for (auto& o : s.opinions) {
        if (o.topic == topic) {
            o.stance = stance;
            o.confidence = confidence;
            o.last_reinforced = now_epoch();
            return;
        }
    }
    Opinion op;
    op.topic = topic;
    op.stance = stance;
    op.confidence = confidence;
    op.formed_at = op.last_reinforced = now_epoch();
    s.opinions.push_back(std::move(op));
}

inline void update_goal(SelfState& s, const std::string& desc,
                        const std::string& progress = "", double priority = -1.0) {
    for (auto& g : s.goals) {
        if (g.desc == desc) {
            if (!progress.empty()) g.progress = progress;
            if (priority >= 0.0) g.priority = std::clamp(priority, 0.0, 1.0);
            g.updated_at = now_epoch();
            return;
        }
    }
    Goal goal;
    goal.desc = desc;
    goal.progress = progress;
    goal.priority = priority >= 0.0 ? std::clamp(priority, 0.0, 1.0) : 0.5;
    goal.updated_at = now_epoch();
    s.goals.push_back(std::move(goal));
}

inline void add_agenda(SelfState& s, const std::string& bring_up,
                       const std::string& reason = "", int expires_days = 3) {
    AgendaItem item;
    item.bring_up = bring_up;
    item.reason = reason;
    item.expires_at = expires_days > 0 ? now_epoch() + 86400LL * expires_days : 0;
    s.agenda.push_back(std::move(item));
}

/// Remove itens de agenda vencidos. Chamar no boot e a cada save de turno.
inline void prune_agenda(SelfState& s, long long now = 0) {
    if (now == 0) now = now_epoch();
    s.agenda.erase(
        std::remove_if(s.agenda.begin(), s.agenda.end(),
                       [now](const AgendaItem& a) {
                           return a.expires_at != 0 && a.expires_at < now;
                       }),
        s.agenda.end());
}

// =========================================================================
// Decaimento hormonal offline
// =========================================================================

/**
 * @brief Aplica o snapshot salvo com decaimento exponencial rumo à baseline.
 * @param saved Snapshot hormonal do último save.
 * @param hours_elapsed Horas de vida desligada (wall-clock).
 * @param set_hormone Callback (nome, nível) — em produção, o
 *        EndocrineSystem::set_hormone_level; nos testes, um lambda.
 * @details nivel = baseline + (salvo − baseline) · e^(−h/TAU). Dormir uma
 *          noite (~8h) deixa ~37% do desvio; 3 dias ≈ baseline de novo.
 */
inline void apply_offline_decay(
    const std::map<std::string, double>& saved,
    double hours_elapsed,
    const std::function<void(const std::string&, double)>& set_hormone) {
    if (hours_elapsed < 0) hours_elapsed = 0;
    const double k = std::exp(-hours_elapsed / OFFLINE_DECAY_TAU_HOURS);
    for (const auto& [name, baseline] : hormone_baselines()) {
        auto it = saved.find(name);
        if (it == saved.end()) continue;
        double level = baseline + (it->second - baseline) * k;
        set_hormone(name, std::clamp(level, 0.0, 1.0));
    }
}

// =========================================================================
// Bloco [EU] pro prompt
// =========================================================================

/**
 * @brief Renderiza o bloco compacto [EU] (~200-400 tokens no máximo).
 * @details Caps deliberados: 5 opiniões (mais reforçadas primeiro), 3 metas
 *          (maior prioridade), 3 piadas internas (mais recentes), 3 itens de
 *          agenda. Tudo vazio → string vazia (sem bloco, sem ruído).
 */
inline std::string render_self_block(const SelfState& s) {
    if (s.opinions.empty() && s.goals.empty() &&
        s.inside_jokes.empty() && s.agenda.empty()) {
        return "";
    }

    std::string block = "[EU]\n";

    if (!s.opinions.empty()) {
        auto sorted = s.opinions;
        std::sort(sorted.begin(), sorted.end(),
                  [](const Opinion& a, const Opinion& b) {
                      return a.last_reinforced > b.last_reinforced;
                  });
        block += "Opiniões formadas (suas, defenda-as ou atualize-as):\n";
        for (size_t i = 0; i < sorted.size() && i < 5; ++i) {
            block += "- " + sorted[i].topic + ": " + sorted[i].stance + "\n";
        }
    }

    if (!s.goals.empty()) {
        auto sorted = s.goals;
        std::sort(sorted.begin(), sorted.end(),
                  [](const Goal& a, const Goal& b) { return a.priority > b.priority; });
        block += "Metas suas:\n";
        for (size_t i = 0; i < sorted.size() && i < 3; ++i) {
            block += "- " + sorted[i].desc;
            if (!sorted[i].progress.empty()) block += " (" + sorted[i].progress + ")";
            block += "\n";
        }
    }

    if (!s.inside_jokes.empty()) {
        block += "Piadas internas: ";
        size_t start = s.inside_jokes.size() > 3 ? s.inside_jokes.size() - 3 : 0;
        for (size_t i = start; i < s.inside_jokes.size(); ++i) {
            block += s.inside_jokes[i];
            if (i + 1 < s.inside_jokes.size()) block += "; ";
        }
        block += "\n";
    }

    if (!s.agenda.empty()) {
        block += "Na sua cabeça pra puxar quando encaixar:\n";
        for (size_t i = 0; i < s.agenda.size() && i < 3; ++i) {
            block += "- " + s.agenda[i].bring_up;
            if (!s.agenda[i].reason.empty()) block += " (" + s.agenda[i].reason + ")";
            block += "\n";
        }
    }

    block += "[/EU]\n";
    return block;
}

}  // namespace alyssa_self
