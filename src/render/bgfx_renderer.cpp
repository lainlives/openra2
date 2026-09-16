#include "render/bgfx_renderer.h"

#include <bgfx/bgfx.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/log.h"

namespace ra2yr::render {
namespace {

constexpr bgfx::ViewId kView = 0;

// Routes bgfx's own diagnostics through the engine logger. Trace messages are
// emitted at debug level, so they show in development builds or with
// --verbose, and stay quiet otherwise.
class BgfxLog final : public bgfx::CallbackI {
public:
    void fatal(const char* file_path, std::uint16_t line, bgfx::Fatal::Enum /*code*/,
               const char* str) override {
        log(LogLevel::Error, "[bgfx] fatal: ", str, " (", file_path, ":", line, ")");
        std::abort();
    }

    void traceVargs(const char* file_path, std::uint16_t line, const char* format,
                    va_list arg_list) override {
        char buffer[2048];
        std::vsnprintf(buffer, sizeof(buffer), format, arg_list);
        std::string message(buffer);
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }
        log(LogLevel::Debug, "[bgfx] ", message, " (", file_path, ":", line, ")");
    }

    void profilerBegin(const char*, std::uint32_t, const char*, std::uint16_t) override {}
    void profilerBeginLiteral(const char*, std::uint32_t, const char*, std::uint16_t) override {}
    void profilerEnd() override {}
    std::uint32_t cacheReadSize(std::uint64_t) override { return 0; }
    bool cacheRead(std::uint64_t, void*, std::uint32_t) override { return false; }
    void cacheWrite(std::uint64_t, const void*, std::uint32_t) override {}
    void screenShot(const char*, std::uint32_t, std::uint32_t, std::uint32_t,
                    bgfx::TextureFormat::Enum, const void*, std::uint32_t, bool) override {}
    void captureBegin(std::uint32_t, std::uint32_t, std::uint32_t,
                      bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, std::uint32_t) override {}
};

BgfxLog g_bgfx_log;

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
    init.callback = &g_bgfx_log;

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
