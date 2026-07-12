#include "ScreenContext.hpp"
#include "../includes/HyprlandContext.hpp"
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace alyssa_vision {

#ifdef __linux__
class HyprlandScreenContext::Impl {
public:
    hyprland::HyprlandContext context;
};

HyprlandScreenContext::HyprlandScreenContext() : pImpl(std::make_unique<Impl>()) {}

ScreenContext HyprlandScreenContext::get_context() {
    ScreenContext ctx;
    auto& hctx = pImpl->context;
    ctx.cursor_x = hctx.get_cursor_x();          // ensure these methods exist (they do)
    ctx.cursor_y = hctx.get_cursor_y();
    auto win = hctx.get_active_window();
    ctx.active_window_title = win.title;
    ctx.active_window_class = win.class_name;
    ctx.workspace_id = hctx.get_active_workspace_id();
    const auto& monitors = hctx.get_monitors();
    if (!monitors.empty()) {
        ctx.screen_width = monitors[0].width;
        ctx.screen_height = monitors[0].height;
    }
    return ctx;
}

void HyprlandScreenContext::update() {
    pImpl->context.update_state();
}
#endif // __linux__


// ----- WindowsScreenContext (stub) -----
WindowsScreenContext::WindowsScreenContext() {}
ScreenContext WindowsScreenContext::get_context() {
    ScreenContext ctx;
#ifdef _WIN32
    ctx.screen_width = GetSystemMetrics(SM_CXSCREEN);
    ctx.screen_height = GetSystemMetrics(SM_CYSCREEN);
    POINT pt;
    GetCursorPos(&pt);
    ctx.cursor_x = pt.x;
    ctx.cursor_y = pt.y;
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        char title[256];
        GetWindowTextA(hwnd, title, sizeof(title));
        ctx.active_window_title = title;
        char className[256];
        GetClassNameA(hwnd, className, sizeof(className));
        ctx.active_window_class = className;
    }
    // workspace not available on Windows without additional APIs
#endif
    return ctx;
}
void WindowsScreenContext::update() {}

// Factory
std::unique_ptr<ScreenContextProvider> create_screen_context() {
#ifdef __linux__
    if (std::getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        return std::make_unique<HyprlandScreenContext>();
    }
#endif
#ifdef _WIN32
    return std::make_unique<WindowsScreenContext>();
#else
    // fallback: X11 could be added later, or return nullptr
    return nullptr;
#endif
}

} // namespace alyssa_vision