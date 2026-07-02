/**
 * @file UserPrefs.hpp
 * @brief Preferências aprendidas do usuário (companion feature).
 *
 * Armazenamento simples em JSON (user_prefs.json no diretório de trabalho).
 * A Alyssa salva via tool save_preference quando o usuário demonstra gostar
 * de algo; o modo lazer (AwayLeisure) e as conversas usam list como contexto.
 * Header-only, puro, testável.
 */

#pragma once

#include "json.hpp"

#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace alyssa_prefs {

struct Preference {
    std::string category;   ///< ex: "música", "jogo", "canal"
    std::string value;      ///< ex: "phonk", "Hollow Knight"
    std::string learned_at; ///< YYYY-MM-DD
};

inline const char* PREFS_FILE = "user_prefs.json";

inline std::vector<Preference> load_preferences(const std::string& path = PREFS_FILE) {
    std::vector<Preference> prefs;
    std::ifstream file(path);
    if (!file.is_open()) return prefs;

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        if (j.contains("preferences") && j["preferences"].is_array()) {
            for (const auto& p : j["preferences"]) {
                Preference pref;
                if (p.contains("category")) pref.category = p["category"].get<std::string>();
                if (p.contains("value"))    pref.value = p["value"].get<std::string>();
                if (p.contains("learned_at")) pref.learned_at = p["learned_at"].get<std::string>();
                if (!pref.value.empty()) prefs.push_back(pref);
            }
        }
    } catch (...) {
        // arquivo corrompido: começa do zero (o save reescreve)
    }
    return prefs;
}

inline bool save_preferences(const std::vector<Preference>& prefs,
                             const std::string& path = PREFS_FILE) {
    nlohmann::json j;
    j["preferences"] = nlohmann::json::array();
    for (const auto& p : prefs) {
        j["preferences"].push_back({
            {"category", p.category},
            {"value", p.value},
            {"learned_at", p.learned_at}
        });
    }
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

/**
 * @brief Adiciona (ou atualiza) uma preferência. Deduplica por valor.
 * @return true quando persistiu com sucesso.
 */
inline bool add_preference(const std::string& category, const std::string& value,
                           const std::string& path = PREFS_FILE) {
    if (value.empty()) return false;

    auto prefs = load_preferences(path);
    for (auto& p : prefs) {
        if (p.value == value) {
            p.category = category; // atualiza a categoria se mudou
            return save_preferences(prefs, path);
        }
    }

    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char date[16];
    std::strftime(date, sizeof(date), "%Y-%m-%d", &tm_buf);

    prefs.push_back({category, value, date});
    return save_preferences(prefs, path);
}

/// Linha compacta para injetar em prompts ("" quando não há nada aprendido).
inline std::string render_preferences_line(const std::string& path = PREFS_FILE) {
    auto prefs = load_preferences(path);
    if (prefs.empty()) return "";

    std::string line = "Gostos conhecidos do usuário: ";
    for (size_t i = 0; i < prefs.size(); ++i) {
        line += prefs[i].value;
        if (!prefs[i].category.empty()) line += " (" + prefs[i].category + ")";
        if (i + 1 < prefs.size()) line += ", ";
    }
    return line;
}

} // namespace alyssa_prefs
