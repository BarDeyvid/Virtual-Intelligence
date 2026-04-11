#include "../includes/VisionEnhancer.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace vision_foveal {

    std::vector<int> FovealVision::quantize_colors(const cv::Mat& crop, int grid_size) {
        cv::Mat small;
        cv::resize(crop, small, cv::Size(grid_size, grid_size), 0, 0, cv::INTER_AREA);
        
        std::vector<int> tokens;
        for (int y = 0; y < small.rows; ++y) {
            for (int x = 0; x < small.cols; ++x) {
                cv::Vec3b pixel = small.at<cv::Vec3b>(y, x);
                // Quantize to 10 levels per channel (0-9)
                tokens.push_back(pixel[2] / 26);  // R
                tokens.push_back(pixel[1] / 26);  // G
                tokens.push_back(pixel[0] / 26);  // B
            }
        }
        return tokens;
    }

    void FovealVision::detect_edges(const cv::Mat& crop, FovealCapture& result) {
        // Convert to grayscale
        cv::Mat gray;
        if (crop.channels() == 3) {
            cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = crop.clone();
        }
        
        // Canny edge detection
        cv::Mat edges;
        cv::Canny(gray, edges, canny_low_threshold, canny_high_threshold);
        result.edge_map = edges;
        
        // Compute edge density
        int edge_pixels = cv::countNonZero(edges);
        result.edge_density = static_cast<float>(edge_pixels) / (crop.rows * crop.cols);
        
        // Create edge grid (downsampled binary grid)
        result.edge_grid = EdgeGrid(edge_grid_size, edge_grid_size);
        
        int grid_cell_h = gray.rows / edge_grid_size;
        int grid_cell_w = gray.cols / edge_grid_size;
        
        for (int gy = 0; gy < edge_grid_size; ++gy) {
            for (int gx = 0; gx < edge_grid_size; ++gx) {
                // Check if this grid cell contains edges
                cv::Rect cell(gx * grid_cell_w, gy * grid_cell_h, grid_cell_w, grid_cell_h);
                cell &= cv::Rect(0, 0, edges.cols, edges.rows);  // Clamp
                
                cv::Mat cell_edges = edges(cell);
                int cell_edge_count = cv::countNonZero(cell_edges);
                
                // Binary: 255 if edges exist, 0 otherwise
                result.edge_grid.at(gy, gx) = (cell_edge_count > 0) ? 255 : 0;
            }
        }
        
        // Convert edge grid to tokens (flat binary array)
        result.edge_tokens.clear();
        for (const auto& val : result.edge_grid.data) {
            result.edge_tokens.push_back(val > 128 ? 1 : 0);
        }
    }

    void FovealVision::clamp_region(int& x, int& y, int src_width, int src_height) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + crop_size > src_width) x = src_width - crop_size;
        if (y + crop_size > src_height) y = src_height - crop_size;
    }

    FovealVision::FovealVision(int crop_sz, int edge_grid_sz)
        : crop_size(crop_sz), edge_grid_size(edge_grid_sz),
          canny_low_threshold(50.0), canny_high_threshold(150.0)
    {
    }

    FovealCapture FovealVision::analyze(const cv::Mat& frame, int cursor_x, int cursor_y) {
        FovealCapture result;
        result.cursor_screen_x = cursor_x;
        result.cursor_screen_y = cursor_y;
        result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Center crop on cursor
        int crop_left = cursor_x - crop_size / 2;
        int crop_top = cursor_y - crop_size / 2;
        
        // Clamp to frame boundaries
        clamp_region(crop_left, crop_top, frame.cols, frame.rows);
        
        result.crop_x = crop_left;
        result.crop_y = crop_top;
        result.crop_width = crop_size;
        result.crop_height = crop_size;
        
        // Extract crop
        cv::Rect crop_rect(crop_left, crop_top, crop_size, crop_size);
        crop_rect &= cv::Rect(0, 0, frame.cols, frame.rows);  // Final safety clamp
        
        if (crop_rect.width <= 0 || crop_rect.height <= 0) {
            return result;  // Empty result if crop is out of bounds
        }
        
        result.color_crop = frame(crop_rect).clone();
        
        // Resize to exactly crop_size×crop_size if needed
        if (result.color_crop.rows != crop_size || result.color_crop.cols != crop_size) {
            cv::Mat resized(crop_size, crop_size, CV_8UC3, cv::Scalar::all(0));
            result.color_crop.copyTo(resized(cv::Rect(0, 0, result.color_crop.cols, result.color_crop.rows)));
            result.color_crop = resized;
        }
        
        // Quantize colors
        result.color_tokens = quantize_colors(result.color_crop, 8);  // 8×8 grid
        
        // Detect edges and create edge grid
        detect_edges(result.color_crop, result);
        
        return result;
    }

    std::vector<int> FovealVision::get_detail_color_tokens(const cv::Mat& crop, int grid_size) {
        return quantize_colors(crop, grid_size);
    }

    void FovealVision::set_canny_thresholds(double low, double high) {
        canny_low_threshold = low;
        canny_high_threshold = high;
    }

    std::vector<FovealCapture> FovealVision::analyze_batch(const cv::Mat& frame,
                                                            const std::vector<std::pair<int, int>>& cursor_positions)
    {
        std::vector<FovealCapture> results;
        for (const auto& [x, y] : cursor_positions) {
            results.push_back(analyze(frame, x, y));
        }
        return results;
    }
}
