/**
 * @file ProactivityEngine.hpp
 * @brief Proactive behavior engine for AlyssaNet (TODO Phase 2.2).
 *
 * Decides WHEN Alyssa should speak without being spoken to, and WHY.
 * The engine is pure decision logic — it never calls the LLM itself. The
 * caller (Alyssa_CLI main loop) polls check() periodically and, on a trigger,
 * asks CoreIntegration::generate_proactive_message() for the actual text.
 *
 * Header-only (like PersonalityCore) and clock-injectable: every method takes
 * an optional explicit time_point so tests are deterministic without sleeping.
 *
 * Triggers (priority order):
 *  1. Boredom     — user idle past threshold and stress low enough to be playful
 *  2. StressCheck — cortisol high: check on the user ("tá tudo bem?")
 *  3. Excitement  — dopamine high: suggest an activity / share a random idea
 *
 * Anti-annoyance rules (TODO Notes: "Too aggressive = annoying"):
 *  - cooldown_s between proactive messages (also applies from startup)
 *  - mood triggers additionally require min_idle_for_mood_s of silence
 */

#pragma once

#include "json.hpp"
#include "EndocrineSystem.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace alyssa_proactivity {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

enum class TriggerType {
    None,
    Boredom,
    StressCheck,
    Excitement,
    UserReturned,  ///< Webcam viu o usuário voltar depois de um tempo fora
    AwayLeisure    ///< Usuário fora há um tempo: Alyssa se distrai sozinha
};

/**
 * @struct ProactiveTrigger
 * @brief A firing decision plus the instruction seed for the LLM (PT-BR).
 */
struct ProactiveTrigger {
    TriggerType type = TriggerType::None;
    std::string reason;
};

/**
 * @struct ProactivityConfig
 * @brief Tunables, loaded from config/proactivity.json.
 */
struct ProactivityConfig {
    bool enabled = true;
    int check_interval_s = 60;               ///< How often the CLI polls check()
    int boredom_threshold_s = 300;           ///< Idle time before boredom fires
    int min_idle_for_mood_s = 120;           ///< Idle required for mood triggers
    int cooldown_s = 600;                    ///< Min gap between proactive messages
    int startup_grace_s = 120;               ///< Min time after boot before the FIRST proactive message
    bool presence_detection = true;          ///< Use webcam presence when available
    int presence_check_interval_s = 90;      ///< Seconds between webcam checks (LED pisca a cada check)
    int min_away_for_welcome_s = 180;        ///< Min absence before a welcome-back fires
    bool away_leisure = true;                ///< Alyssa se distrai (abre site) quando o usuário sai
    int leisure_after_away_s = 240;          ///< Ausência mínima antes do lazer (1x por ausência)
    double max_cortisol_for_boredom = 0.7;   ///< Too stressed → no playful small talk
    double stress_cortisol_threshold = 0.65; ///< Cortisol above this → check on user
    double excitement_dopamine_threshold = 0.75; ///< Dopamine above this → suggest activity
};

/**
 * @brief Load config from JSON; missing/malformed file returns defaults
 *        (graceful degradation, same idiom as the other loaders).
 */
