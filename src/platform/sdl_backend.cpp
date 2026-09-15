// SDL3 platform backend: window creation and event pumping.
//
// Compiled only when RA2YR_ENABLE_SDL3 is on. The native window handle is
// exposed for the bgfx renderer.

#include "platform.h"

#include <SDL3/SDL.h>

#include <cstdint>

namespace ra2yr::platform {
namespace {

class SdlWindow final : public Window {
public:
    SdlWindow(SDL_Window* window, bool owns) : window_(window), owns_(owns) {
        SDL_GetWindowSizeInPixels(window_, &width_, &height_);
    }

    ~SdlWindow() override {
        if (owns_ && window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
    }

    void poll() override {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                closed_ = true;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                       event.window.windowID == SDL_GetWindowID(window_)) {
                closed_ = true;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
                       event.window.windowID == SDL_GetWindowID(window_)) {
                width_ = event.window.data1;
                height_ = event.window.data2;
            }
        }
    }

    bool should_close() const override { return closed_; }
    int width() const override { return width_; }
    int height() const override { return height_; }

    NativeWindow native_window() const override {
        NativeWindow result;
        const SDL_PropertiesID props = SDL_GetWindowProperties(window_);

        if (void* surface = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr)) {
            result.handle = surface;
            result.display = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            result.wayland = true;
            return result;
        }

        const Sint64 x11 =
            SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (x11 != 0) {
            result.handle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(x11));
            result.display = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        }
        return result;
    }

private:
    SDL_Window* window_ = nullptr;
    bool owns_ = false;
    bool closed_ = false;
    int width_ = 0;
    int height_ = 0;
};

class SdlPlatform final : public Platform {
public:
    explicit SdlPlatform(bool owns_sdl) : owns_sdl_(owns_sdl) {}

    ~SdlPlatform() override {
        if (owns_sdl_) {
            SDL_Quit();
        }
    }

    std::string name() const override { return "sdl3"; }

    std::unique_ptr<Window> create_window(const WindowDesc& desc) override {
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
        if (!desc.visible) {
            flags |= SDL_WINDOW_HIDDEN;
        }
        SDL_Window* window =
            SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
        if (window == nullptr) {
            return nullptr;
        }
        return std::make_unique<SdlWindow>(window, true);
    }

    std::uint64_t ticks_ms() const override { return SDL_GetTicks(); }

private:
    bool owns_sdl_ = false;
};

}  // namespace

std::unique_ptr<Platform> make_sdl_platform() {
    const bool owns = !SDL_WasInit(SDL_INIT_VIDEO);
    if (owns && !SDL_Init(SDL_INIT_VIDEO)) {
        return nullptr;
    }
    return std::make_unique<SdlPlatform>(owns);
}

}  // namespace ra2yr::platform
