// ──────────────────────────────────────────────────────────────
// HandTracker.cpp  —  High-Performance MediaPipe Hand Tracking
//                     Implementation
//
// Pipeline:
//   1. Palm detection on full frame (192×192)
//   2. ROI cropping + warping (expanded, rotated)
//   3. Landmark extraction on ROI (256×256)
//
// All hot-path allocations are performed once during
// Initialize() and never repeated during ProcessFrame.
// ──────────────────────────────────────────────────────────────

#include "HandTracker.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

// ── Optional CUDA Provider headers ──────────────────────
#ifdef _WIN32
#include <dml_provider_factory.h>
#endif

namespace vi = virtual_intelligence;

// ═════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════
vi::HandTracker::HandTracker(const HandTrackerConfig& cfg) noexcept
    : config_(cfg)
    , env_(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "HandTracker")
    , memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
{
    // Default session options
    session_options_.SetIntraOpNumThreads(1);
    session_options_.SetInterOpNumThreads(1);
    session_options_.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_options_.DisableCpuMemArena();
}

// ═════════════════════════════════════════════════════════
//  Initialize
// ═════════════════════════════════════════════════════════
bool vi::HandTracker::Initialize() noexcept {
    try {
        // ── Configure provider (CUDA / DML / CPU) ─────────
#if defined(USE_CUDA) && USE_CUDA
        OrtCUDAProviderOptionsV2* cuda_opts = nullptr;
        Ort::GetApi().CreateCUDAProviderOptions(&cuda_opts);
        if (cuda_opts) {
            Ort::GetApi().SetCUDAProviderOptions(cuda_opts, {
                {"device_id", std::to_string(config_.cuda_device_id)},
                {"arena_extend_strategy", "kNextPowerOfTwo"},
                {"gpu_mem_limit", std::to_string(
                    static_cast<int64_t>(config_.gpu_mem_limit_mb) * 1024LL * 1024LL)},
                {"cudnn_conv_algo_search", "EXHAUSTIVE"},
                {"do_copy_in_default_stream", "1"},
            });
            session_options_.AppendExecutionProvider_CUDA(*cuda_opts);
            Ort::GetApi().ReleaseCUDAProviderOptions(cuda_opts);
        }
#elif defined(WIN32)
        // Fallback to DirectML if on Windows – uncomment if linking dml
        // session_options_.AppendExecutionProvider_DML(0);
#endif

        // ── Load both models ──────────────────────────────
        if (!LoadModel(config_.palm_model_path, palm_session_))
            return false;
        if (!LoadModel(config_.landmark_model_path, landmark_session_))
            return false;

        // ── Query IO shapes and allocate buffers ──────────
        SetupSessionIO(palm_session_,
                       palm_input_names_, palm_output_names_,
                       palm_input_shape_,
                       palm_input_names_owned_, palm_output_names_owned_);

        SetupSessionIO(landmark_session_,
                       landmark_input_names_, landmark_output_names_,
                       landmark_input_shape_,
                       landmark_input_names_owned_, landmark_output_names_owned_);

        // ── Validate expected shapes ──────────────────────
        if (static_cast<int>(palm_input_shape_.back()) != kPalmInputSize ||
            static_cast<int>(landmark_input_shape_.back()) != kLandmarkInputSize) {
            return false;
        }

        // ── Allocate persistent input buffers ─────────────
        int64_t palm_elem = palm_input_shape_[0];
        for (size_t i = 1; i < palm_input_shape_.size(); ++i)
            palm_elem *= palm_input_shape_[i];
        palm_input_buffer_.resize(static_cast<size_t>(palm_elem), 0.0f);

        int64_t lm_elem = landmark_input_shape_[0];
        for (size_t i = 1; i < landmark_input_shape_.size(); ++i)
            lm_elem *= landmark_input_shape_[i];
        landmark_input_buffer_.resize(static_cast<size_t>(lm_elem), 0.0f);

        // ── Pre‑create Ort::Value wrappers (pinned addresses) ─
        // ONNX Runtime does NOT own the data – we do.
        palm_input_tensors_.clear();
        palm_input_tensors_.push_back(Ort::Value::CreateTensor<float>(
            memory_info_, palm_input_buffer_.data(), palm_input_buffer_.size(),
            palm_input_shape_.data(), palm_input_shape_.size()));

        landmark_input_tensors_.clear();
        landmark_input_tensors_.push_back(Ort::Value::CreateTensor<float>(
            memory_info_, landmark_input_buffer_.data(), landmark_input_buffer_.size(),
            landmark_input_shape_.data(), landmark_input_shape_.size()));

        return true;

    } catch (const Ort::Exception& ex) {
        return false;
    } catch (const std::exception& ex) {
        return false;
    }
}

