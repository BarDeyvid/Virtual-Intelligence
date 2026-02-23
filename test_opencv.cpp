#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

// New components
#include "includes/HyprlandContext.hpp"
#include "includes/InputCapture.hpp"
#include "includes/VisionEnhancer.hpp"
#include "includes/DatasetTools.hpp"

using json = nlohmann::json;
using namespace hyprland;
using namespace input_capture;
using namespace vision_foveal;
using namespace dataset_tools;

/**
 * @class VisionPipeline
 * @brief Integrated vision capture and processing pipeline
 * 
 * Combines:
 * - Multi-monitor frame capture (grim)
 * - Hyprland context enrichment
 * - Input event logging
 * - Foveal vision (zoom + edge detection)
 * - Dataset normalization and labeling
 */
class VisionPipeline {
private:
    std::string monitor_L, monitor_R;
    int grid_resolution;
    int foveal_crop_size;
    int sampling_interval_ms;
    
    HyprlandContext hyprland_ctx;
    InputCapture input_logger;
    FovealVision foveal_vision;
    CoordinateNormalizer coord_normalizer;
    ActionLabelizer action_labelizer;
    
    std::string dataset_output_path;
    
    // Internal: Capture monitor via grim
    cv::Mat capture_monitor(const std::string& monitor_name);
    
    // Internal: Quantize colors to tokens (8x8 grid)
    std::vector<int> get_color_tokens(const cv::Mat& frame, int grid_size);
    
    // Internal: Build enriched dataset entry
    json build_dataset_entry(const cv::Mat& frame_L, const cv::Mat& frame_R,
                            int cursor_x, int cursor_y);

public:
    VisionPipeline(const std::string& mon_L = "DP-1", 
                   const std::string& mon_R = "DP-2",
                   int interval_ms = 500);
    
    ~VisionPipeline() = default;
    
    /**
     * @brief Run the main capture loop
     */
    void run();
    
    /**
     * @brief Single capture cycle
     */
    bool capture_frame();
    
    /**
     * @brief Set output dataset path
     */
    void set_output_path(const std::string& path) { dataset_output_path = path; }
    
    /**
     * @brief Configure parameters
     */
    void set_sampling_interval(int ms) { sampling_interval_ms = ms; }
    void set_grid_resolution(int res) { grid_resolution = res; }
};

// ====== VisionPipeline Implementation ======

VisionPipeline::VisionPipeline(const std::string& mon_L, 
                               const std::string& mon_R,
                               int interval_ms)
    : monitor_L(mon_L), monitor_R(mon_R), grid_resolution(8),
      foveal_crop_size(64), sampling_interval_ms(interval_ms),
      foveal_vision(64, 8),
      coord_normalizer(2560, 1440),
      dataset_output_path("dataset_vision_enhanced.jsonl")
{
}

