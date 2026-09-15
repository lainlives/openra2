// bgfx renderer bring-up.
//
// Compiled only when RA2YR_ENABLE_BGFX is on. This is the only translation
// unit that includes bgfx, keeping its headers away from the rest of the
// engine.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "platform/platform.h"

namespace ra2yr::render {

class BgfxRenderer {
public:
    BgfxRenderer() = default;
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

    bool valid() const { return initialized_; }
    const char* backend() const { return backend_.c_str(); }

private:
    bool initialized_ = false;
    platform::NativeWindow window_;
    int width_ = 0;
    int height_ = 0;
    std::string backend_;
};

}  // namespace ra2yr::render