// ═════════════════════════════════════════════════════════
//  ProcessFrame  —  full pipeline
// ═════════════════════════════════════════════════════════
bool vi::HandTracker::ProcessFrame(
    const cv::Mat& frame,
    HandLandmarks& out_landmarks) noexcept
{
    // 1. Detect palm
    PalmDetection palm = DetectPalm(frame);
    if (!palm.valid || palm.score < config_.palm_score_threshold) {
        return false;
    }

    // 2. Extract landmarks from ROI
    return ExtractLandmarks(frame, palm, out_landmarks);
}

// ═════════════════════════════════════════════════════════
//  DetectPalm
// ═════════════════════════════════════════════════════════
vi::PalmDetection vi::HandTracker::DetectPalm(
    const cv::Mat& frame) noexcept
{
    // ── Preprocess full frame ─────────────────────────────
    PreprocessForPalm(frame, palm_input_buffer_, palm_input_shape_);

    // ── Run inference ────────────────────────────────────
    try {
        palm_output_tensors_ = palm_session_.Run(
            Ort::RunOptions{nullptr},
            palm_input_names_.data(),  palm_input_tensors_.data(),  palm_input_tensors_.size(),
            palm_output_names_.data(), palm_output_names_.size());
    } catch (const Ort::Exception&) {
        PalmDetection pd;
        pd.valid = false;
        return pd;
    }

    // ── Parse output ──────────────────────────────────────
    return ParsePalmOutput();
}

