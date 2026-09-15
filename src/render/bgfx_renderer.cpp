#include "render/bgfx_renderer.h"

#include <bgfx/bgfx.h>

#include <cstdint>

namespace ra2yr::render {
namespace {

constexpr bgfx::ViewId kView = 0;

}  // namespace

BgfxRenderer::~BgfxRenderer() {
    shutdown();
}

bool BgfxRenderer::initialize(const platform::NativeWindow& window, int width, int height,
                              std::string* error) {
    bgfx::Init init;
    init.type = bgfx::RendererType::Count;
    init.platformData.type = window.wayland
                                 ? bgfx::NativeWindowHandleType::Wayland
                                 : bgfx::NativeWindowHandleType::Default;
    init.swapChain.nwh = window.handle;
    init.swapChain.ndt = window.display;
    init.swapChain.width = static_cast<std::uint32_t>(width);
    init.swapChain.height = static_cast<std::uint32_t>(height);
    init.swapChain.numBackBuffers = 2;
    init.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(init)) {
        if (error != nullptr) {
            *error = "bgfx::init failed";
        }
        return false;
    }

    width_ = width;
    height_ = height;
    window_ = window;
    initialized_ = true;

    bgfx::setViewClear(kView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1A1A2EFF, 1.0f, 0);
    bgfx::setViewRect(kView, 0, 0, static_cast<std::uint16_t>(width_),
                      static_cast<std::uint16_t>(height_));

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    backend_ = bgfx::getRendererName(type);
    return true;
}

void BgfxRenderer::shutdown() {
    if (initialized_) {
        bgfx::shutdown();
        initialized_ = false;
    }
}

void BgfxRenderer::resize(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) {
        return;
    }
    width_ = width;
    height_ = height;
    bgfx::SwapChain swap;
    swap.nwh = window_.handle;
    swap.ndt = window_.display;
    swap.width = static_cast<std::uint32_t>(width_);
    swap.height = static_cast<std::uint32_t>(height_);
    swap.numBackBuffers = 2;
    bgfx::reset(BGFX_RESET_VSYNC, &swap);
    bgfx::setViewRect(kView, 0, 0, static_cast<std::uint16_t>(width_),
                      static_cast<std::uint16_t>(height_));
}

void BgfxRenderer::render() {
    if (!initialized_) {
        return;
    }
    bgfx::touch(kView);
    bgfx::frame();
}

}  // namespace ra2yr::render
