// PresenceDetector.cpp
// Webcam presence detection via Haar face cascade (night-shift feature).

#include "PresenceDetector.hpp"

#include <iostream>

namespace alyssa_vision {

PresenceDetector::PresenceDetector(int camera_index, const std::string& cascade_path)
    : camera_index(camera_index)
{
    // Tenta o caminho dado e um fallback relativo à raiz do repo (execução
    // fora do diretório de build).
    const std::string candidates[] = { cascade_path, "../" + cascade_path };
    for (const auto& path : candidates) {
        if (face_cascade.load(path)) {
            cascade_loaded = true;
            std::cout << "[Presence] Haar cascade carregado de: " << path << std::endl;
            break;
        }
    }
    if (!cascade_loaded) {
        std::cerr << "[Presence] Cascade não encontrado (" << cascade_path
                  << "). Detecção de presença desativada." << std::endl;
    }
}

cv::Mat PresenceDetector::grab_frame(std::string& error_out) {
    cv::VideoCapture cap(camera_index);
    if (!cap.isOpened()) {
        error_out = "webcam indisponível (índice " + std::to_string(camera_index) + ")";
        return {};
    }

    // Descarta os primeiros frames: várias webcams entregam frames escuros
    // enquanto ajustam exposição.
    cv::Mat frame;
    for (int i = 0; i < 3; ++i) cap.read(frame);
    if (!cap.read(frame) || frame.empty()) {
        error_out = "falha ao capturar frame da webcam";
        return {};
    }
    return frame; // cap fecha no destrutor → LED apaga
}

PresenceResult PresenceDetector::check() {
    std::lock_guard<std::mutex> lock(webcam_mtx);
    PresenceResult result;

    if (!cascade_loaded) {
        result.error = "cascade de detecção facial não carregado";
        return result;
    }

    std::string error;
    cv::Mat frame = grab_frame(error);
    if (frame.empty()) {
        result.error = error;
        return result;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    result.brightness = cv::mean(gray)[0];
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    face_cascade.detectMultiScale(gray, faces, 1.1, 5, 0, cv::Size(60, 60));

    result.available = true;
    result.face_count = static_cast<int>(faces.size());
    result.present = !faces.empty();
    return result;
}

std::string PresenceDetector::capture_to_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(webcam_mtx);
    std::string error;
    cv::Mat frame = grab_frame(error);
    if (frame.empty()) {
        return "ERRO: " + error;
    }
    if (!cv::imwrite(filename, frame)) {
        return "ERRO: falha ao salvar imagem da webcam em " + filename;
    }
    return "Foto da webcam salva em '" + filename + "' (" +
           std::to_string(frame.cols) + "x" + std::to_string(frame.rows) + ")";
}

} // namespace alyssa_vision
