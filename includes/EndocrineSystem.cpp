#include "EndocrineSystem.hpp"
#include <sstream>
#include <iostream>

namespace alyssa_endocrine {

// =========================================================================
// HormoneProfile Implementation
// =========================================================================

std::string HormoneProfile::get_emotional_state() const {
    // Determine dominant emotional state based on hormone levels
    
    if (cortisol > 0.6 && adrenaline > 0.5) {
        return "anxious";
    }
    if (cortisol > 0.5 && serotonin < 0.3) {
        return "stressed";
    }
    if (dopamine > 0.7 && serotonin > 0.6) {
        return "euphoric";
    }
    if (dopamine > 0.6) {
        return "motivated";
    }
    if (oxytocin > 0.6) {
        return "social";
    }
    if (serotonin > 0.6 && cortisol < 0.3) {
        return "calm";
    }
    if (adrenaline > 0.6) {
        return "alert";
    }
    
    return "neutral";
}

std::string HormoneProfile::to_string() const {
    std::ostringstream oss;
    oss << "[HORMONAL STATE]\n"
        << "  Cortisol: " << cortisol << " (stress)\n"
        << "  Dopamine: " << dopamine << " (reward)\n"
        << "  Oxytocin: " << oxytocin << " (social)\n"
        << "  Serotonin: " << serotonin << " (mood)\n"
        << "  Adrenaline: " << adrenaline << " (alert)\n"
        << "  Current State: " << get_emotional_state();
    return oss.str();
}

// =========================================================================
// EndocrineSystem Implementation
// =========================================================================

EndocrineSystem::EndocrineSystem() 
    : baseline_levels() {
    current_levels = baseline_levels;
}

double EndocrineSystem::get_hormone_level(const std::string& hormone_name) const {
    if (hormone_name == "cortisol") return current_levels.cortisol;
    if (hormone_name == "dopamine") return current_levels.dopamine;
    if (hormone_name == "oxytocin") return current_levels.oxytocin;
    if (hormone_name == "serotonin") return current_levels.serotonin;
    if (hormone_name == "adrenaline") return current_levels.adrenaline;
    return 0.0;
}

void EndocrineSystem::set_hormone_level(const std::string& hormone_name, double level) {
    // Clamp level to [0.0, 1.0]
    level = std::max(MIN_HORMONE, std::min(MAX_HORMONE, level));
    
    if (hormone_name == "cortisol") {
        current_levels.cortisol = level;
    } else if (hormone_name == "dopamine") {
        current_levels.dopamine = level;
    } else if (hormone_name == "oxytocin") {
        current_levels.oxytocin = level;
    } else if (hormone_name == "serotonin") {
        current_levels.serotonin = level;
    } else if (hormone_name == "adrenaline") {
        current_levels.adrenaline = level;
    }
}

void EndocrineSystem::update_hormone_levels(const std::vector<std::string>& expert_signals) {
    // Analyze signals from experts and adjust hormones accordingly
    
    for (const auto& signal : expert_signals) {
        std::string lower_signal = signal;
        std::transform(lower_signal.begin(), lower_signal.end(), 
                      lower_signal.begin(), ::tolower);
        
        // Stress/anxiety triggers
        if (lower_signal.find("stress") != std::string::npos ||
            lower_signal.find("anxiety") != std::string::npos ||
            lower_signal.find("worried") != std::string::npos ||
            lower_signal.find("concerning") != std::string::npos) {
            current_levels.cortisol = std::min(MAX_HORMONE, current_levels.cortisol + 0.15);
            current_levels.adrenaline = std::min(MAX_HORMONE, current_levels.adrenaline + 0.10);
        }
        
        // Reward/achievement triggers
        if (lower_signal.find("reward") != std::string::npos ||
            lower_signal.find("success") != std::string::npos ||
            lower_signal.find("excellent") != std::string::npos ||
            lower_signal.find("amazing") != std::string::npos) {
            current_levels.dopamine = std::min(MAX_HORMONE, current_levels.dopamine + 0.15);
            current_levels.serotonin = std::min(MAX_HORMONE, current_levels.serotonin + 0.10);
        }
        
        // Social/connection triggers
        if (lower_signal.find("social") != std::string::npos ||
            lower_signal.find("connection") != std::string::npos ||
            lower_signal.find("friend") != std::string::npos ||
            lower_signal.find("kind") != std::string::npos ||
            lower_signal.find("love") != std::string::npos) {
            current_levels.oxytocin = std::min(MAX_HORMONE, current_levels.oxytocin + 0.15);
            current_levels.serotonin = std::min(MAX_HORMONE, current_levels.serotonin + 0.10);
        }
        
        // Dark/negative triggers (rare, but important)
        if (lower_signal.find("dark") != std::string::npos ||
            lower_signal.find("cruel") != std::string::npos ||
            lower_signal.find("destructive") != std::string::npos) {
            current_levels.cortisol = std::min(MAX_HORMONE, current_levels.cortisol + 0.10);
            current_levels.adrenaline = std::min(MAX_HORMONE, current_levels.adrenaline + 0.10);
        }
    }
    
    // Ensure all levels stay within bounds
    current_levels.cortisol = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.cortisol));
    current_levels.dopamine = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.dopamine));
    current_levels.oxytocin = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.oxytocin));
    current_levels.serotonin = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.serotonin));
    current_levels.adrenaline = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.adrenaline));
}

