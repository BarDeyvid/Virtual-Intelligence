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
     * @class FrameCache
     * @brief Frame caching + change detection to kill redundant vision work
     *        (TODO Phase 3.1).
     *
     * Key insight: a FovealCapture depends ONLY on the crop region around the
     * cursor. So the cache stays valid while (a) the cursor stays in the same
     * cell and (b) the pixels inside the cached crop barely changed — even if
     * the rest of the screen changed completely. Global screen change is still
     * measured every frame and exposed via last_global_change_ratio(), which
     * is the hook for the future vision-based proactivity trigger.
     */
    class FrameCache {
    public:
        struct Config {
            bool enabled = true;
            double crop_change_threshold = 0.02;   ///< Fraction of changed crop pixels that invalidates
            int pixel_diff_threshold = 25;         ///< Intensity delta (0-255) for a pixel to count as changed
            int cursor_cell_px = 32;               ///< Cursor movement beyond this invalidates
            int signature_size = 32;               ///< Downscale size for the global change signature
        };

        struct Decision {
            bool hit = false;
            double crop_change_ratio = 0.0;        ///< Change inside the cached crop (0 when no cache)
            double global_change_ratio = 0.0;      ///< Frame-to-frame change of the whole screen
        };

        explicit FrameCache(Config cfg = {}) : config(cfg) {}

        /**
         * @brief Evaluate whether the cached capture is still valid for this
         *        frame + cursor. Also updates the global change signature.
         */
        Decision evaluate(const cv::Mat& frame, int cursor_x, int cursor_y);

        /// Store a freshly computed capture as the new cache entry.
        void store(const cv::Mat& frame, const FovealCapture& result);

        const FovealCapture& cached_result() const { return cached; }

        // Stats (for logs/UI)
        uint64_t hits() const { return hit_count; }
        uint64_t misses() const { return miss_count; }
        double hit_rate() const {
            uint64_t total = hit_count + miss_count;
            return total > 0 ? static_cast<double>(hit_count) / total : 0.0;
        }
        double last_global_change_ratio() const { return last_global_change; }

        void invalidate() { has_cache = false; }

    private:
        Config config;
        bool has_cache = false;
        FovealCapture cached;
        cv::Mat cached_crop_gray;                  ///< Grayscale of the cached crop region
        cv::Rect cached_crop_rect;
        int cached_cursor_x = 0, cached_cursor_y = 0;
        cv::Mat prev_signature;                    ///< Downscaled gray of the previous frame
        double last_global_change = 0.0;
        uint64_t hit_count = 0, miss_count = 0;
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
        FrameCache frame_cache;           // Frame cache + change detection (Phase 3.1)
        
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
         * @brief Cached variant of analyze() (TODO Phase 3.1).
         * @details Returns the cached capture when the frame region around the
         *          cursor didn't change; runs the full analysis otherwise.
         *          Expected ~60-70% latency cut on static scenes.
         */
        FovealCapture analyze_cached(const cv::Mat& frame, int cursor_x, int cursor_y);

        /// Access cache stats (hit rate, global change ratio for proactivity).
        const FrameCache& get_frame_cache() const { return frame_cache; }
        FrameCache& get_frame_cache() { return frame_cache; }

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
