#pragma once

#include <cstdint>
#include <memory>
#include <string>

// Platform abstraction. The simulation and game logic must never include this
// header; only the app and the platform backends may. Backends are selected at
// build time and live beside this file.

namespace ra2yr::platform {

struct WindowDesc {
    int width = 800;
    int height = 600;
    std::string title = "ra2yr";
    bool visible = true;
};

// A window handle in the shape bgfx expects. `handle`/`display` are backend
// specific (X11 Window + Display*, Wayland wl_surface + wl_display, HWND).
struct NativeWindow {
    void* handle = nullptr;
    void* display = nullptr;
    bool wayland = false;
};

class Window {
public:
    virtual ~Window() = default;

    // Pump the OS event queue. Called once per frame.
    virtual void poll() = 0;
    virtual bool should_close() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual NativeWindow native_window() const { return {}; }
};

class Platform {
public:
    virtual ~Platform() = default;

    virtual std::string name() const = 0;
    virtual std::unique_ptr<Window> create_window(const WindowDesc& desc) = 0;

    // Monotonic milliseconds since an unspecified epoch.
    virtual std::uint64_t ticks_ms() const = 0;
};

// Headless backend: no window, no input, no audio. Always available and used by
// tests and CI so the build stays free of SDK dependencies.
std::unique_ptr<Platform> make_null_platform();

#if defined(RA2YR_PLATFORM_SDL)
// SDL3 backend. Returns nullptr if SDL cannot be initialized (for example on a
// headless host), so callers can fall back to the null platform.
std::unique_ptr<Platform> make_sdl_platform();
#endif

}  // namespace ra2yr::platform
