#pragma once
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace alyssa_endocrine {

/**
 * @struct HormoneProfile
 * @brief Represents the current hormonal state of the system.
 */
struct HormoneProfile {
    double cortisol;      ///< Stress hormone (0.0 - 1.0)
    double dopamine;      ///< Reward/motivation hormone (0.0 - 1.0)
    double oxytocin;      ///< Social bonding hormone (0.0 - 1.0)
    double serotonin;     ///< Mood/stability hormone (0.0 - 1.0)
    double adrenaline;    ///< Energy/alertness hormone (0.0 - 1.0)
    
    HormoneProfile() 
        : cortisol(0.2), dopamine(0.5), oxytocin(0.3), serotonin(0.4), adrenaline(0.1) {}
    
    /**
     * @brief Get the overall emotional state based on hormone levels.
     * @return Emotional descriptor (e.g., "alert", "calm", "social", "anxious")
     */
    std::string get_emotional_state() const;
    
    /**
     * @brief Convert hormone profile to debug string.
     * @return Formatted hormonal state
     */
    std::string to_string() const;
};

/**
 * @class EndocrineSystem
 * @brief Manages hormonal levels and their influence on AI behavior.
 * 
 * This system models a simplified endocrine system that affects:
 * - Weight distribution in the Mixture of Experts
 * - Response generation style and tone
 * - Memory consolidation and recall patterns
 * - Social responsiveness and emotional expressiveness
 */
class EndocrineSystem {
private:
    HormoneProfile current_levels;
    HormoneProfile baseline_levels;  ///< Default stable levels
    
    static constexpr double MAX_HORMONE = 1.0;
    static constexpr double MIN_HORMONE = 0.0;
    static constexpr double BASELINE_CORTISOL = 0.2;
    static constexpr double BASELINE_DOPAMINE = 0.5;
    static constexpr double BASELINE_OXYTOCIN = 0.3;
    static constexpr double BASELINE_SEROTONIN = 0.4;
    static constexpr double BASELINE_ADRENALINE = 0.1;

public:
    /**
     * @brief Default constructor.
     * @details Initializes hormone levels to baseline values.
     */
    EndocrineSystem();
    
    /**
     * @brief Get current hormone profile.
     * @return Reference to current hormone levels.
     */
    const HormoneProfile& get_hormone_profile() const { return current_levels; }
    
    /**
     * @brief Get specific hormone level.
     * @param hormone_name Name of hormone (cortisol, dopamine, oxytocin, serotonin, adrenaline)
     * @return Hormone level (0.0 - 1.0)
     */
    double get_hormone_level(const std::string& hormone_name) const;
    
    /**
     * @brief Set hormone level directly (for testing/calibration).
     * @param hormone_name Name of hormone
     * @param level New level (will be clamped to [0.0, 1.0])
     */
    void set_hormone_level(const std::string& hormone_name, double level);
    
    /**
     * @brief Update hormone levels based on expert contributions.
     * 
     * Analyzes the nature of expert responses to trigger hormonal changes.
     * For example:
     * - "stress" or "anxiety" keywords increase cortisol
     * - "reward" or "achievement" keywords increase dopamine
     * - "social" or "connection" keywords increase oxytocin
     * 
     * @param contributions Vector of expert contributions to analyze
     */
    void update_hormone_levels(const std::vector<std::string>& expert_signals);
    
    /**
     * @brief Apply gradual hormone metabolism/decay.
     * 
     * Drives hormone levels back toward baseline over time (like real homeostasis).
     * This prevents "mood locking" and allows recovery from stress.
     * 
     * @param decay_rate Rate of metabolism (0.0 = no change, 0.1 = 10% decay per cycle)
     */
    void apply_metabolism(double decay_rate = 0.05);
    
    /**
     * @brief Reset all hormones to baseline.
     * @details Used when starting fresh conversation or clearing state.
     */
    void reset_to_baseline();
    
    /**
     * @brief Get multiplier for an expert based on hormonal state.
     * 
     * For example:
     * - High cortisol -> darkModel weight increases
     * - High oxytocin -> socialModel weight increases
     * - High dopamine -> creativeModel weight increases
     * 
     * @param expert_id ID of the expert (e.g., "darkModel", "socialModel")
     * @return Weight multiplier (0.5 - 2.0 range, where 1.0 = baseline)
     */
    double get_expert_weight_multiplier(const std::string& expert_id) const;
    
    /**
     * @brief Generate hormonal context string for injection into prompts.
     * 
     * Creates a brief description of current emotional state for the model.
     * Example: "[SYS] You are feeling alert, slightly anxious but social. Cortisol: 0.65"
     * 
     * @return Formatted system context string
     */
    std::string generate_hormonal_system_context() const;
    
    /**
     * @brief Trigger stress response (e.g., from user confrontation).
     * 
     * Simulates acute stress response:
     * - Cortisol ↑ (up to 0.8)
     * - Adrenaline ↑ (up to 0.7)
     * - Serotonin ↓ (down to 0.2)
     * 
     * @param intensity Intensity of stressor [0.0 - 1.0]
     */
    void trigger_stress_response(double intensity);
    
    /**
     * @brief Trigger reward/dopamine response (e.g., successful interaction).
     * 
     * Simulates positive reward:
     * - Dopamine ↑ (up to 0.9)
     * - Serotonin ↑ (up to 0.8)
     * - Cortisol ↓ (down to 0.1)
     * 
     * @param intensity Intensity of reward [0.0 - 1.0]
     */
    void trigger_reward_response(double intensity);
    
    /**
     * @brief Trigger social bonding response (e.g., positive user interaction).
     * 
     * Simulates social bonding:
     * - Oxytocin ↑ (up to 0.8)
     * - Serotonin ↑ (up to 0.8)
     * 
     * @param intensity Intensity of social connection [0.0 - 1.0]
     */
    void trigger_social_response(double intensity);
    
    /**
     * @brief Check if system is in "high stress" state.
     * @return true if cortisol > 0.6
     */
    bool is_stressed() const { return current_levels.cortisol > 0.6; }
    
    /**
     * @brief Check if system is in "high reward" state.
     * @return true if dopamine > 0.7
     */
    bool is_rewarded() const { return current_levels.dopamine > 0.7; }
    
    /**
     * @brief Check if system is in "social" state.
     * @return true if oxytocin > 0.6
     */
    bool is_socially_engaged() const { return current_levels.oxytocin > 0.6; }
};

} // namespace alyssa_endocrine