inline ProactivityConfig load_proactivity_config(const std::string& filepath = "config/proactivity.json") {
    ProactivityConfig cfg;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Proactivity] Config não encontrada: " << filepath
                  << " (usando defaults)" << std::endl;
        return cfg;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        if (j.contains("enabled"))                cfg.enabled = j["enabled"].get<bool>();
        if (j.contains("check_interval_s"))       cfg.check_interval_s = j["check_interval_s"].get<int>();
        if (j.contains("boredom_threshold_s"))    cfg.boredom_threshold_s = j["boredom_threshold_s"].get<int>();
        if (j.contains("min_idle_for_mood_s"))    cfg.min_idle_for_mood_s = j["min_idle_for_mood_s"].get<int>();
        if (j.contains("cooldown_s"))             cfg.cooldown_s = j["cooldown_s"].get<int>();
        if (j.contains("startup_grace_s"))        cfg.startup_grace_s = j["startup_grace_s"].get<int>();
        if (j.contains("presence_detection"))     cfg.presence_detection = j["presence_detection"].get<bool>();
        if (j.contains("presence_check_interval_s"))
            cfg.presence_check_interval_s = j["presence_check_interval_s"].get<int>();
        if (j.contains("min_away_for_welcome_s"))
            cfg.min_away_for_welcome_s = j["min_away_for_welcome_s"].get<int>();
        if (j.contains("away_leisure"))           cfg.away_leisure = j["away_leisure"].get<bool>();
        if (j.contains("leisure_after_away_s"))
            cfg.leisure_after_away_s = j["leisure_after_away_s"].get<int>();
        if (j.contains("max_cortisol_for_boredom"))
            cfg.max_cortisol_for_boredom = j["max_cortisol_for_boredom"].get<double>();
        if (j.contains("stress_cortisol_threshold"))
            cfg.stress_cortisol_threshold = j["stress_cortisol_threshold"].get<double>();
        if (j.contains("excitement_dopamine_threshold"))
            cfg.excitement_dopamine_threshold = j["excitement_dopamine_threshold"].get<double>();

        std::cout << "[Proactivity] Config carregada (enabled="
                  << (cfg.enabled ? "sim" : "não")
                  << ", boredom=" << cfg.boredom_threshold_s << "s"
                  << ", cooldown=" << cfg.cooldown_s << "s)" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Proactivity] ERRO ao parsear " << filepath << ": "
                  << e.what() << " (usando defaults)" << std::endl;
        cfg = ProactivityConfig{};
    }

    return cfg;
}

/**
 * @class ProactivityEngine
 * @brief Tracks activity timestamps and decides when a proactive trigger fires.
 */
class ProactivityEngine {
public:
    explicit ProactivityEngine(ProactivityConfig config = {}, TimePoint start = Clock::now())
        : cfg(config), last_activity(start) {
        // Primeira mensagem proativa: liberada após startup_grace_s (não o
        // cooldown inteiro). O last_proactive é retrodatado de forma que o
        // cooldown "vença" exatamente em start + startup_grace_s. Entre
        // mensagens seguintes vale o cooldown cheio.
        int backdate_s = cfg.cooldown_s > cfg.startup_grace_s
                             ? cfg.cooldown_s - cfg.startup_grace_s
                             : 0;
        last_proactive = start - std::chrono::seconds(backdate_s);
    }

    /// Call whenever the user sends input.
    void note_user_activity(TimePoint now = Clock::now()) {
        last_activity = now;
    }

    /// Call after a proactive message was actually delivered.
    void note_proactive_message(TimePoint now = Clock::now()) {
        last_proactive = now;
    }

    /**
     * @brief Feed a webcam presence reading (Fase presença).
     * @details Detecta a transição ausente→presente: se o usuário ficou fora
     *          por min_away_for_welcome_s ou mais, arma o gatilho de
     *          boas-vindas para o próximo check().
     */
    void note_presence(bool present, TimePoint now = Clock::now()) {
        if (had_presence_reading) {
            if (present && !user_present_) {
                long long away_s = std::chrono::duration_cast<std::chrono::seconds>(
                    now - away_since).count();
                if (away_s >= cfg.min_away_for_welcome_s) {
                    welcome_pending = true;
                    last_away_seconds = away_s;
                }
            } else if (!present && user_present_) {
                away_since = now;
                leisure_done_this_absence = false;
            }
        } else if (!present) {
            away_since = now;
            leisure_done_this_absence = false;
        }
        user_present_ = present;
        had_presence_reading = true;
    }

    /// Último estado de presença conhecido (true quando nunca houve leitura).
    bool user_present() const { return !had_presence_reading || user_present_; }

    long long idle_seconds(TimePoint now = Clock::now()) const {
        return std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();
    }

    long long seconds_since_proactive(TimePoint now = Clock::now()) const {
        return std::chrono::duration_cast<std::chrono::seconds>(now - last_proactive).count();
    }

    const ProactivityConfig& config() const { return cfg; }

