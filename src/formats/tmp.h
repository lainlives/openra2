// Red Alert 2 terrain template (.tmp / theater .tem/.sno/.urb/...) reader.
//
// Each file holds a grid of tile cells. A cell has a diamond image stored in
// "isometric to square" rows, a parallel height (Z) map, and optional extra
// graphics. All pixel data is raw 8-bit palette indices; it is not compressed.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "formats/palette.h"

namespace ra2yr::formats {

struct TmpTile {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t extra_offset = 0;
    std::uint32_t z_offset = 0;
    std::uint32_t extra_z_offset = 0;
    std::int32_t extra_x = 0;
    std::int32_t extra_y = 0;
    std::uint32_t extra_w = 0;
    std::uint32_t extra_h = 0;
    std::uint8_t flags = 0;
    std::uint8_t height = 0;
    std::uint8_t land_type = 0;
    std::uint8_t slope_type = 0;
    std::uint8_t radar_left[3] = {0, 0, 0};
    std::uint8_t radar_right[3] = {0, 0, 0};

    std::vector<std::uint8_t> image;  // tile_width * tile_height
    std::vector<std::uint8_t> zdata;  // tile_width * tile_height
    std::vector<std::uint8_t> extra;  // extra_w * extra_h

    bool has_extra() const { return (flags & 0x1) != 0; }
    bool has_zdata() const { return (flags & 0x2) != 0; }

    std::uint8_t image_at(std::uint32_t px, std::uint32_t py,
                          std::uint32_t width) const {
        return image[static_cast<std::size_t>(py) * width + px];
    }
};

class TmpFile {
public:
    static constexpr std::size_t kCellHeaderSize = 52;

    static std::optional<TmpFile> from_bytes(const std::vector<std::uint8_t>& data,
                                             std::string* error = nullptr);

    std::uint32_t tiles_x() const { return tiles_x_; }
    std::uint32_t tiles_y() const { return tiles_y_; }
    std::uint32_t tile_width() const { return tile_width_; }
    std::uint32_t tile_height() const { return tile_height_; }
    std::size_t tile_count() const { return tiles_.size(); }
    const TmpTile& tile(std::size_t index) const { return tiles_[index]; }

    // Decode one cell image into RGBA using the given palette.
    std::vector<std::uint8_t> to_rgba(const TmpTile& tile, const Palette& palette) const;

private:
    std::uint32_t tiles_x_ = 0;
    std::uint32_t tiles_y_ = 0;
    std::uint32_t tile_width_ = 0;
    std::uint32_t tile_height_ = 0;
    std::vector<TmpTile> tiles_;
};

}  // namespace ra2yr::formats
