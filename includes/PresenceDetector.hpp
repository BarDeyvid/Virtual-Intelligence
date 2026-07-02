/**
 * @file PresenceDetector.hpp
 * @brief Webcam-based user presence detection (night-shift feature).
 *
 * Opens the webcam on demand (LED blinks briefly per check — privacy by
 * design: never keeps the camera streaming), grabs a frame and runs Haar
 * face detection. Feeds two consumers:
 *  - the `webcam_check` tool (Alyssa "olha" pela webcam quando perguntada)
 *  - the ProactivityEngine presence triggers (welcome-back / don't talk to
 *    an empty chair)
 *
 * Graceful degradation everywhere: no webcam, no cascade file, capture
 * failure — all report "unavailable" instead of throwing.
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <mutex>
#include <string>

namespace alyssa_vision {

/**
 * @struct PresenceResult
 * @brief Outcome of one webcam presence check.
 */
struct PresenceResult {
    bool available = false;   ///< false = webcam/cascade unusable (don't trust the rest)
    bool present = false;     ///< true when at least one face was detected
    int face_count = 0;
    double brightness = 0.0;  ///< Mean frame luminance 0-255 (dark room hint)
    std::string error;        ///< Filled when available == false
};

/**
 * @class PresenceDetector
 * @brief Face-based presence detection through the default webcam.
 */
class PresenceDetector {
public:
    /**
     * @param camera_index cv::VideoCapture device index (default 0).
     * @param cascade_path Haar cascade path; default resolves to the copy
     *        shipped in config/ (auto-copied to the build dir).
     */
    explicit PresenceDetector(int camera_index = 0,
                              const std::string& cascade_path = "config/haarcascade_frontalface_default.xml");

    /// true when the cascade loaded — webcam itself is only probed per check.
    bool is_ready() const { return cascade_loaded; }

    /**
     * @brief Open webcam, grab one frame, detect faces, release webcam.
     * @details Blocking (~1-2s on Windows MSMF). Call from a background thread.
     */
    PresenceResult check();

    /**
     * @brief Capture a frame and save it to disk (webcam variant of screenshot).
     * @param filename Output image path.
     * @return Human-readable result string (PT-BR) for the tool pipeline.
     */
    std::string capture_to_file(const std::string& filename);

private:
    int camera_index;
    cv::CascadeClassifier face_cascade;
    bool cascade_loaded = false;
    std::mutex webcam_mtx;    ///< Tool call e thread de proatividade disputam a webcam

    /// Grab a single frame; empty Mat on failure.
    cv::Mat grab_frame(std::string& error_out);
};

} // namespace alyssa_vision
