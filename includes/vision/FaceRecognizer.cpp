#include "FaceRecognizer.hpp"
#include "Embedding/Embedder.hpp"
#include <iostream>
#include <cmath>

namespace alyssa_vision {

FaceRecognizer::FaceRecognizer() = default;
FaceRecognizer::~FaceRecognizer() = default;

void FaceRecognizer::set_embedder(std::shared_ptr<Embedder> embedder) {
    embedder_ = embedder;
}

std::string FaceRecognizer::identify(const cv::Mat& face_roi) {
    if (!embedder_ || !embedder_->is_initialized()) {
        return "unknown";
    }
    if (face_roi.empty()) return "unknown";

    // Preprocess face: resize to expected input (e.g., 224x224)
    cv::Mat resized;
    cv::resize(face_roi, resized, cv::Size(224, 224));
    // Convert to RGB if needed (embedder expects text only? Actually embedder is for text, not images)
    // We need an image embedder, not the text embedder. The current Embedder uses llama.cpp for text.
    // So this is a gap: we need a vision embedder (e.g., CLIP or a face recognition model).
    // For now, we'll use a placeholder: return "unknown" and later we can integrate a face recognition model.

    // Placeholder: for demonstration, we'll just return "Deyvid" if face is large enough
    // In a real implementation, you'd use a face recognition model (e.g., FaceNet, ArcFace) via ONNX.
    // We'll simulate: if face area > 10000 pixels, assume it's Deyvid (just for demo)
    if (face_roi.rows * face_roi.cols > 10000) {
        return "Deyvid";
    }
    return "stranger";
}

void FaceRecognizer::add_known(const std::string& name, const cv::Mat& face_roi) {
    // Placeholder: would compute embedding and store
}

bool FaceRecognizer::load_database(const std::string& path) {
    return false;
}
bool FaceRecognizer::save_database(const std::string& path) {
    return false;
}

} // namespace alyssa_vision