#include "VisionManager.hpp"
#include "PresenceDetector.hpp"        // for camera opening helper
#include "HandTracker.hpp"             // added – defines HandTracker, HandTrackerConfig
#include "ScreenContext.hpp"           // added – defines ScreenContext, create_screen_context
#include <chrono>
#include <iostream>

#ifdef __linux__
#include "../includes/HyprlandContext.hpp"
#endif

using virtual_intelligence::HandTracker;
using virtual_intelligence::HandTrackerConfig;
using alyssa_vision::ScreenContext;
using alyssa_vision::create_screen_context;

namespace alyssa_vision {

VisionManager::VisionManager(int camera_index)
    : camera_index_(camera_index)
{
    HandTrackerConfig ht_config;
    // Optionally set paths if you have them, else leave empty.
    hand_tracker_ = std::make_unique<HandTracker>(ht_config);
    if (!hand_tracker_->Initialize()) {
        std::cerr << "[VisionManager] HandTracker initialization failed (model paths missing?)." << std::endl;
        // Init falhou = sessões ONNX num estado indefinido; ProcessFrame em
        // cima disso derruba a thread de visão. Nulo é seguro: process_frame
        // já checa o ponteiro e segue sem gesto.
        hand_tracker_.reset();
    }

    face_recognizer_ = std::make_unique<FaceRecognizer>();
    screen_context_ = create_screen_context();
}


VisionManager::~VisionManager() {
    stop();
}

void VisionManager::start(SnapshotCallback on_snapshot) {
    if (running_) return;
    on_snapshot_ = std::move(on_snapshot);

    // A câmera abre DENTRO da worker thread: cap_.open via DSHOW leva
    // segundos no Windows e estava rodando na main thread ANTES da UI subir
    // — o "congela no boot" de 2026-07-12 era exatamente isso.
    running_ = true;
    worker_ = std::thread(&VisionManager::loop, this);
    std::cout << "[VisionManager] Pipeline iniciando (câmera abre em background)..." << std::endl;
}

void VisionManager::stop() {
    // Sem early-return em !running_: se a câmera falhou, o loop zera
    // running_ sozinho mas a thread ainda precisa de join — destruir uma
    // std::thread joinable é std::terminate.
    running_ = false;
    if (worker_.joinable()) worker_.join();
    cap_.release();
    std::cout << "[VisionManager] Pipeline stopped." << std::endl;
}

VisionSnapshot VisionManager::get_snapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return latest_snapshot_;
}

void VisionManager::loop() {
    // Open (lento) fora da main thread; falha = avisa e encerra o pipeline
    // em paz em vez de derrubar o processo.
    bool opened = false;
#ifdef _WIN32
    opened = cap_.open(camera_index_, cv::CAP_DSHOW);
#else
    opened = cap_.open(camera_index_, cv::CAP_ANY);
#endif
    if (!opened || !cap_.isOpened()) {
        std::cerr << "[VisionManager] Não abriu a câmera " << camera_index_
                  << " (outro app segurando o device?)" << std::endl;
        running_ = false;
        return;
    }
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, frame_width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, frame_height_);
    cap_.set(cv::CAP_PROP_FPS, target_fps_);
    cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    std::cout << "[VisionManager] Câmera aberta — pipeline rodando." << std::endl;

    cv::Mat frame;
    while (running_) {
        auto start = std::chrono::steady_clock::now();

        if (!cap_.read(frame) || frame.empty()) {
            // Camera may be disconnected; try to reopen
            cap_.release();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!cap_.open(camera_index_)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            continue;
        }

        // Um frame ruim (cascade, ONNX, cvt) não pode virar std::terminate —
        // esta thread não tem handler acima dela.
        try {
            process_frame(frame);
        } catch (const std::exception& e) {
            std::cerr << "[VisionManager] Frame falhou, seguindo: " << e.what() << std::endl;
        }

        // Throttle to target FPS
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto target_duration = std::chrono::milliseconds(1000 / target_fps_);
        if (elapsed < target_duration) {
            std::this_thread::sleep_for(target_duration - elapsed);
        }
    }
}

