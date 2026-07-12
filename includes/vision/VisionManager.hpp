#pragma once
#include "VisionSnapshot.hpp"
#include "ScreenContext.hpp"
#include "FaceRecognizer.hpp"
#include "../HandTracker.hpp"
#include "../HyprlandContext.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <memory>
#include <opencv2/opencv.hpp>

namespace alyssa_vision {

// Forward declaration
class PresenceDetector;

class VisionManager {
public:
    using SnapshotCallback = std::function<void(const VisionSnapshot&)>;

    VisionManager(int camera_index = 0);
    ~VisionManager();

    // Start/stop the continuous pipeline
    void start(SnapshotCallback on_snapshot = nullptr);
    void stop();

    // Get the latest snapshot (thread-safe)
    VisionSnapshot get_snapshot() const;

    // Check if running
    bool is_running() const { return running_; }

    FaceRecognizer* get_face_recognizer() { return face_recognizer_.get(); }

private:
    void loop();
    void process_frame(const cv::Mat& frame);
    void update_screen_context();
    void update_environment(const cv::Mat& frame);

    // Components
    std::unique_ptr<virtual_intelligence::HandTracker> hand_tracker_;
    std::unique_ptr<FaceRecognizer> face_recognizer_;
    std::unique_ptr<ScreenContextProvider> screen_context_;
    cv::VideoCapture cap_;

    // Configuration
    int camera_index_;
    int frame_width_ = 640;   // capture resolution
    int frame_height_ = 480;
    int target_fps_ = 15;

    // State
    std::atomic<bool> running_{false};
    std::thread worker_;
    mutable std::mutex snapshot_mutex_;
    VisionSnapshot latest_snapshot_;
    SnapshotCallback on_snapshot_;

    // Performance: previous frame for motion
    cv::Mat prev_gray_;
};

} // namespace alyssa_vision