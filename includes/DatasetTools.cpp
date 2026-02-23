#include "../includes/DatasetTools.hpp"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <random>
#include <numeric>

namespace dataset_tools {

    // ============ CoordinateNormalizer ============

    std::pair<float, float> CoordinateNormalizer::normalize(int x, int y) const {
        float x_norm = static_cast<float>(x) / resolution.width;
        float y_norm = static_cast<float>(y) / resolution.height;
        
        // Clamp to [0, 1]
        x_norm = std::max(0.0f, std::min(1.0f, x_norm));
        y_norm = std::max(0.0f, std::min(1.0f, y_norm));
        
        return {x_norm, y_norm};
    }

    std::pair<int, int> CoordinateNormalizer::denormalize(float x_norm, float y_norm) const {
        int x = static_cast<int>(x_norm * resolution.width);
        int y = static_cast<int>(y_norm * resolution.height);
        
        // Clamp to screen bounds
        x = std::max(0, std::min(x, resolution.width - 1));
        y = std::max(0, std::min(y, resolution.height - 1));
        
        return {x, y};
    }

    void CoordinateNormalizer::set_resolution(int width, int height) {
        resolution.width = width;
        resolution.height = height;
    }

    // ============ DataAugmenter ============

    DataAugmenter::DataAugmenter()
        : brightness_factor(1.0f), contrast_factor(1.0f), 
          noise_stddev(5.0f), color_jitter(10.0f), coord_jitter_px(2.0f)
    {
    }

    int DataAugmenter::apply_brightness(int pixel_value) const {
        int result = static_cast<int>(pixel_value * brightness_factor);
        return std::max(0, std::min(255, result));
    }

    int DataAugmenter::apply_contrast(int pixel_value) const {
        int midpoint = 128;
        int delta = pixel_value - midpoint;
        int result = midpoint + static_cast<int>(delta * contrast_factor);
        return std::max(0, std::min(255, result));
    }

    int DataAugmenter::apply_coord_jitter(int coord) const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::normal_distribution<> dis(0.0, coord_jitter_px);
        