    /**
     * @brief Decide whether a proactive trigger fires right now.
     * @param h   Current hormone profile (drives mood triggers).
     * @param now Injectable clock for tests.
     */
    ProactiveTrigger check(const alyssa_endocrine::HormoneProfile& h,
                           TimePoint now = Clock::now()) {
        ProactiveTrigger trigger;
        if (!cfg.enabled) return trigger;

        // 0. Boas-vindas: usuário voltou depois de um tempo fora (webcam).
        //    Passa por cima do cooldown — voltar é um evento, não spam.
        if (welcome_pending) {
            welcome_pending = false;
            long long minutes = last_away_seconds / 60;
            trigger.type = TriggerType::UserReturned;
            trigger.reason =
                "Você viu pela webcam que o usuário acabou de voltar pro computador "
                "depois de ~" + std::to_string(minutes) + " minuto(s) fora. "
                "Dê boas-vindas de volta de um jeito casual e curto, sem exagerar.";
            if (leisure_done_this_absence) {
                trigger.reason +=
                    " Enquanto ele estava fora você se distraiu na internet — "
                    "pode mencionar de leve o que estava fazendo.";
            }
            return trigger;
        }

        // Usuário comprovadamente ausente: não falar com a cadeira vazia —
        // mas depois de um tempo sozinha, ela pode se distrair (1x por ausência).
        if (had_presence_reading && !user_present_) {
            if (cfg.away_leisure && !leisure_done_this_absence) {
                long long away_s = std::chrono::duration_cast<std::chrono::seconds>(
                    now - away_since).count();
                if (away_s >= cfg.leisure_after_away_s) {
                    leisure_done_this_absence = true;
                    trigger.type = TriggerType::AwayLeisure;
                    trigger.reason =
                        "O usuário saiu do computador faz " + std::to_string(away_s / 60) +
                        " minuto(s) e você está sozinha. Escolha UMA coisa pra se distrair "
                        "de acordo com seus interesses — por exemplo, abra o YouTube ou outro "
                        "site que você curte usando a ferramenta open_url — e comente pra si "
                        "mesma, em uma frase curta, o que vai fazer.";
                }
            }
            return trigger;
        }

        if (seconds_since_proactive(now) < cfg.cooldown_s) return trigger;

        const long long idle = idle_seconds(now);

        // 1. Tédio: usuário sumiu e o estresse permite descontração
        if (idle >= cfg.boredom_threshold_s && h.cortisol <= cfg.max_cortisol_for_boredom) {
            long long minutes = idle / 60;
            trigger.type = TriggerType::Boredom;
            trigger.reason =
                "O usuário está quieto há " + std::to_string(minutes) +
                " minuto(s) e você está entediada. Puxe assunto de forma natural: "
                "comente algo de um dos seus interesses, faça uma pergunta ou uma "
                "observação casual sobre o momento.";
            return trigger;
        }

        // Gatilhos de humor só depois de um silêncio mínimo (não interromper conversa ativa)
        if (idle < cfg.min_idle_for_mood_s) return trigger;

        // 2. Estresse alto: checar o usuário
        if (h.cortisol >= cfg.stress_cortisol_threshold) {
            trigger.type = TriggerType::StressCheck;
            trigger.reason =
                "Seu nível de estresse está alto e o clima da conversa ficou pesado. "
                "Pergunte de forma leve e genuína se está tudo bem com o usuário.";
            return trigger;
        }

        // 3. Dopamina alta: sugerir atividade / dividir uma ideia
        if (h.dopamine >= cfg.excitement_dopamine_threshold) {
            trigger.type = TriggerType::Excitement;
            trigger.reason =
                "Você está animada e cheia de energia. Sugira algo interessante pra "
                "fazer agora ou compartilhe uma ideia aleatória que te empolgou.";
            return trigger;
        }

        return trigger;
    }

private:
    ProactivityConfig cfg;
    TimePoint last_activity;
    TimePoint last_proactive;

    // Estado de presença (webcam)
    bool had_presence_reading = false;
    bool user_present_ = true;
    TimePoint away_since{};
    bool welcome_pending = false;
    bool leisure_done_this_absence = false;
    long long last_away_seconds = 0;
};

} // namespace alyssa_proactivity
