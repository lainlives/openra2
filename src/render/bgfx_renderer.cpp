#include "render/bgfx_renderer.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/log.h"

#include "spirv/vs_tile.sc.bin.h"
#include "glsl/vs_tile.sc.bin.h"
#include "essl/vs_tile.sc.bin.h"
#include "spirv/fs_tile.sc.bin.h"
#include "glsl/fs_tile.sc.bin.h"
#include "essl/fs_tile.sc.bin.h"
#if defined(_WIN32)
#include "dxbc/vs_tile.sc.bin.h"
#include "dxil/vs_tile.sc.bin.h"
#include "dxbc/fs_tile.sc.bin.h"
#include "dxil/fs_tile.sc.bin.h"
#endif

namespace ra2yr::render {
namespace {

constexpr bgfx::ViewId kView = 0;

struct ShaderBlob {
    const std::uint8_t* data = nullptr;
    std::uint32_t size = 0;
};

ShaderBlob vertex_shader(bgfx::RendererType::Enum type) {
    switch (type) {
        case bgfx::RendererType::Vulkan:
            return {vs_tile_spv, sizeof(vs_tile_spv)};
        case bgfx::RendererType::OpenGL:
            return {vs_tile_glsl, sizeof(vs_tile_glsl)};
        case bgfx::RendererType::OpenGLES:
            return {vs_tile_essl, sizeof(vs_tile_essl)};
#if defined(_WIN32)
        case bgfx::RendererType::Direct3D11:
            return {vs_tile_dxbc, sizeof(vs_tile_dxbc)};
        case bgfx::RendererType::Direct3D12:
            return {vs_tile_dxil, sizeof(vs_tile_dxil)};
#endif
        default:
            return {};
    }
}

ShaderBlob fragment_shader(bgfx::RendererType::Enum type) {
    switch (type) {
        case bgfx::RendererType::Vulkan:
            return {fs_tile_spv, sizeof(fs_tile_spv)};
        case bgfx::RendererType::OpenGL:
            return {fs_tile_glsl, sizeof(fs_tile_glsl)};
        case bgfx::RendererType::OpenGLES:
            return {fs_tile_essl, sizeof(fs_tile_essl)};
#if defined(_WIN32)
        case bgfx::RendererType::Direct3D11:
            return {fs_tile_dxbc, sizeof(fs_tile_dxbc)};
        case bgfx::RendererType::Direct3D12:
            return {fs_tile_dxil, sizeof(fs_tile_dxil)};
#endif
        default:
            return {};
    }
}

std::atomic<bool> g_screenshot_done{false};

void write_ppm(const char* path, std::uint32_t width, std::uint32_t height,
               std::uint32_t pitch, const void* data, bool yflip, bool bgra) {
    if (path == nullptr || data == nullptr || width == 0 || height == 0) {
        return;
    }
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        log(LogLevel::Warn, "[bgfx] cannot write screenshot to ", path);
        return;
    }
    std::fprintf(file, "P6\n%u %u\n255\n", width, height);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t row = yflip ? (height - 1 - y) : y;
        const std::uint8_t* pixel = bytes + static_cast<std::size_t>(row) * pitch;
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint8_t* p = pixel + static_cast<std::size_t>(x) * 4;
            const std::uint8_t rgb[3] = {
                bgra ? p[2] : p[0],
                p[1],
                bgra ? p[0] : p[2],
            };
            std::fwrite(rgb, 1, 3, file);
        }
    }
    std::fclose(file);
    log(LogLevel::Info, "[bgfx] wrote screenshot ", path);
}

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
    void screenShot(const char* file_path, std::uint32_t width, std::uint32_t height,
                    std::uint32_t pitch, bgfx::TextureFormat::Enum format,
                    const void* data, std::uint32_t size, bool yflip) override {
        static_cast<void>(size);
        const bool bgra = format == bgfx::TextureFormat::BGRA8;
        write_ppm(file_path, width, height, pitch, data, yflip, bgra);
        g_screenshot_done.store(true);
    }
    void captureBegin(std::uint32_t, std::uint32_t, std::uint32_t,
                      bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, std::uint32_t) override {}
};

BgfxLog g_bgfx_log;

struct PosTexVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};

}  // namespace

struct BgfxRenderer::Impl {
    bool initialized = false;
    std::string backend;
    int width = 0;
    int height = 0;

    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ortho = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;

    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    std::uint32_t index_count = 0;
    bool have_tiles = false;
    int tile_width = 0;
    int tile_height = 0;

