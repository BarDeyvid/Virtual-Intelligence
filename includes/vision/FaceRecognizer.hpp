#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "Embedding/Embedder.hpp"   // include the actual Embedder

namespace alyssa_vision {

class FaceRecognizer {
public:
    FaceRecognizer();
    ~FaceRecognizer();

    // Set the embedder (must be initialized)
    void set_embedder(std::shared_ptr<Embedder> embedder); // Embedder is in global namespace

    // Identify a face ROI; returns "known_name" or "stranger" or "unknown"
    std::string identify(const cv::Mat& face_roi);

    // Add a new face to the known set
    void add_known(const std::string& name, const cv::Mat& face_roi);

    // Save/load known faces to disk (optional)
    bool load_database(const std::string& path);
    bool save_database(const std::string& path);

private:
    std::shared_ptr<Embedder> embedder_;
    std::vector<std::pair<std::string, std::vector<float>>> known_faces_;
    mutable std::mutex mtx_;
    float similarity_threshold_ = 0.8f;
};

} // namespace alyssa_vision