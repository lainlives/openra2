#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../src/formats/base64.h"
#include "../src/formats/ini.h"
#include "../src/formats/map.h"
#include "../src/formats/palette.h"
#include "../src/formats/shp.h"
#include "../src/formats/theater.h"
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
    // 6-bit DAC values are expanded to 8-bit by bit replication.
    CHECK(transparent.r == 40 && transparent.g == 81 && transparent.b == 121);
    CHECK(transparent.a == 0);
    const auto five = palette->color(5);
    CHECK(five.r == 162 && five.g == 203 && five.b == 243 && five.a == 255);

    // A palette with 8-bit values passes through unchanged.
    std::vector<std::uint8_t> eight_bit(ra2yr::formats::Palette::kByteSize, 0);
    eight_bit[3 * 3 + 0] = 200;
    eight_bit[3 * 3 + 1] = 128;
    eight_bit[3 * 3 + 2] = 64;
    auto wide = ra2yr::formats::Palette::from_bytes(eight_bit, &error);
    CHECK(wide.has_value());
    if (wide) {
        const auto color = wide->color(3);
        CHECK(color.r == 200 && color.g == 128 && color.b == 64);
    }
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

void test_base64() {
    const auto hello = ra2yr::formats::base64_decode("aGVsbG8=");
    CHECK(std::string(hello.begin(), hello.end()) == "hello");
    CHECK(ra2yr::formats::base64_decode("").empty());
}

void test_ini() {
    const char* text =
        "; comment\n[Map]\nTheater=URBAN\nSize=0,0,50,64\n[IsoMapPack5]\n1=aa\n2=bb\n";
    auto ini = ra2yr::formats::IniFile::parse(text);
    const std::string* theater = ini.get("map", "theater");
    CHECK(theater != nullptr && *theater == "URBAN");
    CHECK(ini.get_int("map", "size", -1) == 0);
    const auto* pack = ini.section("isomappack5");
    CHECK(pack != nullptr && pack->size() == 2);
    if (pack != nullptr && pack->size() == 2) {
        CHECK((*pack)[0].first == "1" && (*pack)[0].second == "aa");
        CHECK((*pack)[1].second == "bb");
    }
}

void test_theater() {
    const char* text =
        "[TileSet0000]\nFileName = Clear\nTilesInSet = 2\n"
        "[TileSet0001]\nFileName = blank\nTilesInSet = 0\n"
        "[TileSet0002]\nFileName = Rock\nTilesInSet = 3\n";
    auto theater =
        ra2yr::formats::Theater::from_ini(ra2yr::formats::IniFile::parse(text));
    CHECK(theater.sets().size() == 3);

    std::string base;
    int index = -1;
    CHECK(theater.resolve(0, &base, &index) && base == "Clear" && index == 0);
    CHECK(theater.resolve(1, &base, &index) && base == "Clear" && index == 1);
    CHECK(theater.resolve(2, &base, &index) && base == "Rock" && index == 0);
    CHECK(theater.resolve(4, &base, &index) && base == "Rock" && index == 2);
    CHECK(!theater.resolve(5, &base, &index));

    CHECK(std::string(ra2yr::formats::Theater::tile_suffix("URBAN")) == "urb");
    CHECK(ra2yr::formats::Theater::tile_suffix("NOPE") == nullptr);
}