    float camera_x = 0.0f;
    float camera_y = 0.0f;
    float zoom = 1.0f;

    static void destroy_geometry(bgfx::VertexBufferHandle* vbh,
                                 bgfx::IndexBufferHandle* ibh,
                                 std::uint32_t* index_count) {
        if (bgfx::isValid(*vbh)) {
            bgfx::destroy(*vbh);
            *vbh = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(*ibh)) {
            bgfx::destroy(*ibh);
            *ibh = BGFX_INVALID_HANDLE;
        }
        *index_count = 0;
    }

    void build_geometry(const std::vector<TileInstance>& instances, float default_w,
                        float default_h, bgfx::VertexBufferHandle* out_vbh,
                        bgfx::IndexBufferHandle* out_ibh,
                        std::uint32_t* out_index_count) {
        std::vector<PosTexVertex> vertices;
        std::vector<std::uint32_t> indices;
        vertices.reserve(instances.size() * 4);
        indices.reserve(instances.size() * 6);
        for (const TileInstance& instance : instances) {
            const float w = instance.width > 0.0f ? instance.width : default_w;
            const float h = instance.height > 0.0f ? instance.height : default_h;
            const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
            vertices.push_back({instance.x, instance.y, 0.0f, instance.u0, instance.v0});
            vertices.push_back(
                {instance.x + w, instance.y, 0.0f, instance.u1, instance.v0});
            vertices.push_back(
                {instance.x + w, instance.y + h, 0.0f, instance.u1, instance.v1});
            vertices.push_back({instance.x, instance.y + h, 0.0f, instance.u0, instance.v1});
            indices.insert(indices.end(),
                           {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        const bgfx::Memory* vb_mem = bgfx::copy(
            vertices.data(),
            static_cast<std::uint32_t>(vertices.size() * sizeof(PosTexVertex)));
        *out_vbh = bgfx::createVertexBuffer(vb_mem, layout);
        const bgfx::Memory* ib_mem = bgfx::copy(
            indices.data(),
            static_cast<std::uint32_t>(indices.size() * sizeof(std::uint32_t)));
        *out_ibh = bgfx::createIndexBuffer(ib_mem, BGFX_BUFFER_INDEX32);
        *out_index_count = static_cast<std::uint32_t>(indices.size());
    }
};

BgfxRenderer::BgfxRenderer() : impl_(std::make_unique<Impl>()) {}

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
    impl_->initialized = true;
    impl_->width = width;
    impl_->height = height;

    impl_->layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    impl_->backend = bgfx::getRendererName(type);

    const ShaderBlob vs_blob = vertex_shader(type);
    const ShaderBlob fs_blob = fragment_shader(type);
    if (vs_blob.data == nullptr || fs_blob.data == nullptr) {
        if (error != nullptr) {
            *error = std::string("no embedded shader for renderer ") + impl_->backend;
        }
        return false;
    }
    const bgfx::ShaderHandle vs =
        bgfx::createShader(bgfx::copy(vs_blob.data, vs_blob.size));
    const bgfx::ShaderHandle fs =
        bgfx::createShader(bgfx::copy(fs_blob.data, fs_blob.size));
    impl_->program = bgfx::createProgram(vs, fs, true);
    impl_->u_ortho = bgfx::createUniform("u_ortho", bgfx::UniformType::Mat4);
    impl_->s_tex = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    bgfx::setViewClear(kView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1A1A2EFF, 1.0f, 0);
    bgfx::setViewRect(kView, 0, 0, static_cast<std::uint16_t>(impl_->width),
                      static_cast<std::uint16_t>(impl_->height));
    return true;
}

void BgfxRenderer::shutdown() {
    if (!impl_->initialized) {
        return;
    }
    Impl::destroy_geometry(&impl_->vbh, &impl_->ibh, &impl_->index_count);
    if (bgfx::isValid(impl_->texture)) {
        bgfx::destroy(impl_->texture);
        impl_->texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(impl_->program)) {
        bgfx::destroy(impl_->program);
        impl_->program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(impl_->u_ortho)) {
        bgfx::destroy(impl_->u_ortho);
        impl_->u_ortho = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(impl_->s_tex)) {
        bgfx::destroy(impl_->s_tex);
        impl_->s_tex = BGFX_INVALID_HANDLE;
    }
    bgfx::shutdown();
    impl_->initialized = false;
}

void BgfxRenderer::resize(int width, int height) {
    if (!impl_->initialized || width <= 0 || height <= 0) {
        return;
    }
    impl_->width = width;
    impl_->height = height;
    bgfx::SwapChain swap;
    swap.nwh = nullptr;
    swap.ndt = nullptr;
    swap.width = static_cast<std::uint32_t>(width);
    swap.height = static_cast<std::uint32_t>(height);
    swap.numBackBuffers = 2;
    bgfx::reset(BGFX_RESET_VSYNC, &swap);
}

bool BgfxRenderer::set_atlas(const std::vector<std::uint8_t>& rgba, int width,
                             int height, std::string* error) {
    if (!impl_->initialized) {
        if (error != nullptr) {
            *error = "renderer is not initialized";
        }
        return false;
    }
    if (width <= 0 || height <= 0 ||
        rgba.size() < static_cast<std::size_t>(width) * height * 4) {
        if (error != nullptr) {
            *error = "tile texture is too small";
        }
        return false;
    }
    if (bgfx::isValid(impl_->texture)) {
        bgfx::destroy(impl_->texture);
    }
    impl_->texture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height), false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT |
            BGFX_SAMPLER_MAG_POINT,
        bgfx::copy(rgba.data(), static_cast<std::uint32_t>(rgba.size())));
    return bgfx::isValid(impl_->texture);
}