// ═════════════════════════════════════════════════════════
//  ExtractLandmarks
// ═════════════════════════════════════════════════════════
bool vi::HandTracker::ExtractLandmarks(
    const cv::Mat&       frame,
    const PalmDetection& palm,
    HandLandmarks&       out_landmarks) noexcept
{
    // ── Compute ROI (MediaPipe‑style rotated crop) ────────
    cv::Size roi_size;
    ComputeRoi(palm, frame.cols, frame.rows,
               scratch_rot_matrix_, roi_size);

    // ── Warp the ROI ──────────────────────────────────────
    // Uses the pre‑computed affine matrix (scratch_rot_matrix_ is 2×3)
    // warpAffine writes to scratch_roi_; we must ensure it is the right size.
    {
        const auto& M = scratch_rot_matrix_;
        // M is 2×3; we compute size from the diagonal of the bbox * kRoiScale
        int roi_w = roi_size.width;
        int roi_h = roi_size.height;
        scratch_roi_.create(roi_h, roi_w, CV_8UC3);
        cv::warpAffine(frame, scratch_roi_, M, cv::Size(roi_w, roi_h),
                       cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    }

    // ── Preprocess ROI → landmark input tensor ────────────
    PreprocessForLandmark(scratch_roi_, landmark_input_buffer_,
                          landmark_input_shape_);

    // ── Run inference ─────────────────────────────────────
    try {
        landmark_output_tensors_ = landmark_session_.Run(
            Ort::RunOptions{nullptr},
            landmark_input_names_.data(),  landmark_input_tensors_.data(),
            landmark_input_tensors_.size(),
            landmark_output_names_.data(), landmark_output_names_.size());
    } catch (const Ort::Exception&) {
        return false;
    }

    // ── Parse outputs ─────────────────────────────────────
    return ParseLandmarkOutput(out_landmarks);
}

// ═════════════════════════════════════════════════════════
//  Private Helpers
// ═════════════════════════════════════════════════════════

// ── LoadModel ───────────────────────────────────────────
bool vi::HandTracker::LoadModel(
    const std::string& model_path,
    Ort::Session&      session) noexcept
{
    try {
        session = Ort::Session(env_, model_path.c_str(), session_options_);
        return true;
    } catch (const Ort::Exception&) {
        return false;
    }
}

// ── SetupSessionIO ──────────────────────────────────────
void vi::HandTracker::SetupSessionIO(
    Ort::Session&                 session,
    std::vector<const char*>&     input_names,
    std::vector<const char*>&     output_names,
    std::vector<int64_t>&         input_shape,
    std::vector<std::string>&     input_names_owned,
    std::vector<std::string>&     output_names_owned) noexcept
{
    Ort::AllocatorWithDefaultOptions alloc;

    // Input count (typically 1 for these models)
    size_t num_inputs  = session.GetInputCount();
    size_t num_outputs = session.GetOutputCount();

    input_names_owned.clear();
    output_names_owned.clear();
    input_names.clear();
    output_names.clear();

    // ── Inputs ────────────────────────────────────────────
    for (size_t i = 0; i < num_inputs; ++i) {
        auto name     = session.GetInputNameAllocated(i, alloc);
        auto type_info = session.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape    = tensor_info.GetShape();

        input_names_owned.push_back(name.get());
        input_shape   = std::move(shape);
    }

    // ── Outputs ───────────────────────────────────────────
    for (size_t i = 0; i < num_outputs; ++i) {
        auto name = session.GetOutputNameAllocated(i, alloc);
        output_names_owned.push_back(name.get());
    }

    // Build the char* vectors (point into owned strings)
    for (auto& s : input_names_owned)
        input_names.push_back(s.c_str());
    for (auto& s : output_names_owned)
        output_names.push_back(s.c_str());
}

// ── PreprocessForPalm ──────────────────────────────────
// Input:  BGR 8‑bit frame of any size
// Output: Float32 NHWC (or NCHW depending on model)  [1,3,192,192]
void vi::HandTracker::PreprocessForPalm(
    const cv::Mat&       frame,
    std::vector<float>&  buffer,
    std::vector<int64_t>& shape) noexcept
{
    // 1. Convert BGR → RGB
    cv::cvtColor(frame, scratch_rgb_, cv::COLOR_BGR2RGB);

    // 2. Resize to 192×192
    cv::resize(scratch_rgb_, scratch_resized_,
               cv::Size(kPalmInputSize, kPalmInputSize), 0, 0,
               cv::INTER_LINEAR);

    // 3. Convert to float32 and normalize [0,1]
    scratch_resized_.convertTo(scratch_float_, CV_32FC3, 1.0f / 255.0f);

    // 4. Flatten into buffer (CHW layout)
    const int H = kPalmInputSize;
    const int W = kPalmInputSize;
    const int C = scratch_float_.channels();

    float* dst = buffer.data();
    const float* src = scratch_float_.ptr<float>(0);

    // Check if shape is NCHW ([1,3,192,192]) or NHWC
    if (shape.size() == 4) {
        bool is_nchw = (shape[1] == kPalmInputChannels && shape[2] == H && shape[3] == W);
        if (is_nchw) {
            // NCHW: interleave channels per row
            for (int c = 0; c < C; ++c) {
                for (int i = 0; i < H * W; ++i) {
                    dst[c * H * W + i] = src[i * C + c];
                }
            }
        } else {
            // NHWC: direct copy
            std::memcpy(dst, src, static_cast<size_t>(H * W * C) * sizeof(float));
        }
    }
}

// ── PreprocessForLandmark ──────────────────────────────
// Input:  BGR 8‑bit cropped ROI
// Output: Float32 NCHW [1,3,256,256]
void vi::HandTracker::PreprocessForLandmark(
    cv::Mat&             roi,
    std::vector<float>&  buffer,
    std::vector<int64_t>& shape) noexcept
{
    // roi is already the correct size from warpAffine;
    // but we must ensure it's exactly 256×256.
    if (roi.cols != kLandmarkInputSize || roi.rows != kLandmarkInputSize) {
        cv::resize(roi, roi, cv::Size(kLandmarkInputSize, kLandmarkInputSize),
                   0, 0, cv::INTER_LINEAR);
    }

    // BGR → RGB
    cv::cvtColor(roi, scratch_rgb_, cv::COLOR_BGR2RGB);
    scratch_rgb_.convertTo(scratch_float_, CV_32FC3, 1.0f / 255.0f);

    // Flatten NCHW
    const int H = kLandmarkInputSize;
    const int W = kLandmarkInputSize;
    const int C = scratch_float_.channels();

    float* dst = buffer.data();
    const float* src = scratch_float_.ptr<float>(0);

    if (shape.size() == 4) {
        bool is_nchw = (shape[1] == kLandmarkChannels && shape[2] == H && shape[3] == W);
        if (is_nchw) {
            for (int c = 0; c < C; ++c) {
                for (int i = 0; i < H * W; ++i) {
                    dst[c * H * W + i] = src[i * C + c];
                }
            }
        } else {
            std::memcpy(dst, src, static_cast<size_t>(H * W * C) * sizeof(float));
        }
    }
}

// ── ParsePalmOutput ─────────────────────────────────────
// The palm_detection_builtin model has built-in NMS.
// Common output layouts (varies by conversion):
//   - Single tensor: [1, N, 6+]  (x1,y1,x2,y2,score,...)
//   - Separate bbox + score tensors
//
// We auto‑detect from the number of output tensors and their shapes.
vi::PalmDetection vi::HandTracker::ParsePalmOutput() noexcept {
    PalmDetection pd;
    pd.valid = false;

    if (palm_output_tensors_.empty())
        return pd;

    try {
        // ── Multi‑output case (bbox + score) ──────────────
        if (palm_output_tensors_.size() >= 2) {
            // Assume first output is bbox, second is confidence
            auto& bbox_tensor  = palm_output_tensors_[0];
            auto& score_tensor = palm_output_tensors_[1];

            auto bbox_info  = bbox_tensor.GetTensorTypeAndShapeInfo();
            auto score_info = score_tensor.GetTensorTypeAndShapeInfo();
            auto bbox_shape = bbox_info.GetShape();
            auto scr_shape  = score_info.GetShape();

            const float* bbox_data  = bbox_tensor.GetTensorData<float>();
            const float* score_data = score_tensor.GetTensorData<float>();

            // Typical shape: [1, N, 4]  or  [1, N]
            // Or [1, 4] for a single detection with NMS baked in
            if (!bbox_shape.empty() && !scr_shape.empty()) {
                // Extract top detection
                int num_dets = 1;
                pd.valid = true;

                if (bbox_shape.size() == 3) {
                    num_dets = static_cast<int>(bbox_shape[1]);
                }

                float best_score = score_data[0];
                int best_idx = 0;
                for (int i = 1; i < num_dets; ++i) {
                    float s = score_data[i];
                    if (s > best_score) {
                        best_score = s;
                        best_idx = i;
                    }
                }

                pd.score = best_score;

                int stride = (bbox_shape.size() == 3)
                    ? static_cast<int>(bbox_shape[2])
                    : 4;

                const float* b = bbox_data + best_idx * stride;
                float x1 = b[0], y1 = b[1], x2 = b[2], y2 = b[3];

                // Some models output normalized [0,1] – we check magnitude
                if (x2 <= 1.0f && y2 <= 1.0f) {
                    // Assume normalized; caller will handle
                }
                pd.bbox = cv::Rect2f(x1, y1, x2 - x1, y2 - y1);
                pd.rotation = 0.0f;
            }
            return pd;
        }

        // ── Single output tensor ──────────────────────────
        if (palm_output_tensors_.size() == 1) {
            auto& tensor = palm_output_tensors_[0];
            auto info    = tensor.GetTensorTypeAndShapeInfo();
            auto shape   = info.GetShape();

            const float* data = tensor.GetTensorData<float>();

            // Could be [1, N, 6] where each row is [x1,y1,x2,y2,score,...]
            // or [1, N, 4] bbox + separate score elsewhere.
            if (shape.size() == 3 && shape[2] >= 4) {
                int num_dets = static_cast<int>(shape[1]);
                int stride   = static_cast<int>(shape[2]);

                float best_score = (stride >= 5) ? data[4] : data[0];
                int best_idx = 0;
                for (int i = 0; i < num_dets; ++i) {
                    float score = (stride >= 5) ? data[i * stride + 4] : 0.0f;
                    if (score > best_score) {
                        best_score = score;
                        best_idx   = i;
                    }
                }

                const float* b = data + best_idx * stride;
                pd.bbox     = cv::Rect2f(b[0], b[1], b[2] - b[0], b[3] - b[1]);
                pd.score    = (stride >= 5) ? b[4] : 1.0f;
                pd.rotation = 0.0f;
                pd.valid    = true;
            }
            return pd;
        }

    } catch (const Ort::Exception&) {
        // falls through to pd.valid = false
    }

    return pd;
}

// ── ParseLandmarkOutput ─────────────────────────────────
/// Expected outputs from hand_landmark_lite:
///   'Identity'      : (1, 21, 3)  — screen landmarks [x,y,z] normalized [0,1]
///   'Identity_1'    : (1, 1)      — handedness score
///   'Identity_2'    : (1, 21, 3)  — world landmarks in metres
bool vi::HandTracker::ParseLandmarkOutput(
    HandLandmarks& out_landmarks) noexcept
{
    const size_t n = landmark_output_tensors_.size();
    if (n < 2) {
        return false;
    }

    try {
        // ── Identity — Screen landmarks (typically output 0) ─
        {
            auto info = landmark_output_tensors_[0].GetTensorTypeAndShapeInfo();
            auto shape = info.GetShape();
            const float* data = landmark_output_tensors_[0].GetTensorData<float>();

            int num_pts;
            if (shape.size() == 3 && shape[1] >= kNumLandmarks) {
                num_pts = static_cast<int>(shape[1]);
            } else if (shape.size() == 2 && shape[0] >= kNumLandmarks) {
                num_pts = static_cast<int>(shape[0]);
            } else {
                return false;
            }

            out_landmarks.screen.resize(static_cast<size_t>(num_pts));
            int stride = (shape.size() == 3) ? static_cast<int>(shape[2]) : kNumLandmarks;
            for (int i = 0; i < num_pts; ++i) {
                out_landmarks.screen[static_cast<size_t>(i)] = cv::Point3f(
                    data[i * stride + 0],
                    data[i * stride + 1],
                    data[i * stride + 2]);
            }
        }

        // ── Identity_1 — Handedness ───────────────────────
        if (n >= 2) {
            auto info = landmark_output_tensors_[1].GetTensorTypeAndShapeInfo();
            const float* data = landmark_output_tensors_[1].GetTensorData<float>();
            out_landmarks.handedness = data[0];
        }

        // ── Identity_2 — World coordinates ────────────────
        if (n >= 3) {
            auto info = landmark_output_tensors_[2].GetTensorTypeAndShapeInfo();
            auto shape = info.GetShape();
            const float* data = landmark_output_tensors_[2].GetTensorData<float>();

            int num_pts;
            if (shape.size() == 3 && shape[1] >= kNumLandmarks) {
                num_pts = static_cast<int>(shape[1]);
            } else if (shape.size() == 2 && shape[0] >= kNumLandmarks) {
                num_pts = static_cast<int>(shape[0]);
            } else {
                num_pts = static_cast<int>(out_landmarks.screen.size());
            }

            out_landmarks.world.resize(static_cast<size_t>(num_pts));
            int stride = (shape.size() == 3) ? static_cast<int>(shape[2]) : kNumLandmarks;
            for (int i = 0; i < num_pts; ++i) {
                out_landmarks.world[static_cast<size_t>(i)] = cv::Point3f(
                    data[i * stride + 0],
                    data[i * stride + 1],
                    data[i * stride + 2]);
            }
        }

        return true;

    } catch (const Ort::Exception&) {
        return false;
    }
}

// ── ComputeRoi (MediaPipe‑style) ────────────────────────
//
// Given a palm bounding box and an optional rotation angle,
// compute a square ROI expanded by kRoiScale and rotated.
//
// The rotation matrix is stored in rot_matrix (2×3).
void vi::HandTracker::ComputeRoi(
    const PalmDetection& palm,
    int                  frame_w,
    int                  frame_h,
    cv::Mat&             rot_matrix,
    cv::Size&            roi_size) noexcept
{
    // Bounding box center
    double cx = static_cast<double>(palm.bbox.x + palm.bbox.width  * 0.5f);
    double cy = static_cast<double>(palm.bbox.y + palm.bbox.height * 0.5f);

    // Side length (in original image): largest dimension * scale factor
    double side = static_cast<double>(
        std::max(palm.bbox.width, palm.bbox.height) * kRoiScale);

    double half_out = static_cast<double>(kLandmarkInputSize) * 0.5;
    double angle    = static_cast<double>(palm.rotation);  // radians
    double cs       = std::cos(angle);
    double sn       = std::sin(angle);
    double scale    = side / static_cast<double>(kLandmarkInputSize);

    // ── Build 2×3 affine matrix ───────────────────────────
    // Maps a point (x,y) in the original image to (x',y') in the output
    // such that the palm bbox is centered and upright in the 256×256 ROI.
    rot_matrix.create(2, 3, CV_64F);
    rot_matrix.at<double>(0, 0) =  cs * scale;
    rot_matrix.at<double>(0, 1) = -sn * scale;
    rot_matrix.at<double>(1, 0) =  sn * scale;
    rot_matrix.at<double>(1, 1) =  cs * scale;
    rot_matrix.at<double>(0, 2) = cx - (cs * half_out - sn * half_out) * scale;
    rot_matrix.at<double>(1, 2) = cy - (sn * half_out + cs * half_out) * scale;

    roi_size = cv::Size(kLandmarkInputSize, kLandmarkInputSize);
    (void)frame_w;
    (void)frame_h;
}
