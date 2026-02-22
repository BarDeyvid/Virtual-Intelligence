#pragma once
#include "EndocrineSystem.hpp"

struct CoreIntegration::EndocrineSystem {
    std::map<std::string, double> hormone_levels;

    hormone_levels["cortisol"] = 0.2; // Baseline cortisol level
    hormone_levels["dopamine"] = 0.5; // Baseline dopamine level
    hormone_levels["oxytocin"] = 0.3; // Baseline oxytocin level
    hormone_levels["serotonin"] = 0.4; // Baseline serotonin level
    hormone_levels["adrenaline"] = 0.1; // Baseline adrenaline level

    // Method to update hormone levels based on expert contributions
    void update_hormone_levels(const std::vector<alyssa_fusion::ExpertContribution>& contributions) {
        for (const auto& contrib : contributions) {
            if (contrib.signal.find("stress") != std::string::npos) {
                hormone_levels["cortisol"] += 0.1; // Increase cortisol level
            }
            if (contrib.signal.find("reward") != std::string::npos) {
                hormone_levels["dopamine"] += 0.1; // Increase dopamine level
            }
        }
        // Ensure hormone levels stay within a reasonable range [0, 1]
        for (auto& pair : hormone_levels) {
            pair.second = std::min(std::max(pair.second, 0.0), 1.0);
        }
    }

    void apply_metabolism(double rate = 0.05) {
        for (auto& [name, level] : hormone_levels) {
            double baseline = 0.2;
            level += (baseline - level) * rate; // Move towards baseline
            level = std::min(std::max(level, 0.0), 1.0); // Ensure levels stay within [0, 1]
        }
    }
};

