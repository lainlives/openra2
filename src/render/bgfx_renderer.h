// bgfx renderer: device bring-up, terrain tiles, and frame presentation.
//
// Compiled only when RA2YR_ENABLE_BGFX is on. This is the only translation
// unit that includes bgfx, keeping its headers away from the rest of the
// engine.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "platform/platform.h"
#include "render/scene.h"

namespace ra2yr::render {

class BgfxRenderer {
public:
    BgfxRenderer();
    ~BgfxRenderer();

    BgfxRenderer(const BgfxRenderer&) = delete;
    BgfxRenderer& operator=(const BgfxRenderer&) = delete;

    // Create the bgfx device on the given native window. Returns false and
    // fills `error` on failure.
    bool initialize(const platform::NativeWindow& window, int width, int height,
                    std::string* error = nullptr);
    void shutdown();

    void resize(int width, int height);

    // Clear the frame and present it. Call once per frame.
    void render();

    bool valid() const;
    std::string backend() const;

    // Upload the tile atlas. `rgba` is tightly packed RGBA8.
    bool set_atlas(const std::vector<std::uint8_t>& rgba, int width, int height,
                   std::string* error = nullptr);

    // Replace the scene geometry. Tiles and sprites share the atlas and are
    // drawn in one depth-sorted pass.
    void set_tiles(std::vector<TileInstance> tiles);

    // Camera centre in world pixels and zoom (1.0 = one world pixel per screen
    // pixel).
    void set_camera(float center_x, float center_y, float zoom);

    // Queue a screenshot (PPM) for the end of the next frame.
    void request_screenshot(const std::string& path);
    bool screenshot_ready() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ra2yr::render
