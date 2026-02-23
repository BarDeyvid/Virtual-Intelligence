#include "../includes/InputCapture.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>

namespace input_capture {

    int64_t InputCapture::get_timestamp_ms() {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }

    float InputCapture::compute_velocity(int dx, int dy, int64_t dt_ms) {
        if (dt_ms <= 0) return 0.0f;
        
        float distance = std::sqrt(dx * dx + dy * dy);
        float dt_sec = dt_ms / 1000.0f;
        return distance / dt_sec;  // pixels per second
    }

    void InputCapture::poll_modifiers() {
        // TODO: Implement polling from libinput or /dev/input
        // For now, modifiers are set externally via set_modifiers()
    }

    ClickType InputCapture::detect_click_type(MouseButton btn, int64_t now_ms) {
        const int64_t DOUBLE_CLICK_THRESHOLD_MS = 300;
        const int64_t TRIPLE_CLICK_THRESHOLD_MS = 600;
        
        if (last_click_button != btn || now_ms - last_click_time_ms > TRIPLE_CLICK_THRESHOLD_MS) {
            click_count = 1;
        } else {
            click_count++;
        }
        
        last_click_button = btn;
        last_click_time_ms = now_ms;
        
        if (click_count >= 3) return ClickType::TRIPLE;
        if (click_count >= 2) return ClickType::DOUBLE;
        return ClickType::SINGLE;
    }

    InputCapture::InputCapture()
        : last_mouse_x(0), last_mouse_y(0), last_mouse_time_ms(0),
          last_click_time_ms(0), click_count(0), last_click_button(MouseButton::UNKNOWN)
    {
        last_mouse_time_ms = get_timestamp_ms();
    }

    void InputCapture::record_mouse_move(int x, int y) {
        int64_t now_ms = get_timestamp_ms();
        int dx = x - last_mouse_x;
        int dy = y - last_mouse_y;
        int64_t dt_ms = now_ms - last_mouse_time_ms;
        
        MouseEvent event;
        event.event_type = MouseEvent::MOVE;
        event.x = x;
        event.y = y;
        event.delta_x = dx;
        event.delta_y = dy;
        event.velocity = compute_velocity(dx, dy, dt_ms);
        event.modifiers = current_modifiers;
        event.timestamp_ms = now_ms;
        
        mouse_history.push_back(event);
        
        last_mouse_x = x;
        last_mouse_y = y;
        last_mouse_time_ms = now_ms;
    }

    void InputCapture::record_mouse_click(MouseButton button, int x, int y, ClickType click_type) {
        int64_t now_ms = get_timestamp_ms();
        
        MouseEvent event;
        event.event_type = MouseEvent::CLICK;
        event.button = button;
        event.click_type = (click_type == ClickType::NONE) ? detect_click_type(button, now_ms) : click_type;
        event.x = x;
        event.y = y;
        event.modifiers = current_modifiers;
        event.timestamp_ms = now_ms;
        
        mouse_history.push_back(event);
        
        last_mouse_x = x;
        last_mouse_y = y;
        last_mouse_time_ms = now_ms;
    }

    void InputCapture::record_mouse_release(MouseButton button, int x, int y) {
        int64_t now_ms = get_timestamp_ms();
        
        MouseEvent event;
        event.event_type = MouseEvent::RELEASE;
        event.button = button;
        event.x = x;
        event.y = y;
        event.modifiers = current_modifiers;
        event.timestamp_ms = now_ms;
        
        mouse_history.push_back(event);
        
        last_mouse_x = x;
        last_mouse_y = y;
        last_mouse_time_ms = now_ms;
    }

    void InputCapture::record_scroll(int x, int y, int direction) {
        int64_t now_ms = get_timestamp_ms();
        
        MouseEvent event;
        event.event_type = MouseEvent::SCROLL;
        event.button = (direction > 0) ? MouseButton::SCROLL_UP : MouseButton::SCROLL_DOWN;
        event.x = x;
        event.y = y;
        event.modifiers = current_modifiers;
        event.timestamp_ms = now_ms;
        event.delta_y = direction;  // Store scroll direction
        
        mouse_history.push_back(event);
        
        last_mouse_x = x;
        last_mouse_y = y;
        last_mouse_time_ms = now_ms;
    }

