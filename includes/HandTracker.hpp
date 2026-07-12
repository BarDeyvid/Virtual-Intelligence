// ──────────────────────────────────────────────────────────────
// HandTracker.hpp  —  High-Performance MediaPipe Hand Tracking
//                     Pipeline (Palm Detection + Landmark Extraction)
//
// Requires: C++17, ONNX Runtime, OpenCV
// ──────────────────────────────────────────────────────────────
#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace virtual_intelligence {

// ═══════════════════════════════════════════════════════════
//  Constants
// ═══════════════════════════════════════════════════════════
inline constexpr int   kPalmInputSize       = 192;
inline constexpr int   kLandmarkInputSize   = 256;
inline constexpr int   kNumLandmarks        = 21;
inline constexpr float kRoiScale            = 2.5f;   // expansion factor
inline constexpr int   kPalmInputChannels   = 3;
inline constexpr int   kLandmarkChannels    = 3;

// ═══════════════════════════════════════════════════════════
//  Data structures
// ═══════════════════════════════════════════════════════════

/// 21 hand landmarks (screen-space normalized + world metric)
struct HandLandmarks {
    std::vector<cv::Point3f> screen;       // (x, y, z)  ∈ [0,1] × [0,1] × depth
    std::vector<cv::Point3f> world;        // (x, y, z)  in metres
    float handedness = 0.0f;               // > 0.5 → right;  < 0.5 → left
    float confidence  = 0.0f;              // palm-detection confidence
};

/// Raw output from the palm detector
struct PalmDetection {
    cv::Rect2f bbox;              // bounding box  (pixel coords)
    float      rotation = 0.0f;   // radians  (0 if unavailable)
    float      score    = 0.0f;
    bool       valid    = false;
};

/// User-facing configuration
struct HandTrackerConfig {
    std::string palm_model_path;
    std::string landmark_model_path;
    bool        enable_cuda           = false;
    int         cuda_device_id        = 0;
    float       palm_score_threshold  = 0.5f;
    float       landmark_conf_threshold = 0.5f;
    int         gpu_mem_limit_mb      = 1024;
};

// ═══════════════════════════════════════════════════════════
//  HandTracker  —  zero‑allocation hot path (after warm-up)
// ═══════════════════════════════════════════════════════════
class HandTracker {
public:
    HandTracker() = default;
    explicit HandTracker(const HandTrackerConfig& cfg) noexcept;
    ~HandTracker() = default;

    // ── Movable, non‑copyable ────────────────────────────
    HandTracker(const HandTracker&)            = delete;
    HandTracker& operator=(const HandTracker&) = delete;
    HandTracker(HandTracker&&)                 = default;
    HandTracker& operator=(HandTracker&&)      = default;

    // ── Life‑cycle ───────────────────────────────────────

    /// Load both models, allocate buffers, prepare sessions.
    /// @returns true on success.
    [[nodiscard]] bool Initialize() noexcept;

    // ── Pipeline ─────────────────────────────────────────

    /// Full pipeline: detect palm → crop & warp → extract landmarks.
    /// @param frame       BGR 8‑bit input image (any size).
    /// @param out_landmarks  Filled only when a hand is detected.
    /// @returns true if a hand was detected and landmarks extracted.
    [[nodiscard]] bool ProcessFrame(
        const cv::Mat&       frame,
        HandLandmarks&       out_landmarks) noexcept;

    /// Run only the palm detector.
    [[nodiscard]] PalmDetection DetectPalm(
        const cv::Mat&       frame) noexcept;

    /// Run only the landmark model on a previously detected palm.
    [[nodiscard]] bool ExtractLandmarks(
        const cv::Mat&       frame,
        const PalmDetection& palm,
        HandLandmarks&       out_landmarks) noexcept;

    static std::string classify_gesture(const HandLandmarks& lm);
    
    void set_config(const HandTrackerConfig& cfg) { config_ = cfg; }

private:
    // ── Configuration ────────────────────────────────────
    HandTrackerConfig config_;

    // ── ONNX Runtime objects ─────────────────────────────
    Ort::Env              env_{nullptr};
    Ort::SessionOptions   session_options_{};
    Ort::Session          palm_session_{nullptr};
    Ort::Session          landmark_session_{nullptr};
    Ort::MemoryInfo       memory_info_{nullptr};
    OrtAllocator*         allocator_{nullptr};          // not owned

    // ── Cached tensor shapes ─────────────────────────────
    std::vector<int64_t>  palm_input_shape_;
    std::vector<int64_t>  landmark_input_shape_;

    // ── Tensor names (pointers into owned strings) ───────
    std::vector<const char*> palm_input_names_;
    std::vector<const char*> palm_output_names_;
    std::vector<const char*> landmark_input_names_;
    std::vector<const char*> landmark_output_names_;

    // ── Owned name strings (kept alive for the pointers) ─
    std::vector<std::string> palm_input_names_owned_;
    std::vector<std::string> palm_output_names_owned_;
    std::vector<std::string> landmark_input_names_owned_;
    std::vector<std::string> landmark_output_names_owned_;

    // ── Pre‑allocated Ort::Value vectors (reused) ────────
    std::vector<Ort::Value> palm_input_tensors_;
    std::vector<Ort::Value> palm_output_tensors_;
    std::vector<Ort::Value> landmark_input_tensors_;
    std::vector<Ort::Value> landmark_output_tensors_;

    // ── Raw pixel buffers (zero‑alloc hot path) ──────────
    std::vector<float>      palm_input_buffer_;
    std::vector<float>      landmark_input_buffer_;

    // ── Scratch buffers for intermediate image processing ─
    cv::Mat                 scratch_rgb_;
    cv::Mat                 scratch_resized_;
    cv::Mat                 scratch_float_;
    cv::Mat                 scratch_roi_;
    cv::Mat                 scratch_rot_matrix_;   // 2×3 affine
    int                     frame_width_  = 0;
    int                     frame_height_ = 0;

    // ── Internal helpers ─────────────────────────────────

    /// Load a single .onnx model and configure its session.
    [[nodiscard]] bool LoadModel(
        const std::string&   model_path,
        Ort::Session&        session) noexcept;

    /// Query input/output names/shapes and allocate buffers.
    void SetupSessionIO(
        Ort::Session&                  session,
        std::vector<const char*>&      input_names,
        std::vector<const char*>&      output_names,
        std::vector<int64_t>&          input_shape,
        std::vector<std::string>&      input_names_owned,
        std::vector<std::string>&      output_names_owned) noexcept;

    /// Preprocess a full frame → palm‑detector input tensor.
    void PreprocessForPalm(
        const cv::Mat&       frame,
        std::vector<float>&  buffer,
        std::vector<int64_t>& shape) noexcept;

    /// Preprocess a cropped ROI → landmark‑model input tensor.
    void PreprocessForLandmark(
        cv::Mat&             roi,              // non‑const: may be resized in-place
        std::vector<float>&  buffer,
        std::vector<int64_t>& shape) noexcept;

    /// Parse the palm‑detector output(s) into a PalmDetection.
    [[nodiscard]] PalmDetection ParsePalmOutput() noexcept;

    /// Parse the landmark‑model output(s) into HandLandmarks.
    [[nodiscard]] bool ParseLandmarkOutput(
        HandLandmarks&       out_landmarks) noexcept;

    /// MediaPipe‑style ROI computation with rotation.
    static void ComputeRoi(
        const PalmDetection& palm,
        int                  frame_w,
        int                  frame_h,
        cv::Mat&             rot_matrix,
        cv::Size&            roi_size) noexcept;
};

}   // namespace virtual_intelligence
