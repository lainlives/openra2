#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../src/formats/palette.h"
#include "../src/formats/tmp.h"
#include "../src/render/iso.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " \
                      << #cond << "\n";                                    \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

void test_palette() {
    std::vector<std::uint8_t> data(ra2yr::formats::Palette::kByteSize, 0);
    data[0] = 10;
    data[1] = 20;
    data[2] = 30;
    data[5 * 3 + 0] = 40;
    data[5 * 3 + 1] = 50;
    data[5 * 3 + 2] = 60;

    std::string error;
    auto palette = ra2yr::formats::Palette::from_bytes(data, &error);
    CHECK(palette.has_value());
    if (!palette) {
        return;
    }
    const auto transparent = palette->color(0);
    CHECK(transparent.r == 10 && transparent.g == 20 && transparent.b == 30);
    CHECK(transparent.a == 0);
    const auto five = palette->color(5);
    CHECK(five.r == 40 && five.g == 50 && five.b == 60 && five.a == 255);
}

void test_iso_projection() {
    const auto origin = ra2yr::render::cell_to_screen(0, 0, 60, 30);
    CHECK(origin.x == 0 && origin.y == 0);
    const auto east = ra2yr::render::cell_to_screen(1, 0, 60, 30);
    CHECK(east.x == 30 && east.y == 15);
    const auto south = ra2yr::render::cell_to_screen(0, 1, 60, 30);
    CHECK(south.x == -30 && south.y == 15);

    const auto order = ra2yr::render::build_iso_order(3, 3);
    CHECK(order.size() == 9);
    for (std::size_t i = 1; i < order.size(); ++i) {
        const int prev = order[i - 1].cx + order[i - 1].cy;
        const int cur = order[i].cx + order[i].cy;
        CHECK(prev <= cur);
    }
}

// Produce the isometric-to-square on-disk bytes for a square tile buffer.
std::vector<std::uint8_t> encode_iso(const std::vector<std::uint8_t>& square, int width,
                                     int height) {
    std::vector<std::uint8_t> out;
    const int step = width / (height / 2);
    int row_width = step;
    int inc = step;
    for (int y = 0; y + 1 < height; ++y) {
        const int start = (width - row_width) / 2;
        out.insert(out.end(), square.begin() + y * width + start,
                   square.begin() + y * width + start + row_width);
        row_width += inc;
        if (row_width == width) {
            inc = -step;
        }
    }
    return out;
}

std::vector<std::uint8_t> make_diamond_square(int width, int height) {
    std::vector<std::uint8_t> square(static_cast<std::size_t>(width) * height, 0);
    const int step = width / (height / 2);
    int row_width = step;
    int inc = step;
    for (int y = 0; y + 1 < height; ++y) {
        const int start = (width - row_width) / 2;
        for (int i = 0; i < row_width; ++i) {
            square[static_cast<std::size_t>(y) * width + start + i] =
                static_cast<std::uint8_t>(1 + ((y * 7 + i) % 254));
        }
        row_width += inc;
        if (row_width == width) {
            inc = -step;
        }
    }
    return square;
}

std::vector<std::uint8_t> make_synthetic_tmp(const std::vector<std::uint8_t>& square) {
    constexpr int kWidth = 60;
    constexpr int kHeight = 30;
    const auto image = encode_iso(square, kWidth, kHeight);
    const auto zdata = encode_iso(square, kWidth, kHeight);

    std::vector<std::uint8_t> out;
    const auto u32 = [&out](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
        }
    };
    u32(1);              // tilesX
    u32(1);              // tilesY
    u32(kWidth);         // tileWidth
    u32(kHeight);        // tileHeight
    u32(20);             // offset of the single cell header

    // Cell header (52 bytes).
    u32(0);              // x
    u32(0);              // y
    u32(0);              // extraOffset
    u32(52 + static_cast<std::uint32_t>(image.size()));  // zOffset
    u32(0);              // extraZOffset
    u32(0);              // extraX
    u32(0);              // extraY
    u32(0);              // extraW
    u32(0);              // extraH
    out.push_back(0);    // flags
    out.insert(out.end(), {0, 0, 0});
    out.push_back(7);    // height
    out.push_back(13);   // landType
    out.push_back(0);    // slopeType
    out.insert(out.end(), {0x11, 0x22, 0x33});  // radar left
    out.insert(out.end(), {0x44, 0x55, 0x66});  // radar right
    out.insert(out.end(), {0, 0, 0});
    CHECK(out.size() == 20 + 52);

    out.insert(out.end(), image.begin(), image.end());
    out.insert(out.end(), zdata.begin(), zdata.end());
    return out;
}

void test_tmp_roundtrip() {
    const auto square = make_diamond_square(60, 30);
    const auto bytes = make_synthetic_tmp(square);

    std::string error;
    auto tmp = ra2yr::formats::TmpFile::from_bytes(bytes, &error);
    CHECK(tmp.has_value());
    if (!tmp) {
        std::cerr << "tmp parse failed: " << error << "\n";
        return;
    }
    CHECK(tmp->tiles_x() == 1 && tmp->tiles_y() == 1);
    CHECK(tmp->tile_width() == 60 && tmp->tile_height() == 30);
    CHECK(tmp->tile_count() == 1);
    const auto& tile = tmp->tile(0);
    CHECK(tile.height == 7);
    CHECK(tile.land_type == 13);
    CHECK(tile.image == square);
    CHECK(tile.zdata == square);
    CHECK(tile.radar_left[0] == 0x11 && tile.radar_right[2] == 0x66);

    std::vector<std::uint8_t> pal_bytes(ra2yr::formats::Palette::kByteSize, 0);
    pal_bytes[7 * 3 + 0] = 100;
    auto palette = ra2yr::formats::Palette::from_bytes(pal_bytes, &error);
    CHECK(palette.has_value());
    if (palette) {
        const auto rgba = tmp->to_rgba(tile, *palette);
        CHECK(rgba.size() == square.size() * 4);
        CHECK(rgba[3] == 0);  // index 0 with alpha 0 where the square is empty
    }
}

}  // namespace

int main() {
    test_palette();
    test_iso_projection();
    test_tmp_roundtrip();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all format checks passed\n";
    return 0;
}
