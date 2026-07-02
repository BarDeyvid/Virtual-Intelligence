/**
 * @file PersonalityCore.hpp
 * @brief Personality system for AlyssaNet (TODO Phase 2.1).
 *
 * Header-only, following the load_config() idiom from AlyssaCore.hpp.
 * Loads static personality traits from config/personality.json and renders a
 * compact per-turn prompt block whose "current state" line is modulated by
 * the EndocrineSystem's hormone levels — the personality is the stable part,
 * the hormones are the daily variation.
 *
 * Deliberately decoupled: only reads the public double fields of
 * HormoneProfile, so tests build without linking EndocrineSystem.cpp.
 */

#pragma once

#include "json.hpp"
#include "EndocrineSystem.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace alyssa_personality {

/**
 * @struct Personality
 * @brief Static personality profile loaded from personality.json.
 */
struct Personality {
    std::string name = "Alyssa";
    std::string identity;
    std::map<std::string, double> traits;   ///< friendliness, curiosity, sass, patience (0.0-1.0)
    std::vector<std::string> interests;
    std::vector<std::string> style_rules;
    double emoji_frequency = 0.0;
    bool loaded = false;                     ///< false = file missing/malformed → block omitted

    double trait(const std::string& key, double fallback = 0.5) const {
        auto it = traits.find(key);
        return it != traits.end() ? it->second : fallback;
    }
};

/**
 * @brief Load personality profile from JSON. Missing file is non-fatal:
 *        returns a Personality with loaded=false and Alyssa works without it.
 */
inline Personality load_personality(const std::string& filepath = "config/personality.json") {
    Personality p;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Personality] Arquivo não encontrado: " << filepath
                  << " (personalidade desativada)" << std::endl;
        return p;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);

        if (j.contains("name"))     p.name     = j["name"].get<std::string>();
        if (j.contains("identity")) p.identity = j["identity"].get<std::string>();

        if (j.contains("traits") && j["traits"].is_object()) {
            for (auto& [key, value] : j["traits"].items()) {
                if (value.is_number()) p.traits[key] = value.get<double>();
            }
        }
        if (j.contains("interests") && j["interests"].is_array()) {
            for (const auto& i : j["interests"]) {
                if (i.is_string()) p.interests.push_back(i.get<std::string>());
            }
        }
        if (j.contains("speech") && j["speech"].is_object()) {
            const auto& s = j["speech"];
            if (s.contains("style_rules") && s["style_rules"].is_array()) {
                for (const auto& r : s["style_rules"]) {
                    if (r.is_string()) p.style_rules.push_back(r.get<std::string>());
                }
            }
            if (s.contains("emoji_frequency") && s["emoji_frequency"].is_number()) {
                p.emoji_frequency = s["emoji_frequency"].get<double>();
            }
        }

        p.loaded = true;
        std::cout << "[Personality] Perfil '" << p.name << "' carregado ("
                  << p.traits.size() << " traços, " << p.interests.size()
                  << " interesses)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Personality] ERRO ao parsear " << filepath << ": " << e.what() << std::endl;
        p = Personality{}; // volta ao estado "não carregado"
    }

    return p;
}

/**
 * @brief Period-of-day label (pure, testable).
 * @param hour Local hour 0-23.
 */
inline std::string period_of_day(int hour) {
    if (hour >= 0 && hour < 6)  return "madrugada";
    if (hour >= 6 && hour < 12) return "manhã";
    if (hour >= 12 && hour < 18) return "tarde";
    return "noite";
}

/// Current local hour (0-23).
inline int current_local_hour() {
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    return tm_buf.tm_hour;
}

/**
 * @brief Derive the "current state" descriptors from traits + hormones.
 *
 * Traits are the baseline; hormones push the effective value up or down.
 * Kept to a single short line — small expert models lose the plot with
 * verbose prompts.
 */
inline std::string derive_current_state(const Personality& p,
                                        const alyssa_endocrine::HormoneProfile* h) {
    if (!h) return "";

    // Energia: motivação + alerta
    double energy = 0.5 * h->dopamine + 0.5 * h->adrenaline;
    std::string energy_desc = energy < 0.25 ? "energia baixa"
                            : energy > 0.6  ? "energia alta"
                                            : "energia normal";

    // Humor: estabilidade menos estresse
    double mood = h->serotonin - 0.5 * h->cortisol;
    std::string mood_desc = mood < 0.15 ? "humor instável, meio no limite"
                          : mood > 0.45 ? "de bom humor"
                                        : "humor estável";

    // Paciência: traço base corroído por cortisol, recuperado por oxitocina
    double patience = p.trait("patience") - 0.5 * h->cortisol + 0.2 * h->oxytocin;
    std::string patience_desc = patience < 0.25 ? "paciência curta hoje"
                              : patience > 0.6  ? "paciente"
                                                : "";

    // Afeto: oxitocina alta deixa mais carinhosa que o normal
    std::string affection_desc = h->oxytocin > 0.6 ? "mais carinhosa que o normal" : "";

    // Sass efetivo: traço base + adrenalina, amaciado por oxitocina
    double sass = p.trait("sass") + 0.2 * h->adrenaline - 0.1 * h->oxytocin;
    std::string sass_desc = sass > 0.7 ? "respondona" : "";

    std::string state = energy_desc + ", " + mood_desc;
    if (!patience_desc.empty())  state += ", " + patience_desc;
    if (!affection_desc.empty()) state += ", " + affection_desc;
    if (!sass_desc.empty())      state += ", " + sass_desc;
    return state;
}

/**
 * @brief Render the compact [PERSONALIDADE] prompt block for the fused prompt.
 * @param p Loaded personality (empty string returned when !p.loaded).
 * @param h Current hormone profile (nullptr → omits the state line).
 */
inline std::string generate_personality_context(const Personality& p,
                                                const alyssa_endocrine::HormoneProfile* h) {
    if (!p.loaded) return "";

    std::string block = "[PERSONALIDADE]\n";
    block += p.name + ": " + p.identity + "\n";

    if (!p.interests.empty()) {
        block += "Curte: ";
        for (size_t i = 0; i < p.interests.size(); ++i) {
            block += p.interests[i];
            if (i + 1 < p.interests.size()) block += ", ";
        }
        block += "\n";
    }

    if (!p.style_rules.empty()) {
        block += "Estilo: ";
        for (size_t i = 0; i < p.style_rules.size(); ++i) {
            block += p.style_rules[i];
            if (i + 1 < p.style_rules.size()) block += "; ";
        }
        block += "\n";
    }

    std::string state = derive_current_state(p, h);
    std::string period = period_of_day(current_local_hour());
    if (!state.empty()) {
        block += "Agora: " + state + ", " + period + "\n";
    } else {
        block += "Agora: " + period + "\n";
    }

    block += "[/PERSONALIDADE]\n";
    return block;
}

} // namespace alyssa_personality