void VisionManager::process_frame(const cv::Mat& frame) {
    VisionSnapshot snap;
    snap.timestamp = std::chrono::steady_clock::now();

    // 1. Face detection & recognition
    // We'll use a cascade detector (from PresenceDetector) or a lightweight CNN.
    // For simplicity, we reuse PresenceDetector's detection logic.
    // We'll create a local PresenceDetector instance just for detection? Better to refactor.
    // We'll implement a simple Haar cascade here for now.
    static cv::CascadeClassifier face_cascade;
    static bool cascade_loaded = false;
    static bool cascade_ok = false;
    if (!cascade_loaded) {
        // Mesmo arquivo que o PresenceDetector usa (copiado pro build output)
        cascade_ok = face_cascade.load("config/haarcascade_frontalface_default.xml");
        if (!cascade_ok) {
            std::cerr << "[VisionManager] Could not load face cascade "
                         "(config/haarcascade_frontalface_default.xml)." << std::endl;
        }
        cascade_loaded = true;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    // cascade_ok, não cascade_loaded: "tentou" != "conseguiu" — chamar
    // detectMultiScale num cascade vazio joga cv::Exception e derrubava a
    // thread inteira (o crash "quando a câmera liga" de 2026-07-12).
    if (cascade_ok) {
        face_cascade.detectMultiScale(gray, faces, 1.1, 5, 0, cv::Size(60, 60));
    }

    if (!faces.empty()) {
        snap.face_detected = true;
        // Use the largest face for recognition
        auto largest = std::max_element(faces.begin(), faces.end(),
            [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); });
        cv::Mat face_roi = frame(*largest);
        snap.face_confidence = 1.0f; // placeholder
        snap.user_identity = face_recognizer_->identify(face_roi);
        // Expression detection: heuristic based on mouth/eyebrow positions (simplified)
        // We'll just set neutral for now
        snap.expression = "neutral";
    } else {
        snap.face_detected = false;
        snap.user_identity = "unknown";
        snap.face_confidence = 0.0f;
    }

    // 2. Hand tracking & gesture
    virtual_intelligence::HandLandmarks landmarks;
    if (hand_tracker_ && hand_tracker_->ProcessFrame(frame, landmarks)) {
        snap.hand_detected = true;
        snap.gesture = virtual_intelligence::HandTracker::classify_gesture(landmarks);
    } else {
        snap.hand_detected = false;
        snap.gesture = "none";
    }

    // 3. Screen context
    update_screen_context();
    if (screen_context_) {
        ScreenContext ctx = screen_context_->get_context();
        snap.cursor.x = ctx.cursor_x;
        snap.cursor.y = ctx.cursor_y;
        snap.active_window_title = ctx.active_window_title;
        snap.active_window_class = ctx.active_window_class;
        snap.workspace_id = ctx.workspace_id;
    }



    // 4. Environment (brightness, motion)
    snap.brightness = static_cast<float>(cv::mean(gray)[0]);
    update_environment(frame);
    // motion_level computed in update_environment

    // 5. Preview (downscaled for UI)
    const int preview_w = 120;
    int preview_h = frame.rows * preview_w / frame.cols;
    cv::resize(frame, snap.preview, cv::Size(preview_w, preview_h), 0, 0, cv::INTER_NEAREST);

    snap.valid = true;

    // Store snapshot
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        latest_snapshot_ = snap;
    }

    // Notify callback
    if (on_snapshot_) on_snapshot_(snap);
}

void VisionManager::update_screen_context() {
    if (screen_context_) {
        screen_context_->update();
    }
}

void VisionManager::update_environment(const cv::Mat& frame) {
    // Motion: compare with previous frame
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    if (prev_gray_.empty()) {
        prev_gray_ = gray.clone();
        latest_snapshot_.motion_level = 0.0f;
        return;
    }
    cv::Mat diff;
    cv::absdiff(gray, prev_gray_, diff);
    double mean_diff = cv::mean(diff)[0];
    latest_snapshot_.motion_level = static_cast<float>(mean_diff / 255.0);
    prev_gray_ = gray.clone();
}

// Helper function to classify gestures (can be moved to HandTracker)
static std::string classify_gesture(const virtual_intelligence::HandLandmarks& lm) {
    // Simple heuristics based on landmark indices (MediaPipe hand landmarks)
    // 0: wrist, 4: thumb tip, 8: index tip, 12: middle tip, 16: ring tip, 20: pinky tip
    if (lm.screen.size() < 21) return "none";

    auto tip = [&](int idx) { return lm.screen[idx]; };
    auto angle = [&](int a, int b, int c) {
        cv::Point3f v1 = lm.screen[a] - lm.screen[b];
        cv::Point3f v2 = lm.screen[c] - lm.screen[b];
        float dot = v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
        float mag1 = std::sqrt(v1.x*v1.x + v1.y*v1.y + v1.z*v1.z);
        float mag2 = std::sqrt(v2.x*v2.x + v2.y*v2.y + v2.z*v2.z);
        if (mag1 == 0 || mag2 == 0) return 0.0f;
        return std::acos(dot / (mag1 * mag2)) * 180.0f / 3.14159f;
    };

    // Check if fingers are extended (tip above knuckle)
    auto is_extended = [&](int tip_idx, int knuckle_idx) {
        return lm.screen[tip_idx].y < lm.screen[knuckle_idx].y;
    };

    bool thumb_up = is_extended(4, 3);
    bool index_up = is_extended(8, 6);
    bool middle_up = is_extended(12, 10);
    bool ring_up = is_extended(16, 14);
    bool pinky_up = is_extended(20, 18);

    // Thumbs up: thumb extended, others closed
    if (thumb_up && !index_up && !middle_up && !ring_up && !pinky_up) return "thumbsup";

    // Peace: index and middle extended
    if (index_up && middle_up && !ring_up && !pinky_up) return "peace";

    // Point: only index extended
    if (index_up && !middle_up && !ring_up && !pinky_up) return "point";

    // Stop: all fingers extended and spread
    if (index_up && middle_up && ring_up && pinky_up) return "stop";

    // Wave: hand moving left-right (we need temporal info, so this is best-effort)
    // For simplicity, we'll just check if hand is open and moving (not implemented yet)
    // Could use history of positions.

    return "none";
}

} // namespace alyssa_vision