        int jitter = static_cast<int>(dis(gen));
        return coord + jitter;
    }

    std::vector<int> DataAugmenter::augment_color_tokens(const std::vector<int>& original_tokens) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(-5, 5);
        
        std::vector<int> augmented = original_tokens;
        for (auto& token : augmented) {
            int noise = dis(gen);
            token = std::max(0, std::min(9, token + noise));
        }
        return augmented;
    }

    json DataAugmenter::augment_entry(const json& original_entry) {
        json augmented = original_entry;
        
        // Augment coordinates if present
        if (augmented.contains("interaction") && augmented["interaction"].is_object()) {
            auto& interaction = augmented["interaction"];
            
            if (interaction.contains("cursor_global") && interaction["cursor_global"].is_array()) {
                int x = interaction["cursor_global"][0];
                int y = interaction["cursor_global"][1];
                
                interaction["cursor_global"] = json::array({
                    apply_coord_jitter(x),
                    apply_coord_jitter(y)
                });
            }
        }
        
        // Augment color tokens if present
        if (augmented.contains("visual_left") && augmented["visual_left"].is_array()) {
            std::vector<int> tokens = augmented["visual_left"];
            augmented["visual_left"] = augment_color_tokens(tokens);
        }
        if (augmented.contains("visual_right") && augmented["visual_right"].is_array()) {
            std::vector<int> tokens = augmented["visual_right"];
            augmented["visual_right"] = augment_color_tokens(tokens);
        }
        
        return augmented;
    }

    // ============ ActionLabelizer ============

    ActionLabel ActionLabelizer::label_from_events(const json& input_events) {
        if (!input_events.is_object()) {
            return ActionLabel();  // Default IDLE
        }
        
        // Check for recent keyboard events
        if (input_events.contains("recent_key_events")) {
            ActionLabel typing_label = detect_typing(input_events);
            if (typing_label.action_type != ActionLabel::IDLE) {
                return typing_label;
            }
        }
        
        // Check for mouse click
        if (input_events.contains("recent_mouse_events")) {
            ActionLabel click_label = detect_click(input_events);
            if (click_label.action_type != ActionLabel::IDLE) {
                return click_label;
            }
            
            // Check for drag
            ActionLabel drag_label = detect_drag(input_events);
            if (drag_label.action_type != ActionLabel::IDLE) {
                return drag_label;
            }
        }
        
        return ActionLabel();  // Default to IDLE
    }

    ActionLabel ActionLabelizer::detect_click(const json& recent_events) {
        ActionLabel label;
        
        if (!recent_events.contains("recent_mouse_events")) {
            return label;
        }
        
        auto mouse_events = recent_events["recent_mouse_events"];
        if (!mouse_events.is_array() || mouse_events.empty()) {
            return label;
        }
        
        for (const auto& event : mouse_events) {
            if (!event.is_object()) continue;
            
            if (event.contains("event_type") && event["event_type"] == "click") {
                label.action_type = ActionLabel::CLICK;
                label.confidence = 0.95f;
                label.description = "Click detected";
                
                if (event.contains("click_type")) {
                    std::string click_type = event["click_type"];
                    if (click_type == "double") {
                        label.action_type = ActionLabel::DOUBLE_CLICK;
                        label.description = "Double-click detected";
                    }
                }
                
                if (event.contains("button") && event["button"] == "right") {
                    label.action_type = ActionLabel::RIGHT_CLICK;
                    label.description = "Right-click detected";
                }
                
                if (event.contains("timestamp_ms")) {
                    label.timestamp_ms = event["timestamp_ms"];
                }
                
                return label;
            }
        }
        
        return label;
    }

    ActionLabel ActionLabelizer::detect_typing(const json& recent_events) {
        ActionLabel label;
        
        if (!recent_events.contains("recent_key_events")) {
            return label;
        }
        
        auto key_events = recent_events["recent_key_events"];
        if (!key_events.is_array() || key_events.empty()) {
            return label;
        }
        
        // If there are recent non-modifier key presses, mark as typing
        int key_press_count = 0;
        for (const auto& event : key_events) {
            if (!event.is_object()) continue;
            
            if (event.contains("event_type") && event["event_type"] == "press") {
                if (event.contains("key_name")) {
                    std::string key = event["key_name"];
                    // Exclude modifier keys
                    if (key != "shift" && key != "control" && key != "alt" && key != "super") {
                        key_press_count++;
                    }
                }
            }
        }
        
        if (key_press_count > 0) {
            label.action_type = ActionLabel::TYPING;
            label.confidence = 0.9f;
            label.description = "Typing activity detected";
        }
        
        return label;
    }

    ActionLabel ActionLabelizer::detect_drag(const json& recent_events) {
        ActionLabel label;
        
        if (!recent_events.contains("recent_mouse_events")) {
            return label;
        }
        
        auto mouse_events = recent_events["recent_mouse_events"];
        if (!mouse_events.is_array()) {
            return label;
        }
        
        // Look for CLICK followed by MOVE with high velocity
        bool button_held = false;
        int total_distance = 0;
        
        for (const auto& event : mouse_events) {
            if (!event.is_object()) continue;
            
            if (event.contains("event_type")) {
                std::string type = event["event_type"];
                
                if (type == "click") {
                    button_held = true;
                } else if (type == "release") {
                    button_held = false;
                } else if (type == "move" && button_held) {
                    if (event.contains("delta_x") && event.contains("delta_y")) {
                        int dx = event["delta_x"];
                        int dy = event["delta_y"];
                        total_distance += std::abs(dx) + std::abs(dy);
                    }
                }
            }
        }
        
        if (total_distance > DRAG_DISTANCE_THRESHOLD) {
            label.action_type = ActionLabel::DRAG;
            label.confidence = 0.85f;
            label.description = "Drag/drag-and-drop detected";
        }
        
        return label;
    }

    void ActionLabelizer::label_dataset_file(const std::string& input_jsonl,
                                             const std::string& output_jsonl)
    {
        std::ifstream infile(input_jsonl);
        std::ofstream outfile(output_jsonl);
        
        if (!infile.is_open() || !outfile.is_open()) {
            return;
        }
        
        std::string line;
        while (std::getline(infile, line)) {
            try {
                json entry = json::parse(line);
                
                // Add action label
                ActionLabel label = label_from_events(entry.value("interaction", json::object()));
                entry["action_label"] = label.to_json();
                
                outfile << entry.dump() << "\n";
            } catch (const std::exception& e) {
                // Skip malformed entries
            }
        }
        
        infile.close();
        outfile.close();
    }

    // ============ DatasetValidator ============

    DatasetValidator::DatasetStats DatasetValidator::analyze_file(const std::string& jsonl_path) {
        DatasetStats stats{};
        stats.total_samples = 0;
        stats.samples_with_action = 0;
        stats.samples_idle = 0;
        stats.action_ratio = 0.0f;
        stats.mean_cursor_x = stats.mean_cursor_y = 0.0f;
        stats.cursor_x_std = stats.cursor_y_std = 0.0f;
        stats.count_clicks = stats.count_typing = stats.count_scrolls = 0;
        
        std::ifstream file(jsonl_path);
        if (!file.is_open()) {
            return stats;
        }
        
        std::vector<float> cursor_xs, cursor_ys;
        std::string line;
        
        while (std::getline(file, line)) {
            try {
                json entry = json::parse(line);
                stats.total_samples++;
                
                // Extract cursor position
                if (entry.contains("interaction") && entry["interaction"].contains("cursor_global")) {
                    auto cursor = entry["interaction"]["cursor_global"];
                    if (cursor.is_array() && cursor.size() >= 2) {
                        cursor_xs.push_back(cursor[0]);
                        cursor_ys.push_back(cursor[1]);
                    }
                }
                
                // Count actions
                if (entry.contains("action_label")) {
                    auto label = entry["action_label"];
                    if (label.contains("action_type")) {
                        std::string action = label["action_type"];
                        if (action != "idle") {
                            stats.samples_with_action++;
                            
                            if (action == "click" || action == "double_click" || action == "right_click") {
                                stats.count_clicks++;
                            } else if (action == "typing") {
                                stats.count_typing++;
                            } else if (action == "scroll") {
                                stats.count_scrolls++;
                            }
                        } else {
                            stats.samples_idle++;
                        }
                    }
                }
            } catch (const std::exception&) {
                // Skip malformed lines
            }
        }
        
        // Compute statistics
        if (!cursor_xs.empty()) {
            stats.mean_cursor_x = std::accumulate(cursor_xs.begin(), cursor_xs.end(), 0.0f) / cursor_xs.size();
            stats.mean_cursor_y = std::accumulate(cursor_ys.begin(), cursor_ys.end(), 0.0f) / cursor_ys.size();
            
            // Compute standard deviation
            float sum_sq_x = 0, sum_sq_y = 0;
            for (size_t i = 0; i < cursor_xs.size(); ++i) {
                sum_sq_x += (cursor_xs[i] - stats.mean_cursor_x) * (cursor_xs[i] - stats.mean_cursor_x);
                sum_sq_y += (cursor_ys[i] - stats.mean_cursor_y) * (cursor_ys[i] - stats.mean_cursor_y);
            }
            stats.cursor_x_std = std::sqrt(sum_sq_x / cursor_xs.size());
            stats.cursor_y_std = std::sqrt(sum_sq_y / cursor_ys.size());
        }
        
        if (stats.total_samples > 0) {
            stats.action_ratio = static_cast<float>(stats.samples_with_action) / stats.total_samples;
        }
        
        file.close();
        return stats;
    }

    std::vector<int> DatasetValidator::find_near_duplicates(const std::string& jsonl_path,
                                                           float similarity_threshold)
    {
        std::vector<int> duplicates;
        // Placeholder: More sophisticated duplicate detection could be implemented
        return duplicates;
    }

    void DatasetValidator::shuffle_file(const std::string& input_path,
                                       const std::string& output_path)
    {
        std::vector<std::string> lines;
        std::ifstream infile(input_path);
        std::string line;
        
        while (std::getline(infile, line)) {
            lines.push_back(line);
        }
        infile.close();
        
        // Shuffle
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(lines.begin(), lines.end(), g);
        
        // Write
        std::ofstream outfile(output_path);
        for (const auto& l : lines) {
            outfile << l << "\n";
        }
        outfile.close();
    }

    void DatasetValidator::split_dataset(const std::string& input_path,
                                        const std::string& train_path,
                                        const std::string& test_path,
                                        float train_ratio)
    {
        std::vector<std::string> lines;
        std::ifstream infile(input_path);
        std::string line;
        
        while (std::getline(infile, line)) {
            lines.push_back(line);
        }
        infile.close();
        
        // Shuffle first
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(lines.begin(), lines.end(), g);
        
        // Split
        int split_idx = static_cast<int>(lines.size() * train_ratio);
        
        std::ofstream train_file(train_path);
        for (int i = 0; i < split_idx && i < (int)lines.size(); ++i) {
            train_file << lines[i] << "\n";
        }
        train_file.close();
        
        std::ofstream test_file(test_path);
        for (int i = split_idx; i < (int)lines.size(); ++i) {
            test_file << lines[i] << "\n";
        }
        test_file.close();
    }
}
