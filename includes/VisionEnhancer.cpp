#include "../includes/VisionEnhancer.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace vision_foveal {

    // =========================================================================
    // FrameCache (Phase 3.1)
    // =========================================================================

    // Fração de pixels de 'a' que diferem de 'b' além do threshold de intensidade.
    static double changed_pixel_ratio(const cv::Mat& a, const cv::Mat& b, int pixel_threshold) {
        if (a.empty() || b.empty() || a.size() != b.size()) return 1.0;
        cv::Mat diff;
        cv::absdiff(a, b, diff);
        cv::Mat changed;
        cv::threshold(diff, changed, pixel_threshold, 255, cv::THRESH_BINARY);
        return static_cast<double>(cv::countNonZero(changed)) / (a.rows * a.cols);
    }

    static cv::Mat to_gray(const cv::Mat& src) {
        if (src.channels() == 1) return src;
        cv::Mat gray;
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
        return gray;
    }

    FrameCache::Decision FrameCache::evaluate(const cv::Mat& frame, int cursor_x, int cursor_y) {
        Decision decision;
        if (frame.empty()) return decision;

        // 1. Assinatura global (frame inteiro, reduzido): mede o quanto a CENA
        //    mudou desde o último frame — alimenta o gatilho de proatividade,
        //    independente de hit/miss do crop.
        //    Ordem importa para o custo: subamostra PRIMEIRO (NEAREST lê só
        //    signature_size² pixels) e converte pra cinza só o thumbnail.
        //    Fazer cvtColor/INTER_AREA no frame inteiro custava mais que a
        //    própria análise foveal.
        cv::Mat small;
        cv::resize(frame, small,
                   cv::Size(config.signature_size, config.signature_size), 0, 0, cv::INTER_NEAREST);
        cv::Mat signature = to_gray(small);
        if (!prev_signature.empty()) {
            last_global_change = changed_pixel_ratio(signature, prev_signature,
                                                     config.pixel_diff_threshold);
        }
        decision.global_change_ratio = last_global_change;
        prev_signature = signature;

        if (!config.enabled || !has_cache) {
            ++miss_count;
            return decision;
        }

        // 2. Cursor saiu da célula? O crop mudou de lugar, cache inválido.
        //    (Jitter de mouse dentro da célula não invalida.)
        if (std::abs(cursor_x - cached_cursor_x) > config.cursor_cell_px ||
            std::abs(cursor_y - cached_cursor_y) > config.cursor_cell_px) {
            ++miss_count;
            return decision;
        }

        // 3. Diff parcial: só a região do crop cacheado importa. Mudança fora
        //    dela não invalida o resultado foveal.
        cv::Rect bounded = cached_crop_rect & cv::Rect(0, 0, frame.cols, frame.rows);
        if (bounded.width <= 0 || bounded.height <= 0 ||
            bounded.size() != cached_crop_gray.size()) {
            ++miss_count;
            return decision;
        }

        cv::Mat current_crop_gray = to_gray(frame(bounded));
        decision.crop_change_ratio = changed_pixel_ratio(current_crop_gray, cached_crop_gray,
                                                         config.pixel_diff_threshold);

        if (decision.crop_change_ratio <= config.crop_change_threshold) {
            decision.hit = true;
            ++hit_count;
        } else {
            ++miss_count;
        }
        return decision;
    }

    void FrameCache::store(const cv::Mat& frame, const FovealCapture& result) {
        cached = result;
        cached_cursor_x = result.cursor_screen_x;
        cached_cursor_y = result.cursor_screen_y;
        cached_crop_rect = cv::Rect(result.crop_x, result.crop_y,
                                    result.crop_width, result.crop_height);

        cv::Rect bounded = cached_crop_rect & cv::Rect(0, 0, frame.cols, frame.rows);
        if (bounded.width > 0 && bounded.height > 0) {
            cached_crop_gray = to_gray(frame(bounded)).clone();
            has_cache = true;
        } else {
            has_cache = false;
        }
    }

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

    FovealCapture FovealVision::analyze_cached(const cv::Mat& frame, int cursor_x, int cursor_y) {
        FrameCache::Decision decision = frame_cache.evaluate(frame, cursor_x, cursor_y);
        if (decision.hit) {
            return frame_cache.cached_result();
        }
        FovealCapture result = analyze(frame, cursor_x, cursor_y);
        frame_cache.store(frame, result);
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
