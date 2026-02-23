#pragma once

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <cstdint>

using json = nlohmann::json;

/**
 * @namespace vision_foveal
 * @brief Foveal vision: High-resolution cursor zoom + edge detection
 * 
 * Mimics biological vision with:
 * - 64×64 zoomed crop around cursor (high detail)
 * - Edge detection (Canny) for structural info
 * - Simplified edge grid for token representation
 */
namespace vision_foveal {

    /**
     * @struct EdgeGrid
     * @brief Binary edge grid for low-level structural representation
     */
    struct EdgeGrid {
        int rows, cols;
        std::vector<uint8_t> data;  // Binary grid (0 or 255)
        
        EdgeGrid(int r = 8, int c = 8) : rows(r), cols(c), data(r * c, 0) {}
        
        uint8_t& at(int y, int x) {
            return data[y * cols + x];
        }
        
        const uint8_t& at(int y, int x) const {
            return data[y * cols + x];
        }
        
        json to_json() const {
            json grid = json::array();
            for (int y = 0; y < rows; ++y) {
                json row = json::array();
                for (int x = 0; x < cols; ++x) {
                    row.push_back(at(y, x) > 128 ? 1 : 0);
                }
                grid.push_back(row);
            }
            return grid;
        }
    };

    /**
     * @struct FovealCapture
     * @brief Result of foveal analysis around cursor
     */
    struct FovealCapture {
        // Spatial info
        int cursor_screen_x, cursor_screen_y;     // Real position
        int crop_x, crop_y;                       // Crop offset in screen
        int crop_width, crop_height;              // Usually 64x64
        
        // Actual image data
        cv::Mat color_crop;                       // BGR image
        cv::Mat edge_map;                        // Grayscale edge detection (Canny)
        EdgeGrid edge_grid;                      // Binary edge grid (8x8 or similar)
        
        // Token representation
        std::vector<int> color_tokens;           // Quantized colors for the crop
        std::vector<int> edge_tokens;            // Edge grid as tokens
        
        // Metadata
        float edge_density;                      // Proportion of edges in crop
        int64_t timestamp_ms;
        
        FovealCapture() : cursor_screen_x(0), cursor_screen_y(0),
                         crop_x(0), crop_y(0), crop_width(64), crop_height(64),
                         edge_density(0.0f), timestamp_ms(0) {}
        
        json to_json() const {
            return json{
                {"cursor_pos", {cursor_screen_x, cursor_screen_y}},
                {"crop_offset", {crop_x, crop_y}},
                {"crop_size", {crop_width, crop_height}},
                {"color_tokens", color_tokens},
                {"edge_tokens", edge_tokens},
                {"edge_grid", edge_grid.to_json()},
                {"edge_density", edge_density},
                {"timestamp_ms", timestamp_ms}
            };
        }
    };

    /**
     * @class FovealVision
     * @brief Cursor-focused vision analysis
     */
    class FovealVision {
    private:
        int crop_size;                    // Usually 64
        int edge_grid_size;               // Usually 8 (creates 8x8 edge grid)
        double canny_low_threshold;
        double canny_high_threshold;
        
        // Internal: Quantize color crop to tokens
        std::vector<int> quantize_colors(const cv::Mat& crop, int grid_size = 8);
        
        // Internal: Detect edges and create grid
        void detect_edges(const cv::Mat& crop, FovealCapture& result);
        
        // Internal: Clamp region to screen bounds
        void clamp_region(int& x, int& y, int src_width, int src_height);

    public:
        /**
         * @brief Constructor
         * @param crop_sz Crop size around cursor (64 recommended)
         * @param edge_grid_sz Edge grid resolution (8 = 8×8 binary grid)
         */
        FovealVision(int crop_sz = 64, int edge_grid_sz = 8);
        ~FovealVision() = default;

        /**
         * @brief Analyze image around cursor position
         * @param frame Full-screen image (BGR)
         * @param cursor_x Screen cursor X coordinate
         * @param cursor_y Screen cursor Y coordinate
         * @return FovealCapture with zoom, edges, and tokens
         */
        FovealCapture analyze(const cv::Mat& frame, int cursor_x, int cursor_y);

        /**
         * @brief Get high-detail color tokens for 64×64 crop (different from 8×8 grid)
         * Creates a finer-grained 16×16 token grid for the zoom region
         */
        std::vector<int> get_detail_color_tokens(const cv::Mat& crop, int grid_size = 16);

        /**
         * @brief Set Canny edge detection thresholds
         */
        void set_canny_thresholds(double low, double high);

        /**
         * @brief Process a batch of frames (for video sequences)
         */
        std::vector<FovealCapture> analyze_batch(const cv::Mat& frame, 
                                                  const std::vector<std::pair<int, int>>& cursor_positions);

        /**
         * @brief Get crop size
         */
        int get_crop_size() const { return crop_size; }

        /**
         * @brief Get edge grid size
         */
        int get_edge_grid_size() const { return edge_grid_size; }
    };
}
