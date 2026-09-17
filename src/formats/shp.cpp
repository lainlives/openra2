#include "formats/shp.h"

#include <cstring>

namespace ra2yr::formats {
namespace {

constexpr std::size_t kHeaderSize = 8;
constexpr std::size_t kFrameEntrySize = 24;

std::uint16_t read_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1]) << 8;
}

std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

// Raw rows: width * height bytes with no framing.
bool decode_raw(const std::uint8_t* data, std::size_t size, ShpFrame* frame) {
    const std::size_t needed =
        static_cast<std::size_t>(frame->width) * frame->height;
    if (size < needed) {
        return false;
    }
    frame->pixels.assign(data, data + needed);
    return true;
}

bool decode_rows(const std::uint8_t* data, std::size_t size, bool rle, ShpFrame* frame) {
    const std::size_t total = static_cast<std::size_t>(frame->width) * frame->height;
    frame->pixels.assign(total, 0);
    std::size_t pos = 0;
    std::size_t out = 0;
    for (std::uint16_t row = 0; row < frame->height; ++row) {
        if (pos + 2 > size) {
            return false;
        }
        std::size_t row_bytes = read_u16(data + pos);
        pos += 2;
        if (row_bytes < 2) {
            return false;
        }
        row_bytes -= 2;
        if (pos + row_bytes > size) {
            return false;
        }
        const std::size_t row_end = pos + row_bytes;
        std::size_t x = 0;
        while (pos < row_end && x < frame->width) {
            const std::uint8_t value = data[pos++];
            if (!rle || value != 0) {
                if (out >= total) {
                    return false;
                }
                frame->pixels[out++] = value;
                ++x;
                continue;
            }
            // RLE-Zero: 0x00 introduces a run of zeros.
            if (pos >= row_end) {
                return false;
            }
            std::size_t run = data[pos++];
            if (x + run > frame->width) {
                run = frame->width - x;
            }
            for (std::size_t i = 0; i < run; ++i) {
                if (out >= total) {
                    return false;
                }
                frame->pixels[out++] = 0;
                ++x;
            }
        }
        pos = row_end;
    }
    return true;
}

}  // namespace

std::optional<ShpFile> ShpFile::from_bytes(const std::vector<std::uint8_t>& data,
                                           std::string* error) {
    const auto fail = [error](const char* message) -> std::optional<ShpFile> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    if (data.size() < kHeaderSize) {
        return fail("SHP is too short");
    }
    ShpFile shp;
    if (read_u16(data.data()) != 0) {
        return fail("not an SHP file");
    }
    shp.width_ = read_u16(data.data() + 2);
    shp.height_ = read_u16(data.data() + 4);
    const std::uint16_t frames = read_u16(data.data() + 6);
    if (shp.width_ == 0 || shp.height_ == 0 || frames == 0) {
        return fail("SHP has no frames");
    }
    if (data.size() < kHeaderSize + static_cast<std::size_t>(frames) * kFrameEntrySize) {
        return fail("SHP frame table is truncated");
    }

    shp.frames_.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const std::uint8_t* entry = data.data() + kHeaderSize + i * kFrameEntrySize;
        ShpFrame& frame = shp.frames_[i];
        frame.x = read_u16(entry + 0);
        frame.y = read_u16(entry + 2);
        frame.width = read_u16(entry + 4);
        frame.height = read_u16(entry + 6);
        frame.compression = entry[8];
        frame.radar_color = {entry[12], entry[13], entry[14]};
        const std::uint32_t offset = read_u32(entry + 20);

        if (frame.width == 0 || frame.height == 0) {
            continue;
        }
        if (offset == 0 || offset >= data.size()) {
            continue;
        }
        const std::uint8_t* frame_data = data.data() + offset;
        const std::size_t frame_size = data.size() - offset;
        bool ok = false;
        switch (frame.compression) {
            case 0:
            case 1:
                ok = decode_raw(frame_data, frame_size, &frame);
                break;
            case 2:
                ok = decode_rows(frame_data, frame_size, false, &frame);
                break;
            case 3:
                ok = decode_rows(frame_data, frame_size, true, &frame);
                break;
            default:
                return fail("SHP frame has an unknown compression type");
        }
        if (!ok) {
            return fail("SHP frame data is truncated");
        }
    }
    return shp;
}

std::vector<std::uint8_t> ShpFile::to_rgba(const ShpFrame& frame,
                                           const Palette& palette) const {
    std::vector<std::uint8_t> out(
        static_cast<std::size_t>(width_) * height_ * 4, 0);
    for (std::uint32_t row = 0; row < frame.height; ++row) {
        const std::uint32_t dy = frame.y + row;
        if (dy >= height_) {
            break;
        }
        for (std::uint32_t col = 0; col < frame.width; ++col) {
            const std::uint32_t dx = frame.x + col;
            if (dx >= width_) {
                break;
            }
            const std::uint8_t index =
                frame.pixels[static_cast<std::size_t>(row) * frame.width + col];
            const Rgba color = palette.color(index);
            const std::size_t dst = (static_cast<std::size_t>(dy) * width_ + dx) * 4;
            out[dst + 0] = color.r;
            out[dst + 1] = color.g;
            out[dst + 2] = color.b;
            out[dst + 3] = color.a;
        }
    }
    return out;
}

}  // namespace ra2yr::formats
