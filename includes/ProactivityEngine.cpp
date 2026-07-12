#include "ProactivityEngine.hpp"
#include "vision/VisionSnapshot.hpp"
#include <iostream>

namespace alyssa_proactivity {

void ProactivityEngine::process_vision(const alyssa_vision::VisionSnapshot& snap) {
    if (!cfg.enabled || !snap.valid) return;   // changed config_ to cfg

    auto now = std::chrono::steady_clock::now();

    // User just appeared (known face)
    if (snap.face_detected && snap.user_identity == "Deyvid" && !user_was_present_) {
        if (now - last_vision_trigger_ > std::chrono::seconds(cfg.cooldown_s)) {
            pending_trigger_ = {"welcome_back", "Welcome back, Deyvid! Nice to see you."};
            last_vision_trigger_ = now;
        }
        user_was_present_ = true;
    } else if (!snap.face_detected) {
        user_was_present_ = false;
    }

    // If user is looking at game and we haven't triggered recently
    if (snap.active_window_class.find("minecraft") != std::string::npos ||
        snap.active_window_title.find("Minecraft") != std::string::npos) {
        if (now - last_vision_trigger_ > std::chrono::seconds(cfg.cooldown_s * 2)) {
            pending_trigger_ = {"game_help", "Need a strategy for that boss? I'm watching."};
            last_vision_trigger_ = now;
        }
    }

    // Gesture: wave to pause TTS – can be handled elsewhere
    if (snap.gesture == "wave") {
        std::cout << "[Proactivity] Wave gesture detected." << std::endl;
    }
}

} // namespace alyssa_proactivity