void EndocrineSystem::apply_metabolism(double decay_rate) {
    decay_rate = std::max(0.0, std::min(1.0, decay_rate)); // Clamp decay_rate to [0, 1]
    
    // Each hormone drifts back toward its baseline
    current_levels.cortisol += (baseline_levels.cortisol - current_levels.cortisol) * decay_rate;
    current_levels.dopamine += (baseline_levels.dopamine - current_levels.dopamine) * decay_rate;
    current_levels.oxytocin += (baseline_levels.oxytocin - current_levels.oxytocin) * decay_rate;
    current_levels.serotonin += (baseline_levels.serotonin - current_levels.serotonin) * decay_rate;
    current_levels.adrenaline += (baseline_levels.adrenaline - current_levels.adrenaline) * decay_rate;
    
    // Ensure all stay within bounds
    current_levels.cortisol = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.cortisol));
    current_levels.dopamine = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.dopamine));
    current_levels.oxytocin = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.oxytocin));
    current_levels.serotonin = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.serotonin));
    current_levels.adrenaline = std::max(MIN_HORMONE, std::min(MAX_HORMONE, current_levels.adrenaline));
}

void EndocrineSystem::reset_to_baseline() {
    current_levels = baseline_levels;
}

double EndocrineSystem::get_expert_weight_multiplier(const std::string& expert_id) const {
    // Map expert IDs to hormone-based weight multipliers
    // Range: [0.5, 2.0] where 1.0 is baseline (no modification)
    
    double multiplier = 1.0;
    
    // Dark/negative models respond to high stress and cortisol
    if (expert_id == "darkModel" || expert_id == "rebellModel") {
        multiplier = 0.8 + (current_levels.cortisol * 1.2); // [0.8, 2.0]
    }
    // Social models respond to oxytocin
    else if (expert_id == "socialModel") {
        multiplier = 0.8 + (current_levels.oxytocin * 1.2); // [0.8, 2.0]
    }
    // Creative models respond to dopamine
    else if (expert_id == "creativeModel") {
        multiplier = 0.8 + (current_levels.dopamine * 0.8); // [0.8, 1.6]
    }
    // Analytical/logical models favor low stress and high serotonin
    else if (expert_id == "analyticalModel") {
        multiplier = 0.6 + ((1.0 - current_levels.cortisol) * 0.8); // [0.6, 1.4]
    }
    // Memory models respond to calm state (high serotonin, low cortisol)
    else if (expert_id == "memoryModel") {
        multiplier = 0.7 + (current_levels.serotonin * 0.6); // [0.7, 1.3]
    }
    // Introspective models favor moderate stress but high serotonin
    else if (expert_id == "introspectiveModel") {
        multiplier = 0.8 + ((current_levels.serotonin + (1.0 - current_levels.cortisol)) * 0.4);
    }
    // Emotional models respond to both dopamine and negative hormones
    else if (expert_id == "emotionalModel") {
        multiplier = 0.8 + (std::max(current_levels.dopamine, current_levels.cortisol) * 0.8);
    }
    // Default: minimal variability for other models
    else {
        multiplier = 1.0 + (current_levels.dopamine - current_levels.cortisol) * 0.3;
    }
    
    // Clamp to reasonable range even if calculations go off
    multiplier = std::max(0.5, std::min(2.0, multiplier));
    return multiplier;
}

std::string EndocrineSystem::generate_hormonal_system_context() const {
    std::ostringstream oss;
    oss << "[SYSTEM: ENDOCRINE STATE] "
        << "You are currently feeling " << current_levels.get_emotional_state() << ". "
        << "Stress level: " << (is_stressed() ? "HIGH" : "normal") << ", "
        << "Mood: " << (current_levels.dopamine > 0.6 ? "optimistic" : "cautious");
    
    if (is_socially_engaged()) {
        oss << ", Social: ENGAGED";
    }
    
    oss << ". Cortisol: " << current_levels.cortisol
        << " | Dopamine: " << current_levels.dopamine
        << " | Oxytocin: " << current_levels.oxytocin << ".";
    
    return oss.str();
}

void EndocrineSystem::trigger_stress_response(double intensity) {
    intensity = std::max(0.0, std::min(1.0, intensity));
    
    current_levels.cortisol = std::min(MAX_HORMONE, current_levels.cortisol + intensity * 0.6);
    current_levels.adrenaline = std::min(MAX_HORMONE, current_levels.adrenaline + intensity * 0.6);
    current_levels.serotonin = std::max(MIN_HORMONE, current_levels.serotonin - intensity * 0.3);
}

void EndocrineSystem::trigger_reward_response(double intensity) {
    intensity = std::max(0.0, std::min(1.0, intensity));
    
    current_levels.dopamine = std::min(MAX_HORMONE, current_levels.dopamine + intensity * 0.4);
    current_levels.serotonin = std::min(MAX_HORMONE, current_levels.serotonin + intensity * 0.3);
    current_levels.cortisol = std::max(MIN_HORMONE, current_levels.cortisol - intensity * 0.2);
}

void EndocrineSystem::trigger_social_response(double intensity) {
    intensity = std::max(0.0, std::min(1.0, intensity));
    
    current_levels.oxytocin = std::min(MAX_HORMONE, current_levels.oxytocin + intensity * 0.5);
    current_levels.serotonin = std::min(MAX_HORMONE, current_levels.serotonin + intensity * 0.3);
}

} // namespace alyssa_endocrine

