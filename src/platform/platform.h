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

class Window {
public:
    virtual ~Window() = default;

    // Pump the OS event queue. Called once per frame.
    virtual void poll() = 0;
    virtual bool should_close() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
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

}  // namespace ra2yr::platform