    void InputCapture::record_key_press(uint32_t keycode, const std::string& key_name) {
        int64_t now_ms = get_timestamp_ms();
        
        KeyEvent event;
        event.event_type = KeyEvent::PRESS;
        event.keycode = keycode;
        event.key_name = key_name;
        event.modifiers = current_modifiers;
        event.timestamp_ms = now_ms;
        
        key_history.push_back(event);
        
        // Update modifiers based on the key pressed
        if (key_name == "shift" || key_name == "Shift_L" || key_name == "Shift_R") {
            current_modifiers.shift = true;
        } else if (key_name == "control" || key_name == "Control_L" || key_name == "Control_R") {
            current_modifiers.ctrl = true;
        } else if (key_name == "alt" || key_name == "Alt_L" || key_name == "Alt_R") {
            current_modifiers.alt = true;
        } else if (key_name == "super" || key_name == "Super_L" || key_name == "Super_R") {
            current_modifiers.super = true;
        }
    }

    void InputCapture::record_key_release(uint32_t keycode, const std::string& key_name) {
        int64_t now_ms = get_timestamp_ms();
        
        KeyEvent event;
        event.event_type = KeyEvent::RELEASE;
        event.keycode = keycode;
        event.key_name = key_name;
        event.modifiers = current_modifiers;
        event.timestamp_ms = now_ms;
        
        key_history.push_back(event);
        
        // Update modifiers based on the key released
        if (key_name == "shift" || key_name == "Shift_L" || key_name == "Shift_R") {
            current_modifiers.shift = false;
        } else if (key_name == "control" || key_name == "Control_L" || key_name == "Control_R") {
            current_modifiers.ctrl = false;
        } else if (key_name == "alt" || key_name == "Alt_L" || key_name == "Alt_R") {
            current_modifiers.alt = false;
        } else if (key_name == "super" || key_name == "Super_L" || key_name == "Super_R") {
            current_modifiers.super = false;
        }
    }

    void InputCapture::set_modifiers(bool shift, bool ctrl, bool alt, bool super) {
        current_modifiers.shift = shift;
        current_modifiers.ctrl = ctrl;
        current_modifiers.alt = alt;
        current_modifiers.super = super;
    }

    std::vector<MouseEvent> InputCapture::get_recent_mouse_events(int count) const {
        std::vector<MouseEvent> recent;
        int start_idx = std::max(0, (int)mouse_history.size() - count);
        recent.insert(recent.end(), mouse_history.begin() + start_idx, mouse_history.end());
        return recent;
    }

    std::vector<KeyEvent> InputCapture::get_recent_key_events(int count) const {
        std::vector<KeyEvent> recent;
        int start_idx = std::max(0, (int)key_history.size() - count);
        recent.insert(recent.end(), key_history.begin() + start_idx, key_history.end());
        return recent;
    }

    void InputCapture::clear_history() {
        mouse_history.clear();
        key_history.clear();
    }

    void InputCapture::get_last_mouse_pos(int& x, int& y) const {
        x = last_mouse_x;
        y = last_mouse_y;
    }

    json InputCapture::to_json() const {
        json result;
        
        result["modifiers"] = current_modifiers.to_json();
        
        // Recent mouse events (last 5)
        json mouse_array = json::array();
        for (const auto& event : get_recent_mouse_events(5)) {
            mouse_array.push_back(event.to_json());
        }
        result["recent_mouse_events"] = mouse_array;
        
        // Recent key events (last 5)
        json key_array = json::array();
        for (const auto& event : get_recent_key_events(5)) {
            key_array.push_back(event.to_json());
        }
        result["recent_key_events"] = key_array;
        
        // Statistics
        result["total_mouse_events"] = (int)mouse_history.size();
        result["total_key_events"] = (int)key_history.size();
        
        return result;
    }
}
