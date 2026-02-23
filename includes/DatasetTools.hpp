#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

using json = nlohmann::json;

/**
 * @namespace dataset_tools
 * @brief Dataset preparation: normalization, augmentation, labeling
 */
namespace dataset_tools {

    /**
     * @struct ScreenResolution
     * @brief Monitor resolution for coordinate normalization
     */
    struct ScreenResolution {
        int width, height;
        
        ScreenResolution(int w = 1920, int h = 1080) : width(w), height(h) {}
    };

    /**
     * @struct ActionLabel
     * @brief Semantic label for an action/interaction
     */
    struct ActionLabel {
        enum Type {
            IDLE,           // No specific action
            MOUSE_MOVE,     // Movement only
            CLICK,          // Any click
            DOUBLE_CLICK,
            RIGHT_CLICK,
            SCROLL,
            TYPING,         // Keyboard input detected
            DRAG,           // Mouse drag
            UNKNOWN
        };
        
        Type action_type;
        std::string description;
        double confidence;  // 0.0 - 1.0
        int64_t timestamp_ms;
        
        ActionLabel() : action_type(IDLE), confidence(1.0), timestamp_ms(0) {}
        
        json to_json() const {
            std::string type_str;
            switch (action_type) {
                case IDLE: type_str = "idle"; break;
                case MOUSE_MOVE: type_str = "mouse_move"; break;
                case CLICK: type_str = "click"; break;
                case DOUBLE_CLICK: type_str = "double_click"; break;
                case RIGHT_CLICK: type_str = "right_click"; break;
                case SCROLL: type_str = "scroll"; break;
                case TYPING: type_str = "typing"; break;
                case DRAG: type_str = "drag"; break;
                default: type_str = "unknown"; break;
            }
            
            return json{
                {"action_type", type_str},
                {"description", description},
                {"confidence", confidence},
                {"timestamp_ms", timestamp_ms}
            };
        }
    };

    /**
     * @class CoordinateNormalizer
     * @brief Normalize pixel coordinates to [0, 1] range
     */
    class CoordinateNormalizer {
    private:
        ScreenResolution resolution;
        
    public:
        /**
         * @brief Constructor
         * @param width Screen width in pixels
         * @param height Screen height in pixels
         */
        CoordinateNormalizer(int width, int height) 
            : resolution(width, height) {}

        /**
         * @brief Normalize absolute pixel coordinates to [0, 1]
         * @return {x_norm, y_norm} where values are in [0, 1]
         */
        std::pair<float, float> normalize(int x, int y) const;

        /**
         * @brief Inverse: convert normalized [0, 1] back to pixels
         */
        std::pair<int, int> denormalize(float x_norm, float y_norm) const;

        /**
         * @brief Update resolution (for multi-monitor scenarios)
         */
        void set_resolution(int width, int height);

        /**
         * @brief Get current resolution
         */
        ScreenResolution get_resolution() const { return resolution; }
    };

    /**
     * @class DataAugmenter
     * @brief Apply augmentation transformations to dataset entries
     * 
     * Augmentations:
     * - Brightness/contrast variation
     * - Slight color shifts
     * - Gaussian noise
     * - Coordinate jitter
     */
    class DataAugmenter {
    private:
        float brightness_factor;    // [0.8, 1.2]
        float contrast_factor;      // [0.8, 1.2]
        float noise_stddev;         // Gaussian noise std
        float color_jitter;         // Max change in each channel
        float coord_jitter_px;      // Max pixel offset
        
    public:
        DataAugmenter();
        ~DataAugmenter() = default;

        /**
         * @brief Configure augmentation parameters
         */
        void set_brightness_factor(float f) { brightness_factor = f; }
        void set_contrast_factor(float f) { contrast_factor = f; }
        void set_noise_stddev(float n) { noise_stddev = n; }
        void set_color_jitter(float j) { color_jitter = j; }
        void set_coord_jitter(float j) { coord_jitter_px = j; }

        /**
         * @brief Augment a dataset entry (JSON)
         * Creates a new entry with applied augmentations
         */
        json augment_entry(const json& original_entry);

        /**
         * @brief Apply brightness change to a value
         */
        int apply_brightness(int pixel_value) const;

        /**
         * @brief Apply contrast change to a value (around 128 midpoint)
         */
        int apply_contrast(int pixel_value) const;

        /**
         * @brief Apply Gaussian noise to a coordinate
         */
        int apply_coord_jitter(int coord) const;

        /**
         * @brief Augment color tokens (simple: add noise)
         */
        std::vector<int> augment_color_tokens(const std::vector<int>& original_tokens);
    };

    /**
     * @class ActionLabelizer
     * @brief Analyze event sequences and assign action labels
     */
    class ActionLabelizer {
    private:
        const int CLICK_VELOCITY_THRESHOLD = 50;      // px/s
        const int DOUBLE_CLICK_TIME_MS = 300;
        const int DRAG_DISTANCE_THRESHOLD = 50;       // px
        
    public:
        ActionLabelizer() = default;
        ~ActionLabelizer() = default;

        /**
         * @brief Analyze input events and assign action label
         * @param input_events Recent mouse/key events JSON
         * @return ActionLabel for this moment
         */
        ActionLabel label_from_events(const json& input_events);

        /**
         * @brief Batch labeling: process dataset and add labels
         */
        void label_dataset_file(const std::string& input_jsonl, 
                               const std::string& output_jsonl);

        /**
         * @brief Detect click from position change
         */
        ActionLabel detect_click(const json& recent_events);

        /**
         * @brief Detect typing activity
         */
        ActionLabel detect_typing(const json& recent_events);

        /**
         * @brief Detect drag (sustained movement with button held)
         */
        ActionLabel detect_drag(const json& recent_events);
    };

    /**
     * @class DatasetValidator
     * @brief Validate and analyze dataset characteristics
     */
    class DatasetValidator {
    public:
        struct DatasetStats {
            int total_samples;
            int samples_with_action;  // Non-idle samples
            int samples_idle;
            float action_ratio;       // % with meaningful actions
            
            // Coordinate distribution
            float mean_cursor_x, mean_cursor_y;
            float cursor_x_std, cursor_y_std;
            
            // Action type counts
            int count_clicks;
            int count_typing;
            int count_scrolls;
        };

        /**
         * @brief Analyze JSONL dataset file
         */
        static DatasetStats analyze_file(const std::string& jsonl_path);

        /**
         * @brief Check for duplicates and anomalies
         */
        static std::vector<int> find_near_duplicates(const std::string& jsonl_path, 
                                                     float similarity_threshold = 0.95f);

        /**
         * @brief Shuffle dataset for training
         */
        static void shuffle_file(const std::string& input_path, 
                                const std::string& output_path);

        /**
         * @brief Split into train/test
         */
        static void split_dataset(const std::string& input_path, 
                                 const std::string& train_path,
                                 const std::string& test_path,
                                 float train_ratio = 0.8f);
    };
}
