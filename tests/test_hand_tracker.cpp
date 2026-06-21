// ──────────────────────────────────────────────────────────────
// test_hand_tracker.cpp  —  HandTracker integration test
//
// Usage:
//   Build & run:  ./test_hand_tracker [--camera]
//   With --camera: reads from webcam and prints landmarks per frame
//   Without:       loads a static image and runs once
// ──────────────────────────────────────────────────────────────

#include "HandTracker.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

// ──────────────────────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────────────────────
static void PrintLandmarks(const vi::HandLandmarks& lm) {
    std::printf("  Landmarks (%zu pts):\n", lm.screen.size());
    for (size_t i = 0; i < lm.screen.size() && i < 21; ++i) {
        std::printf("    %2zu: screen=(%.4f, %.4f, %.4f)  world=(%.4f, %.4f, %.4f)\n",
                    i,
                    lm.screen[i].x, lm.screen[i].y, lm.screen[i].z,
                    lm.world[i].x, lm.world[i].y, lm.world[i].z);
    }
    std::printf("  Handedness: %.2f  (%s)\n",
                lm.handedness,
                lm.handedness > 0.5f ? "RIGHT" : "LEFT");
}

// ──────────────────────────────────────────────────────────────
//  Main
// ──────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    bool use_camera = (argc > 1 && std::string(argv[1]) == "--camera");

    // ── Configuration ──────────────────────────────────────
    vi::HandTrackerConfig cfg;
    cfg.palm_model_path     = "models/palm_detection_builtin.onnx";
    cfg.landmark_model_path = "models/hand_landmark_lite.onnx";
    cfg.palm_score_threshold = 0.5f;

    // ── Initialize tracker ─────────────────────────────────
    vi::HandTracker tracker(cfg);
    if (!tracker.Initialize()) {
        std::cerr << "[ERROR] Failed to initialize HandTracker.\n";
        std::cerr << "  Check that model files exist at:\n";
        std::cerr << "    " << cfg.palm_model_path << "\n";
        std::cerr << "    " << cfg.landmark_model_path << "\n";
        return 1;
    }

    std::cout << "[INFO] HandTracker initialized successfully.\n";

    if (use_camera) {
        // ── Camera loop ────────────────────────────────────
        cv::VideoCapture cap(0);
        if (!cap.isOpened()) {
            std::cerr << "[ERROR] Could not open camera.\n";
            return 1;
        }

        cv::Mat frame;
        vi::HandLandmarks landmarks;
        int frame_count = 0;

        std::cout << "[INFO] Camera opened. Press ESC to quit.\n";

        while (true) {
            cap >> frame;
            if (frame.empty()) break;

            bool detected = tracker.ProcessFrame(frame, landmarks);

            if (detected) {
                std::printf("[Frame %4d] Hand detected (conf=%.3f)\n",
                            frame_count++, landmarks.confidence);
                PrintLandmarks(landmarks);
            }

            // Optional: draw landmarks on frame for visual feedback
            // (omitted for brevity – use cv::circle on projected points)

            cv::imshow("HandTracker", frame);
            int key = cv::waitKey(1);
            if (key == 27) break;  // ESC
        }

        cap.release();
        cv::destroyAllWindows();

    } else {
        // ── Static image test ──────────────────────────────
        cv::Mat frame = cv::imread("test_hand.jpg");
        if (frame.empty()) {
            std::cout << "[WARN] test_hand.jpg not found; creating dummy frame.\n";
            frame = cv::Mat::zeros(480, 640, CV_8UC3);
            // Draw a rough hand-like shape for sanity check
            cv::rectangle(frame, cv::Rect(200, 150, 80, 120), cv::Scalar(200, 180, 160), cv::FILLED);
            cv::circle(frame, cv::Point(240, 140), 30, cv::Scalar(200, 180, 160), cv::FILLED);
        }

        vi::HandLandmarks landmarks;
        bool detected = tracker.ProcessFrame(frame, landmarks);

        if (detected) {
            std::cout << "[OK] Hand detected.\n";
            PrintLandmarks(landmarks);
        } else {
            std::cout << "[INFO] No hand detected (expected if no real hand in image).\n";
        }
    }

    return 0;
}