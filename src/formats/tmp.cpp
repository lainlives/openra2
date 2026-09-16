#include "formats/tmp.h"

#include <array>
#include <cstring>

namespace ra2yr::formats {
namespace {

std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t read_i32(const std::uint8_t* p) {
    return static_cast<std::int32_t>(read_u32(p));
}

// Expand "isometric to square" rows into a full width*height buffer. Returns
// the number of source bytes consumed, or 0 if the source is too short.
std::size_t decode_iso(const std::uint8_t* src, std::size_t src_size, std::uint32_t width,
                       std::uint32_t height, std::vector<std::uint8_t>* out) {
    out->assign(static_cast<std::size_t>(width) * height, 0);
    if (width == 0 || height < 2) {
        return 0;
    }
    const std::uint32_t step = width / (height / 2);
    if (step == 0) {
        return 0;
    }
    std::uint32_t row_width = step;
    int width_inc = static_cast<int>(step);
    std::size_t pos = 0;
    for (std::uint32_t y = 0; y + 1 < height; ++y) {
        if (row_width == 0 || row_width > width) {
            return 0;
        }
        const std::uint32_t start = (width - row_width) / 2;
        if (pos + row_width > src_size) {
            return 0;
        }
        std::memcpy(out->data() + static_cast<std::size_t>(y) * width + start, src + pos,
                    row_width);
        pos += row_width;
        row_width = static_cast<std::uint32_t>(static_cast<int>(row_width) + width_inc);
        if (row_width == width) {
            width_inc = -static_cast<int>(step);
        }
    }
    return pos;
}

}  // namespace

std::optional<TmpFile> TmpFile::from_bytes(const std::vector<std::uint8_t>& data,
                                           std::string* error) {
    const auto fail = [error](const char* message) -> std::optional<TmpFile> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    if (data.size() < 16) {
        return fail("TMP is too short");
    }
    TmpFile tmp;
    tmp.tiles_x_ = read_u32(data.data());
    tmp.tiles_y_ = read_u32(data.data() + 4);
    tmp.tile_width_ = read_u32(data.data() + 8);
    tmp.tile_height_ = read_u32(data.data() + 12);

    if (tmp.tiles_x_ == 0 || tmp.tiles_y_ == 0) {
        return fail("TMP contains no tiles");
    }
    if (tmp.tile_width_ == 0 || tmp.tile_height_ < 2) {
        return fail("TMP has an invalid tile size");
    }

    const std::size_t count = static_cast<std::size_t>(tmp.tiles_x_) * tmp.tiles_y_;
    const std::size_t index_bytes = count * 4;
    if (data.size() < 16 + index_bytes) {
        return fail("TMP offset index is truncated");
    }

    tmp.tiles_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t cell = read_u32(data.data() + 16 + i * 4);
        TmpTile& tile = tmp.tiles_[i];
        if (cell == 0) {
            continue;
        }
        if (static_cast<std::size_t>(cell) + kCellHeaderSize > data.size()) {
            return fail("TMP cell header is out of range");
        }
        const std::uint8_t* header = data.data() + cell;
        tile.x = read_i32(header + 0);
        tile.y = read_i32(header + 4);
        tile.extra_offset = read_u32(header + 8);
        tile.z_offset = read_u32(header + 12);
        tile.extra_z_offset = read_u32(header + 16);
        tile.extra_x = read_i32(header + 20);
        tile.extra_y = read_i32(header + 24);
        tile.extra_w = read_u32(header + 28);
        tile.extra_h = read_u32(header + 32);
        tile.flags = header[36];
        tile.height = header[40];
        tile.land_type = header[41];
        tile.slope_type = header[42];
        std::memcpy(tile.radar_left, header + 43, 3);
        std::memcpy(tile.radar_right, header + 46, 3);

        // Image data follows the cell header; the height map is at z_offset
        // (relative to the cell header) or immediately after the image. The
        // on-disk image is smaller than the square buffer because only the
        // diamond rows are stored, so bounds are validated by the decoder.
        const std::size_t image_start = static_cast<std::size_t>(cell) + kCellHeaderSize;
        if (image_start >= data.size()) {
            return fail("TMP image data is out of range");
        }
        const std::size_t image_consumed =
            decode_iso(data.data() + image_start, data.size() - image_start, tmp.tile_width_,
                       tmp.tile_height_, &tile.image);
        if (image_consumed == 0) {
            return fail("TMP image data is truncated");
        }

        std::size_t z_start = 0;
        if (tile.z_offset != 0) {
            z_start = static_cast<std::size_t>(cell) + tile.z_offset;
        } else {
            z_start = image_start + image_consumed;
        }
        if (z_start < data.size()) {
            decode_iso(data.data() + z_start, data.size() - z_start, tmp.tile_width_,
                       tmp.tile_height_, &tile.zdata);
        }

        if (tile.has_extra() && tile.extra_offset != 0) {
            const std::size_t extra_start = static_cast<std::size_t>(cell) + tile.extra_offset;
            const std::size_t extra_bytes =
                static_cast<std::size_t>(tile.extra_w) * tile.extra_h;
            if (extra_start + extra_bytes <= data.size()) {
                tile.extra.assign(data.begin() + static_cast<std::ptrdiff_t>(extra_start),
                                  data.begin() +
                                      static_cast<std::ptrdiff_t>(extra_start + extra_bytes));
            }
        }
    }
    return tmp;
}

std::vector<std::uint8_t> TmpFile::to_rgba(const TmpTile& tile,
                                           const Palette& palette) const {
    std::vector<std::uint8_t> out(tile.image.size() * 4, 0);
    for (std::size_t i = 0; i < tile.image.size(); ++i) {
        const Rgba color = palette.color(tile.image[i]);
        out[i * 4 + 0] = color.r;
        out[i * 4 + 1] = color.g;
        out[i * 4 + 2] = color.b;
        out[i * 4 + 3] = color.a;
    }
    return out;
}

}  // namespace ra2yr::formats
