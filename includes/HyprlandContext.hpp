#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <memory>
#include <chrono>

using json = nlohmann::json;

/**
 * @brief HyprlandContext: Manages Hyprland window manager state
 * 
 * Provides:
 * - Window tree mapping with geometry
 * - Workspace state tracking
 * - Layer shell (panels, popups) detection
 * - Monitor configuration
 * - Cached hierarchical information
 */
namespace hyprland {

    /**
     * @struct Window
     * @brief Represents a window in the Hyprland hierarchy
     */
    struct Window {
        uint64_t address;           // Hyprland window pointer
        std::string title;
        std::string class_name;
        std::string role;
        int workspace_id;
        int monitor_id;
        int x, y, width, height;    // Position and size
        bool floating;
        bool fullscreen;
        bool pinned;
        int focus_order;            // Z-order
    };

    /**
     * @struct Workspace
     * @brief Represents a workspace state
     */
    struct Workspace {
        int id;
        std::string name;
        int active_monitor;
        int window_count;
        bool is_active;
        std::vector<uint64_t> window_addresses;
    };

    /**
     * @struct LayerSurface
     * @brief Represents a Wayland layer surface (panels, popups, etc.)
     */
    struct LayerSurface {
        std::string namespace_name;
        int layer;                  // 0:background, 1:bottom, 2:top, 3:overlay
        std::string geometry;       // Or parsed: x, y, width, height
        bool exclusive_zone;
        bool keyboard_interactive;
    };

    /**
     * @struct Monitor
     * @brief Represents a monitor configuration
     */
    struct Monitor {
        int id;
        std::string name;
        int x, y;                   // Offset in virtual space
        int width, height;          // Resolution
        float refresh_rate;
        bool enabled;
    };

    class HyprlandContext {
    private:
        std::map<uint64_t, Window> window_tree;                    // All windows by address
        std::map<int, Workspace> workspace_state;                  // Workspaces by ID
        std::vector<Monitor> monitors;                             // Monitor configuration
        std::vector<LayerSurface> layer_surfaces;                  // Layer shells
        
        uint64_t active_window_addr;                               // Current focused window
        int active_workspace_id;                                   // Current workspace
        int active_monitor_id;                                     // Current monitor
        int cursor_x, cursor_y;                                    // Cursor position
        
        std::chrono::time_point<std::chrono::steady_clock> last_update;
        
        // Internal helper: Execute Hyprland command and parse JSON
        json exec_hypr_json(const std::string& cmd);
        
        // Internal: Parse clients -j output
        void parse_window_tree(const json& clients_data);
        
        // Internal: Parse workspaces -j output
        void parse_workspaces(const json& workspaces_data);
        
        // Internal: Parse monitors -j output
        void parse_monitors(const json& monitors_data);
        
        // Internal: Parse layer surfaces
        void parse_layer_surfaces(const json& layers_data);

    public:
        HyprlandContext();
        ~HyprlandContext() = default;

        /**
         * @brief Update all Hyprland state in one call
         * Queries: clients, workspaces, activewindow, cursorpos, monitors, layers
         */
        void update_state();

        /**
         * @brief Get the active window
         */
        Window get_active_window() const;

        /**
         * @brief Get window by address
         */
        Window* get_window(uint64_t address);

        /**
         * @brief Get all windows
         */
        const std::map<uint64_t, Window>& get_all_windows() const;

        /**
         * @brief Get workspace state
         */
        const Workspace& get_workspace(int ws_id) const;

        /**
         * @brief Get all workspaces
         */
        const std::map<int, Workspace>& get_all_workspaces() const;

        /**
         * @brief Get monitor config
         */
        const std::vector<Monitor>& get_monitors() const;

        /**
         * @brief Get layer surfaces (panels, popups)
         */
        const std::vector<LayerSurface>& get_layer_surfaces() const;

        /**
         * @brief Get cursor position
         */
        void get_cursor_pos(int& x, int& y) const;

        /**
         * @brief Get active workspace ID
         */
        int get_active_workspace() const { return active_workspace_id; }

        /**
         * @brief Get active monitor ID
         */
        int get_active_monitor() const { return active_monitor_id; }

        /**
         * @brief Check if window is under cursor (hit test)
         */
        Window* window_at_cursor();

        /**
         * @brief Get window monitor assignment
         */
        Monitor* get_window_monitor(uint64_t window_addr);

        /**
         * @brief Serialize full context to JSON for dataset
         */
        json to_json() const;

        /**
         * @brief Time since last update (ms)
         */
        long get_update_age_ms() const;
    };
}
