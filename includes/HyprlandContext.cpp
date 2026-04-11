#include "../includes/HyprlandContext.hpp"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace hyprland {

    json HyprlandContext::exec_hypr_json(const std::string& cmd) {
        std::string full_cmd = "hyprctl " + cmd;
        char buffer[65536];
        std::string result;
        
        FILE* pipe = popen(full_cmd.c_str(), "r");
        if (!pipe) return json::object();
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        
        try {
            return json::parse(result);
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error in exec_hypr_json: " << e.what() << std::endl;
            std::cerr << "  Command: " << cmd << std::endl;
            std::cerr << "  Result: " << result.substr(0, 200) << "..." << std::endl;
            return json::object();
        }
    }

    void HyprlandContext::parse_window_tree(const json& clients_data) {
        window_tree.clear();
        
        if (!clients_data.is_array()) return;
        
        int focus_order = 0;
        for (const auto& client : clients_data) {
            if (!client.is_object()) continue;
            
            Window win;
            win.address = 0;
            win.title = "unknown";
            win.class_name = "unknown";
            win.role = "";
            win.workspace_id = -1;
            win.monitor_id = -1;
            win.x = win.y = 0;
            win.width = win.height = 0;
            win.floating = false;
            win.fullscreen = false;
            win.pinned = false;
            win.focus_order = focus_order++;
            
            // Parse fields from hyprctl clients -j
            if (client.contains("address") && client["address"].is_string()) {
                try {
                    std::string addr_str = client["address"];
                    // Hyprland uses 0x format
                    win.address = std::stoull(addr_str, nullptr, 16);
                } catch (...) {
                    continue;
                }
            } else {
                continue;
            }
            
            if (client.contains("title")) {
                win.title = client["title"].is_string() ? client["title"].get<std::string>() : "untitled";
            }
            
            if (client.contains("class")) {
                win.class_name = client["class"].is_string() ? client["class"].get<std::string>() : "?";
            }
            
            if (client.contains("role")) {
                win.role = client["role"].is_string() ? client["role"].get<std::string>() : "";
            }
            
            if (client.contains("workspace") && client["workspace"].is_object()) {
                auto ws_obj = client["workspace"];
                if (ws_obj.contains("id")) {
                    win.workspace_id = ws_obj["id"].is_number() ? ws_obj["id"].get<int>() : -1;
                }
            }
            
            if (client.contains("monitor")) {
                win.monitor_id = client["monitor"].is_number() ? client["monitor"].get<int>() : -1;
            }
            
            if (client.contains("at") && client["at"].is_array() && client["at"].size() >= 2) {
                win.x = client["at"][0].is_number() ? client["at"][0].get<int>() : 0;
                win.y = client["at"][1].is_number() ? client["at"][1].get<int>() : 0;
            }
            
            if (client.contains("size") && client["size"].is_array() && client["size"].size() >= 2) {
                win.width = client["size"][0].is_number() ? client["size"][0].get<int>() : 0;
                win.height = client["size"][1].is_number() ? client["size"][1].get<int>() : 0;
            }
            
            if (client.contains("floating")) {
                win.floating = client["floating"].is_boolean() ? client["floating"].get<bool>() : false;
            }
            
            if (client.contains("fullscreen")) {
                win.fullscreen = client["fullscreen"].is_boolean() ? client["fullscreen"].get<bool>() : false;
            }
            
            if (client.contains("pinned")) {
                win.pinned = client["pinned"].is_boolean() ? client["pinned"].get<bool>() : false;
            }
            
            window_tree[win.address] = win;
        }
    }

    void HyprlandContext::parse_workspaces(const json& workspaces_data) {
        workspace_state.clear();
        
        if (!workspaces_data.is_array()) return;
        
        for (const auto& ws : workspaces_data) {
            if (!ws.is_object()) continue;
            
            Workspace workspace;
            workspace.id = -1;
            workspace.name = "unknown";
            workspace.active_monitor = -1;
            workspace.window_count = 0;
            workspace.is_active = false;
            
            if (ws.contains("id")) {
                workspace.id = ws["id"].is_number() ? ws["id"].get<int>() : -1;
            } else {
                continue;
            }
            
            if (ws.contains("name")) {
                workspace.name = ws["name"].is_string() ? ws["name"].get<std::string>() : "ws_" + std::to_string(workspace.id);
            }
            
            if (ws.contains("monitor")) {
                workspace.active_monitor = ws["monitor"].is_string() ? 0 : -1;
            }
            
            if (ws.contains("windows")) {
                workspace.window_count = ws["windows"].is_number() ? ws["windows"].get<int>() : 0;
            }
            
            // Mark active workspace (typically the one with a monitor assigned)
            workspace.is_active = (ws.contains("monitor") && !ws["monitor"].is_null());
            
            workspace_state[workspace.id] = workspace;
        }
    }

    void HyprlandContext::parse_monitors(const json& monitors_data) {
        monitors.clear();
        
        if (!monitors_data.is_array()) return;
        
        int mon_id = 0;
        for (const auto& mon : monitors_data) {
            if (!mon.is_object()) continue;
            
            Monitor monitor;
            monitor.id = mon_id++;
            monitor.name = "UNKNOWN";
            monitor.x = monitor.y = 0;
            monitor.width = monitor.height = 1920;
            monitor.refresh_rate = 60.0;
            monitor.enabled = true;
            
            if (mon.contains("name")) {
                monitor.name = mon["name"].is_string() ? mon["name"].get<std::string>() : "MON_" + std::to_string(monitor.id);
            }
            
            if (mon.contains("x")) {
                monitor.x = mon["x"].is_number() ? mon["x"].get<int>() : 0;
            }
            
            if (mon.contains("y")) {
                monitor.y = mon["y"].is_number() ? mon["y"].get<int>() : 0;
            }
            
            if (mon.contains("width")) {
                monitor.width = mon["width"].is_number() ? mon["width"].get<int>() : 1920;
            }
            
            if (mon.contains("height")) {
                monitor.height = mon["height"].is_number() ? mon["height"].get<int>() : 1080;
            }
            
            if (mon.contains("refreshRate")) {
                monitor.refresh_rate = mon["refreshRate"].is_number() ? mon["refreshRate"].get<float>() : 60.0f;
            }
            
            if (mon.contains("disabled")) {
                monitor.enabled = !(mon["disabled"].is_boolean() && mon["disabled"].get<bool>());
            }
            
            monitors.push_back(monitor);
        }
    }

    void HyprlandContext::parse_layer_surfaces(const json& layers_data) {
        layer_surfaces.clear();
        
        if (!layers_data.is_object()) return;
        
        // Hyprland layers structure: layers[level][monitor_name][namespace][...]
        std::vector<std::string> layer_names = {"background", "bottom", "top", "overlay"};
        
        for (int layer_idx = 0; layer_idx < 4; ++layer_idx) {
            if (!layers_data.contains(layer_names[layer_idx])) continue;
            
            auto layer_obj = layers_data[layer_names[layer_idx]];
            if (!layer_obj.is_object()) continue;
            
            // Iterate through monitors in this layer
            for (auto& [monitor_name, monitor_layers] : layer_obj.items()) {
                if (!monitor_layers.is_array()) continue;
                
                for (const auto& surf : monitor_layers) {
                    if (!surf.is_object()) continue;
                    
                    LayerSurface layer;
                    layer.namespace_name = surf.contains("namespace") && surf["namespace"].is_string()
                        ? surf["namespace"].get<std::string>() : "";
                    layer.layer = layer_idx;
                    layer.geometry = "";
                    layer.exclusive_zone = false;
                    layer.keyboard_interactive = false;
                    
                    if (surf.contains("geometry")) {
                        layer.geometry = surf["geometry"].is_string() ? surf["geometry"].get<std::string>() : "";
                    }
                    
                    if (surf.contains("exclusive_zone")) {
                        layer.exclusive_zone = surf["exclusive_zone"].is_number() && surf["exclusive_zone"].get<int>() > 0;
                    }
                    
                    if (surf.contains("keyboard_interactive")) {
                        layer.keyboard_interactive = surf["keyboard_interactive"].is_boolean() ? surf["keyboard_interactive"].get<bool>() : false;
                    }
                    
                    layer_surfaces.push_back(layer);
                }
            }
        }
    }

    HyprlandContext::HyprlandContext()
        : active_window_addr(0), active_workspace_id(-1), active_monitor_id(-1), 
          cursor_x(0), cursor_y(0) {
        last_update = std::chrono::steady_clock::now();
    }

    void HyprlandContext::update_state() {
        last_update = std::chrono::steady_clock::now();
        
        // Query all Hyprland state
        json clients = exec_hypr_json("clients -j");
        json workspaces = exec_hypr_json("workspaces -j");
        json monitors = exec_hypr_json("monitors -j");
        json active_win = exec_hypr_json("activewindow -j");
        json layers = exec_hypr_json("layers -j");
        
        // Parse window tree
        parse_window_tree(clients);
        
        // Parse workspaces
        parse_workspaces(workspaces);
        
        // Parse monitors
        parse_monitors(monitors);
        
        // Parse layer surfaces
        parse_layer_surfaces(layers);
        
        // Get active window
        if (active_win.contains("address") && active_win["address"].is_string()) {
            try {
                active_window_addr = std::stoull(active_win["address"].get<std::string>(), nullptr, 16);
            } catch (...) {
                active_window_addr = 0;
            }
        }
        
        // Get active workspace
        if (!workspace_state.empty()) {
            for (auto& [ws_id, ws] : workspace_state) {
                if (ws.is_active) {
                    active_workspace_id = ws_id;
                    break;
                }
            }
        }
        
        // Get cursor position
        json cursor_pos = exec_hypr_json("cursorpos -j");
        if (cursor_pos.contains("x")) {
            cursor_x = cursor_pos["x"].is_number() ? cursor_pos["x"].get<int>() : 0;
        }
        if (cursor_pos.contains("y")) {
            cursor_y = cursor_pos["y"].is_number() ? cursor_pos["y"].get<int>() : 0;
        }
    }

    Window HyprlandContext::get_active_window() const {
        auto it = window_tree.find(active_window_addr);
        if (it != window_tree.end()) {
            return it->second;
        }
        return Window();
    }

    Window* HyprlandContext::get_window(uint64_t address) {
        auto it = window_tree.find(address);
        if (it != window_tree.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const std::map<uint64_t, Window>& HyprlandContext::get_all_windows() const {
        return window_tree;
    }

    const Workspace& HyprlandContext::get_workspace(int ws_id) const {
        static Workspace empty;
        auto it = workspace_state.find(ws_id);
        if (it != workspace_state.end()) {
            return it->second;
        }
        return empty;
    }

    const std::map<int, Workspace>& HyprlandContext::get_all_workspaces() const {
        return workspace_state;
    }

    const std::vector<Monitor>& HyprlandContext::get_monitors() const {
        return monitors;
    }

    const std::vector<LayerSurface>& HyprlandContext::get_layer_surfaces() const {
        return layer_surfaces;
    }

    void HyprlandContext::get_cursor_pos(int& x, int& y) const {
        x = cursor_x;
        y = cursor_y;
    }

    Window* HyprlandContext::window_at_cursor() {
        for (auto& [addr, win] : window_tree) {
            if (cursor_x >= win.x && cursor_x <= win.x + win.width &&
                cursor_y >= win.y && cursor_y <= win.y + win.height) {
                return &win;
            }
        }
        return nullptr;
    }

    Monitor* HyprlandContext::get_window_monitor(uint64_t window_addr) {
        auto win = get_window(window_addr);
        if (!win || win->monitor_id < 0) return nullptr;
        
        for (auto& mon : monitors) {
            if (mon.id == win->monitor_id) {
                return &mon;
            }
        }
        return nullptr;
    }

    json HyprlandContext::to_json() const {
        json result;
        
        // Active window
        result["active_window"] = {
            {"address", std::to_string(active_window_addr)},
            {"title", get_active_window().title},
            {"class", get_active_window().class_name}
        };
        
        // Active workspace
        result["active_workspace_id"] = active_workspace_id;
        
        // Cursor
        result["cursor"] = {{"x", cursor_x}, {"y", cursor_y}};
        
        // Window tree (serialized)
        json windows_array = json::array();
        for (const auto& [addr, win] : window_tree) {
            windows_array.push_back({
                {"address", std::to_string(addr)},
                {"title", win.title},
                {"class", win.class_name},
                {"x", win.x},
                {"y", win.y},
                {"width", win.width},
                {"height", win.height},
                {"floating", win.floating},
                {"fullscreen", win.fullscreen},
                {"workspace_id", win.workspace_id},
                {"monitor_id", win.monitor_id}
            });
        }
        result["window_tree"] = windows_array;
        
        // Workspaces
        json ws_array = json::array();
        for (const auto& [ws_id, ws] : workspace_state) {
            ws_array.push_back({
                {"id", ws.id},
                {"name", ws.name},
                {"is_active", ws.is_active},
                {"window_count", ws.window_count}
            });
        }
        result["workspaces"] = ws_array;
        
        // Monitors
        json mon_array = json::array();
        for (const auto& mon : monitors) {
            mon_array.push_back({
                {"id", mon.id},
                {"name", mon.name},
                {"x", mon.x},
                {"y", mon.y},
                {"width", mon.width},
                {"height", mon.height},
                {"enabled", mon.enabled}
            });
        }
        result["monitors"] = mon_array;
        
        // Layer surfaces
        json layer_array = json::array();
        for (const auto& layer : layer_surfaces) {
            layer_array.push_back({
                {"namespace", layer.namespace_name},
                {"layer", layer.layer},
                {"exclusive_zone", layer.exclusive_zone}
            });
        }
        result["layer_surfaces"] = layer_array;
        
        return result;
    }

    long HyprlandContext::get_update_age_ms() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
    }
}
