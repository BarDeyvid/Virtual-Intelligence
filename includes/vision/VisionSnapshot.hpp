#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>

namespace alyssa_vision {

struct VisionSnapshot {
    bool valid = false;
    std::chrono::steady_clock::time_point timestamp;

    // Face & identity
    bool face_detected = false;
    std::string user_identity;          // "Deyvid", "stranger", or "unknown"
    float face_confidence = 0.0f;
    std::string expression;             // "happy", "neutral", "tired", "surprised", "angry"

    // Hands & gestures
    bool hand_detected = false;
    std::string gesture;                // "wave", "thumbsup", "peace", "point", "stop", "none"

    // Screen context
    cv::Point2i cursor;
    std::string active_window_title;
    std::string active_window_class;
    int workspace_id = -1;

    // Environment
    float brightness = 0.0f;            // 0-255
    float motion_level = 0.0f;          // 0-1, rough estimate of scene change

    // Raw frame for UI preview (downscaled)
    cv::Mat preview;
};

} // namespace alyssa_vision