void BgfxRenderer::set_tiles(std::vector<TileInstance> tiles) {
    if (!impl_->initialized || tiles.empty()) {
        return;
    }
    Impl::destroy_geometry(&impl_->vbh, &impl_->ibh, &impl_->index_count);
    std::stable_sort(tiles.begin(), tiles.end(),
                     [](const TileInstance& a, const TileInstance& b) {
                         return a.depth < b.depth;
                     });
    impl_->build_geometry(tiles, 1.0f, 1.0f, &impl_->vbh, &impl_->ibh,
                          &impl_->index_count);
    impl_->have_tiles = true;
    log(LogLevel::Debug, "terrain: ", tiles.size(), " tiles, ", impl_->index_count,
        " indices");
}

void BgfxRenderer::set_camera(float center_x, float center_y, float zoom) {
    impl_->camera_x = center_x;
    impl_->camera_y = center_y;
    impl_->zoom = zoom > 0.0f ? zoom : 1.0f;
}

void BgfxRenderer::request_screenshot(const std::string& path) {
    if (!impl_->initialized) {
        return;
    }
    g_screenshot_done.store(false);
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
}

bool BgfxRenderer::screenshot_ready() const {
    return g_screenshot_done.load();
}

void BgfxRenderer::render() {
    if (!impl_->initialized) {
        return;
    }
    bgfx::setViewRect(kView, 0, 0, static_cast<std::uint16_t>(impl_->width),
                      static_cast<std::uint16_t>(impl_->height));
    bgfx::touch(kView);

    if (impl_->have_tiles && bgfx::isValid(impl_->texture) &&
        bgfx::isValid(impl_->program) && bgfx::isValid(impl_->vbh) &&
        bgfx::isValid(impl_->ibh)) {
        const float width = static_cast<float>(impl_->width);
        const float height = static_cast<float>(impl_->height);
        const float zoom = impl_->zoom;

        // world -> window: (world - centre) * zoom + viewport/2
        float proj[16] = {};
        proj[0] = 2.0f * zoom / width;
        proj[5] = -2.0f * zoom / height;
        proj[10] = 1.0f;
        proj[12] = -2.0f * zoom * impl_->camera_x / width;
        proj[13] = 2.0f * zoom * impl_->camera_y / height;
        proj[15] = 1.0f;

        bgfx::setUniform(impl_->u_ortho, proj);
        bgfx::setTexture(0, impl_->s_tex, impl_->texture);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                       BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                             BGFX_STATE_BLEND_INV_SRC_ALPHA));
        bgfx::setVertexBuffer(0, impl_->vbh);
        bgfx::setIndexBuffer(impl_->ibh, 0, impl_->index_count);
        bgfx::submit(kView, impl_->program);
    }
    bgfx::frame();
}

bool BgfxRenderer::valid() const {
    return impl_->initialized;
}

std::string BgfxRenderer::backend() const {
    return impl_->backend;
}

}  // namespace ra2yr::render