void test_map_entries() {
    std::vector<std::uint8_t> records(22, 0);
    const auto put16 = [&records](std::size_t offset, std::uint16_t value) {
        records[offset] = static_cast<std::uint8_t>(value & 0xFF);
        records[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    };
    put16(0, 1);
    put16(2, 2);
    put16(4, 3);
    records[8] = 4;
    records[9] = 5;
    put16(11, 7);
    put16(13, 8);
    put16(15, 9);
    records[19] = 1;

    auto cells =
        ra2yr::formats::MapFile::parse_iso_entries(records.data(), records.size());
    CHECK(cells.size() == 2);
    if (cells.size() == 2) {
        CHECK(cells[0].x == 1 && cells[0].y == 2 && cells[0].tile == 3);
        CHECK(cells[0].sub_tile == 4 && cells[0].z == 5);
        CHECK(cells[1].x == 7 && cells[1].y == 8 && cells[1].tile == 9);
    }

    // CELL_NONE-style negative coordinates are skipped.
    put16(0, 0xFFFF);
    put16(2, 0xFFFF);
    auto filtered =
        ra2yr::formats::MapFile::parse_iso_entries(records.data(), records.size());
    CHECK(filtered.size() == 1);
}

void test_shp() {
    const auto u16 = [](std::vector<std::uint8_t>& out, std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>(v >> 8));
    };
    const auto u32 = [](std::vector<std::uint8_t>& out, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
        }
    };

    // Two 4x2 frames: one raw, one RLE-Zero.
    std::vector<std::uint8_t> shp;
    u16(shp, 0);
    u16(shp, 4);
    u16(shp, 2);
    u16(shp, 2);
    const std::uint32_t raw_offset = 8 + 2 * 24;
    const std::vector<std::uint8_t> raw = {5, 6, 7, 8, 9, 10, 11, 12};
    // Frame 0: raw.
    u16(shp, 0);
    u16(shp, 0);
    u16(shp, 4);
    u16(shp, 2);
    shp.push_back(1);  // compression
    shp.push_back(0);
    u16(shp, 0);
    shp.insert(shp.end(), {1, 2, 3, 0});  // radar colour
    u32(shp, 0);                          // reserved
    u32(shp, raw_offset);
    // Frame 1: RLE-Zero. Row 0 = 1,2,0,0 ; row 1 = 0,0,3,4
    const std::vector<std::uint8_t> rle = {6, 0, 1, 2, 0, 2, 6, 0, 0, 2, 3, 4};
    u16(shp, 0);
    u16(shp, 0);
    u16(shp, 4);
    u16(shp, 2);
    shp.push_back(3);  // compression
    shp.push_back(0);
    u16(shp, 0);
    shp.insert(shp.end(), {0, 0, 0, 0});
    u32(shp, 0);
    u32(shp, raw_offset + static_cast<std::uint32_t>(raw.size()));
    shp.insert(shp.end(), raw.begin(), raw.end());
    shp.insert(shp.end(), rle.begin(), rle.end());

    std::string error;
    auto parsed = ra2yr::formats::ShpFile::from_bytes(shp, &error);
    CHECK(parsed.has_value());
    if (!parsed) {
        std::cerr << "shp parse failed: " << error << "\n";
        return;
    }
    CHECK(parsed->width() == 4 && parsed->height() == 2);
    CHECK(parsed->frame_count() == 2);
    CHECK(parsed->frame(0).pixels == raw);
    CHECK(parsed->frame(0).radar_color[1] == 2);
    const std::vector<std::uint8_t> expected = {1, 2, 0, 0, 0, 0, 3, 4};
    CHECK(parsed->frame(1).pixels == expected);

    std::vector<std::uint8_t> pal_bytes(ra2yr::formats::Palette::kByteSize, 0);
    pal_bytes[5 * 3 + 0] = 200;
    auto palette = ra2yr::formats::Palette::from_bytes(pal_bytes, &error);
    CHECK(palette.has_value());
    if (palette) {
        const auto rgba = parsed->to_rgba(parsed->frame(0), *palette);
        CHECK(rgba.size() == 4 * 2 * 4);
        CHECK(rgba[5 * 4 + 3] == 255);  // index 5 opaque
    }
    // Frame 1 begins with index 1 (opaque) and index 0 (transparent).
    if (palette) {
        const auto rgba = parsed->to_rgba(parsed->frame(1), *palette);
        CHECK(rgba[0] == 0 && rgba[3] == 255);  // index 1 -> palette black, opaque
        CHECK(rgba[2 * 4 + 3] == 0);            // index 0 -> transparent
    }
}

}  // namespace

int main() {
    test_palette();
    test_iso_projection();
    test_tmp_roundtrip();
    test_base64();
    test_ini();
    test_theater();
    test_map_entries();
    test_shp();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all format checks passed\n";
    return 0;
}
