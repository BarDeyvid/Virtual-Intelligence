#pragma once

#include <nlohmann/json.hpp>
#include <chrono>
#include <vector>
#include <queue>
#include <memory>
#include <cstdint>
#include <mutex>

using json = nlohmann::json;

/**
 * @namespace input_capture
 * @brief Input event logging and tracking for Hyprland
 * 
 * Features:
 * - Modifier key state tracking (Shift, Ctrl, Alt, Super)
 * - Click event differentiation (left, right, double, scroll)
 * - Mouse velocity computation
 * - Timestamp logging
 */
namespace input_capture {

    /**
     * @enum MouseButton
     */
    enum class MouseButton {
        LEFT = 1,
        RIGHT = 3,
        MIDDLE = 2,
        SCROLL_UP = 4,
        SCROLL_DOWN = 5,
        UNKNOWN = 0
    };

    /**
     * @enum ClickType
     */
    enum class ClickType {
        SINGLE,
        DOUBLE,
        TRIPLE,
        NONE
    };

    /**
     * @struct ModifierState
     * @brief Current state of modifier keys
     */
    struct ModifierState {
        bool shift;
        bool ctrl;
        bool alt;
        bool super;  // Windows/Cmd key
        
        ModifierState() : shift(false), ctrl(false), alt(false), super(false) {}
        
        json to_json() const {
            return json{
                {"shift", shift},
                {"ctrl", ctrl},
                {"alt", alt},
                {"super", super}
            };
        }
    };

    /**
     * @struct MouseEvent
     * @brief Single mouse event (move, click, scroll)
     */
    struct MouseEvent {
        enum Type {
            MOVE,
            CLICK,
            RELEASE,
            SCROLL
        };
        
        Type event_type;
        MouseButton button;
        ClickType click_type;
        int x, y;                           // Absolute position
        int delta_x, delta_y;               // Movement since last event
        float velocity;                     // Pixels per second
        ModifierState modifiers;
        int64_t timestamp_ms;               // Unix milliseconds
        
        MouseEvent() : event_type(MOVE), button(MouseButton::UNKNOWN), 
                      click_type(ClickType::NONE), x(0), y(0), 
                      delta_x(0), delta_y(0), velocity(0.0), timestamp_ms(0) {}
        
        json to_json() const {
            std::string type_str;
            switch (event_type) {
                case MOVE: type_str = "move"; break;
                case CLICK: type_str = "click"; break;
                case RELEASE: type_str = "release"; break;
                case SCROLL: type_str = "scroll"; break;
            }
            
            std::string button_str;
            switch (button) {
                case MouseButton::LEFT: button_str = "left"; break;
                case MouseButton::RIGHT: button_str = "right"; break;
                case MouseButton::MIDDLE: button_str = "middle"; break;
                case MouseButton::SCROLL_UP: button_str = "scroll_up"; break;
                case MouseButton::SCROLL_DOWN: button_str = "scroll_down"; break;
                default: button_str = "unknown"; break;
            }
            
            std::string click_str;
            switch (click_type) {
                case ClickType::SINGLE: click_str = "single"; break;
                case ClickType::DOUBLE: click_str = "double"; break;
                case ClickType::TRIPLE: click_str = "triple"; break;
                default: click_str = "none"; break;
            }
            
            return json{
                {"event_type", type_str},
                {"button", button_str},
                {"click_type", click_str},
                {"x", x},
                {"y", y},
                {"delta_x", delta_x},
                {"delta_y", delta_y},
                {"velocity", velocity},
                {"modifiers", modifiers.to_json()},
                {"timestamp_ms", timestamp_ms}
            };
        }
    };

    /**
     * @struct KeyEvent
     * @brief Single keyboard event
     */
    struct KeyEvent {
        enum Type {
            PRESS,
            RELEASE,
            REPEAT
        };
        
        Type event_type;
        uint32_t keycode;                   // XKB keycode
        std::string key_name;               // Symbolic name (a, b, Return, etc)
        ModifierState modifiers;
        int64_t timestamp_ms;               // Unix milliseconds
        
        KeyEvent() : event_type(PRESS), keycode(0), timestamp_ms(0) {}
        
        json to_json() const {
            std::string type_str;
            switch (event_type) {
                case PRESS: type_str = "press"; break;
                case RELEASE: type_str = "release"; break;
                case REPEAT: type_str = "repeat"; break;
            }
            
            return json{
                {"event_type", type_str},
                {"keycode", keycode},
                {"key_name", key_name},
                {"modifiers", modifiers.to_json()},
                {"timestamp_ms", timestamp_ms}
            };
        }
    };

    /**
     * @class InputCapture
     * @brief Captures mouse and keyboard input events
     * 
     * Note: This is a polling-based implementation that reads from /dev/input
     * or uses Wayland event capture. Full libinput integration can be added later.
     */
    class InputCapture {
    private:
        std::vector<MouseEvent> mouse_history;
        std::vector<KeyEvent> key_history;
        // Buffered key events: consumed once per dataset capture cycle
        mutable std::vector<KeyEvent> buffered_key_events;
        mutable std::mutex buffer_mutex;
        ModifierState current_modifiers;
        
        int last_mouse_x, last_mouse_y;
        int64_t last_mouse_time_ms;
        int64_t last_click_time_ms;
        int click_count;
        MouseButton last_click_button;
        
        // Internal: Get current time in milliseconds
        static int64_t get_timestamp_ms();
        
        // Internal: Compute mouse velocity
        static float compute_velocity(int dx, int dy, int64_t dt_ms);
        
        // Internal: Read modifier state from keyboard (polling)
        void poll_modifiers();
        
        // Internal: Detect double/triple clicks
        ClickType detect_click_type(MouseButton btn, int64_t now_ms);

    public:
        InputCapture();
        ~InputCapture() = default;

        /**
         * @brief Record a mouse move event
         */
        void record_mouse_move(int x, int y);

        /**
         * @brief Record a mouse click
         */
        void record_mouse_click(MouseButton button, int x, int y, ClickType click_type);

        /**
         * @brief Record a mouse release
         */
        void record_mouse_release(MouseButton button, int x, int y);

        /**
         * @brief Record a scroll event (wheel/trackpad)
         */
        void record_scroll(int x, int y, int direction);  // direction: 1=up, -1=down

        /**
         * @brief Record a keyboard press
         */
        void record_key_press(uint32_t keycode, const std::string& key_name);

        /**
         * @brief Record a keyboard release
         */
        void record_key_release(uint32_t keycode, const std::string& key_name);

        /**
         * @brief Manually update modifier state
         */
        void set_modifiers(bool shift, bool ctrl, bool alt, bool super);

        /**
         * @brief Get modifier state
         */
        ModifierState get_modifiers() const { return current_modifiers; }

        /**
         * @brief Get recent mouse events (last N events)
         */
        std::vector<MouseEvent> get_recent_mouse_events(int count = 10) const;

        /**
         * @brief Get recent key events (last N events)
         */
        std::vector<KeyEvent> get_recent_key_events(int count = 10) const;

        /**
         * @brief Get all recorded events
         */
        const std::vector<MouseEvent>& get_all_mouse_events() const { return mouse_history; }
        const std::vector<KeyEvent>& get_all_key_events() const { return key_history; }

        /**
         * @brief Clear history (for privacy or memory management)
         */
        void clear_history();

        /**
         * @brief Get last mouse position
         */
        void get_last_mouse_pos(int& x, int& y) const;

        /**
         * @brief Serialize events to JSON (for dataset)
         */
        json to_json() const;
    };
}