cv::Mat VisionPipeline::capture_monitor(const std::string& monitor_name) {
    std::string cmd = "grim -t ppm -o " + monitor_name + " -";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return cv::Mat();

    std::vector<uchar> buffer;
    uchar chunk[65536];
    size_t bytesRead;
    while ((bytesRead = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        buffer.insert(buffer.end(), chunk, chunk + bytesRead);
    }
    pclose(pipe);
    return buffer.empty() ? cv::Mat() : cv::imdecode(buffer, cv::IMREAD_COLOR);
}

std::vector<int> VisionPipeline::get_color_tokens(const cv::Mat& frame, int grid_size) {
    cv::Mat small;
    cv::resize(frame, small, cv::Size(grid_size, grid_size), 0, 0, cv::INTER_AREA);
    std::vector<int> tokens;
    for (int y = 0; y < small.rows; ++y) {
        for (int x = 0; x < small.cols; ++x) {
            cv::Vec3b p = small.at<cv::Vec3b>(y, x);
            tokens.push_back(p[2]/26); 
            tokens.push_back(p[1]/26); 
            tokens.push_back(p[0]/26);
        }
    }
    return tokens;
}

json VisionPipeline::build_dataset_entry(const cv::Mat& frame_L, const cv::Mat& frame_R,
                                         int cursor_x, int cursor_y) {
    json entry;
    
    // 1. Timestamp
    auto now = std::chrono::system_clock::now();
    entry["timestamp"] = std::chrono::system_clock::to_time_t(now);
    entry["timestamp_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    // 2. Hyprland context (enriched)
    entry["hyprland_context"] = hyprland_ctx.to_json();
    
    // 3. Input events
    entry["input_events"] = input_logger.to_json();
    
    // 4. Basic interaction data
    bool is_hovering_active = false;
    Window active_win = hyprland_ctx.get_active_window();
    if (cursor_x >= active_win.x && cursor_x <= active_win.x + active_win.width &&
        cursor_y >= active_win.y && cursor_y <= active_win.y + active_win.height) {
        is_hovering_active = true;
    }
    
    auto [norm_x, norm_y] = coord_normalizer.normalize(cursor_x, cursor_y);
    
    entry["interaction"] = {
        {"is_hovering_active_window", is_hovering_active},
        {"cursor_global", {cursor_x, cursor_y}},
        {"cursor_normalized", {norm_x, norm_y}}
    };
    
    // 5. Panoramic vision (8×8 grid per monitor)
    entry["visual_left"] = get_color_tokens(frame_L, grid_resolution);
    entry["visual_right"] = get_color_tokens(frame_R, grid_resolution);
    
    // 6. Foveal vision (zoom + edges around cursor)
    FovealCapture fovea_left = foveal_vision.analyze(frame_L, cursor_x, cursor_y);
    FovealCapture fovea_right = foveal_vision.analyze(frame_R, cursor_x, cursor_y);
    
    entry["foveal_vision"] = {
        {"left", fovea_left.to_json()},
        {"right", fovea_right.to_json()}
    };
    
    // 7. Action label (computed from recent events)
    ActionLabel label = action_labelizer.label_from_events(entry["input_events"]);
    entry["action_label"] = label.to_json();
    
    // 8. Window context
    entry["window_context"] = {
        {"title", active_win.title},
        {"class", active_win.class_name},
        {"x", active_win.x},
        {"y", active_win.y},
        {"width", active_win.width},
        {"height", active_win.height},
        {"is_floating", active_win.floating},
        {"monitor_id", active_win.monitor_id}
    };
    
    return entry;
}

bool VisionPipeline::capture_frame() {
    auto start = std::chrono::steady_clock::now();
    
    // 1. Update Hyprland state
    hyprland_ctx.update_state();
    
    // 2. Get cursor position
    int cursor_x, cursor_y;
    hyprland_ctx.get_cursor_pos(cursor_x, cursor_y);
    
    // 3. Capture frames
    cv::Mat frame_L = capture_monitor(monitor_L);
    cv::Mat frame_R = capture_monitor(monitor_R);
    
    if (frame_L.empty() || frame_R.empty()) {
        std::cerr << "Failed to capture frames" << std::endl;
        return false;
    }
    
    // 4. Build and save dataset entry
    json entry = build_dataset_entry(frame_L, frame_R, cursor_x, cursor_y);
    
    std::ofstream out(dataset_output_path, std::ios::app);
    out << entry.dump() << "\n";
    out.close();
    
    // 5. Print status
    Window active = hyprland_ctx.get_active_window();
    std::cout << "[" << std::chrono::system_clock::now().time_since_epoch().count() / 1000000000UL 
              << "] Class: " << active.class_name 
              << " | Cursor: (" << cursor_x << ", " << cursor_y << ")"
              << " | Action: " << entry["action_label"]["action_type"] << std::endl;
    
    // 6. Respect sampling interval
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();
    
    if (elapsed < sampling_interval_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval_ms - elapsed));
    }
    
    return true;
}

void VisionPipeline::run() {
    std::cout << ">>> ENHANCED VISION PIPELINE STARTED" << std::endl;
    std::cout << ">>> Output: " << dataset_output_path << std::endl;
    std::cout << ">>> Monitors: " << monitor_L << ", " << monitor_R << std::endl;
    std::cout << ">>> Sampling interval: " << sampling_interval_ms << " ms" << std::endl;
    std::cout << ">>> Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    
    while (true) {
        if (!capture_frame()) {
            std::cerr << "Error in capture cycle" << std::endl;
            break;
        }
    }
}

// ====== Main ======

int main(int argc, char* argv[]) {
    VisionPipeline pipeline("DP-1", "DP-2", 500);
    
    // Allow CLI argument for output path
    if (argc > 1) {
        pipeline.set_output_path(argv[1]);
    }
    
    pipeline.run();
    return 0;
}