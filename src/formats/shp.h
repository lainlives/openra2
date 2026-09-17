// Red Alert 2 / Tiberian Sun SHP sprite reader.
//
// Frames are cropped 8-bit palette images positioned inside a full frame.
// Three storage types are used: raw, raw with per-scanline byte counts, and
// the TS variant of Westwood RLE-Zero (per-scanline counts, 0x00 introduces a
// zero run). Palette index 0 is transparent.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "formats/palette.h"

namespace ra2yr::formats {

struct ShpFrame {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t compression = 0;  // 0/1 raw, 2 counted rows, 3 RLE-Zero
    std::array<std::uint8_t, 3> radar_color = {0, 0, 0};
    std::vector<std::uint8_t> pixels;  // width * height, row-major
};

class ShpFile {
public:
    static std::optional<ShpFile> from_bytes(const std::vector<std::uint8_t>& data,
                                             std::string* error = nullptr);

    std::uint16_t width() const { return width_; }
    std::uint16_t height() const { return height_; }
    std::size_t frame_count() const { return frames_.size(); }
    const ShpFrame& frame(std::size_t index) const { return frames_[index]; }

    // Expand a cropped frame into a full width x height RGBA image at its
    // frame offset, with palette index 0 treated as transparent.
    std::vector<std::uint8_t> to_rgba(const ShpFrame& frame, const Palette& palette) const;

private:
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    std::vector<ShpFrame> frames_;
};

}  // namespace ra2yr::formats
