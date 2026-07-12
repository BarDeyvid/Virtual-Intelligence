#pragma once
#include <string>
#include <optional>
#include <memory>

namespace alyssa_vision {

struct ScreenContext {
    std::string active_window_title;
    std::string active_window_class;
    int workspace_id = -1;
    int cursor_x = 0;
    int cursor_y = 0;
    int screen_width = 1920;
    int screen_height = 1080;
};

class ScreenContextProvider {
public:
    virtual ~ScreenContextProvider() = default;
    virtual ScreenContext get_context() = 0;
    virtual void update() = 0;
};

// Linux implementation using HyprlandContext
class HyprlandScreenContext : public ScreenContextProvider {
public:
    HyprlandScreenContext();
    ~HyprlandScreenContext() override = default;
    ScreenContext get_context() override;
    void update() override;
private:
    // We'll use the existing HyprlandContext
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Windows stub (could use Windows API)
class WindowsScreenContext : public ScreenContextProvider {
public:
    WindowsScreenContext();
    ~WindowsScreenContext() override = default;
    ScreenContext get_context() override;
    void update() override;
};

// Factory to create the appropriate provider
std::unique_ptr<ScreenContextProvider> create_screen_context();

} // namespace alyssa_vision