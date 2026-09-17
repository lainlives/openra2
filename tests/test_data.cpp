#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../src/formats/map.h"
#include "../src/formats/shp.h"
#include "../src/vfs/mix.h"

#ifndef RA2YR_TEST_DATA_DIR
#define RA2YR_TEST_DATA_DIR "tests/data"
#endif

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

std::string data_path(const std::string& relative) {
    return std::string(RA2YR_TEST_DATA_DIR) + "/" + relative;
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

ra2yr::formats::Palette test_palette() {
    std::vector<std::uint8_t> bytes(ra2yr::formats::Palette::kByteSize, 0);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>((i * 255) / bytes.size());
    }
    bytes[0] = 0;
    std::string error;
    auto palette = ra2yr::formats::Palette::from_bytes(bytes, &error);
    return palette ? *palette : ra2yr::formats::Palette{};
}

struct ShpExpectation {
    const char* name;
    std::size_t frames;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t compression;
};

void test_shp_fixtures() {
    const ShpExpectation expectations[] = {
        {"1frame-uncompressed.shp", 1, 96, 96, 1},
        {"1frame-compressed.shp", 1, 96, 96, 3},
        {"uncompressed-4frame.shp", 4, 8, 6, 1},
        {"compressed-8frame-simple.shp", 8, 96, 96, 3},
    };
    const ra2yr::formats::Palette palette = test_palette();

    for (const ShpExpectation& expected : expectations) {
        const auto bytes = read_file(data_path(std::string("shp/") + expected.name));
        CHECK(!bytes.empty());
        if (bytes.empty()) {
            continue;
        }
        std::string error;
        auto shp = ra2yr::formats::ShpFile::from_bytes(bytes, &error);
        CHECK(shp.has_value());
        if (!shp) {
            std::cerr << expected.name << ": " << error << "\n";
            continue;
        }
        CHECK(shp->frame_count() == expected.frames);
        CHECK(shp->width() == expected.width);
        CHECK(shp->height() == expected.height);
        for (std::size_t i = 0; i < shp->frame_count(); ++i) {
            const auto& frame = shp->frame(i);
            CHECK(frame.compression == expected.compression);
            const auto rgba = shp->to_rgba(frame, palette);
            CHECK(rgba.size() ==
                  static_cast<std::size_t>(shp->width()) * shp->height() * 4);
        }
    }
}

std::optional<ra2yr::formats::MapFile> load_map(const std::string& relative) {
    std::vector<std::uint8_t> bytes = read_file(data_path(relative));
    if (bytes.empty()) {
        return std::nullopt;
    }
    if (auto member = ra2yr::vfs::extract_mix_member(
            bytes, [](std::string_view name, const std::vector<std::uint8_t>& data) {
                if (name.size() >= 4 && name.substr(name.size() - 4) == ".map") {
                    return true;
                }
                const std::string text(data.begin(), data.end());
                return text.find("[IsoMapPack5]") != std::string::npos;
            })) {
        bytes = std::move(*member);
    }
    std::string error;
    return ra2yr::formats::MapFile::from_bytes(bytes, &error);
}

void test_map_fixtures() {
    auto scripting = load_map("maps/test_80x80_scripting+lighting.yrm");
    CHECK(scripting.has_value());
    if (scripting) {
        CHECK(scripting->theater() == "TEMPERATE");
        CHECK(!scripting->cells().empty());
        CHECK(!scripting->structures().empty());
    }

    auto plain = load_map("maps/test_newurban.yrm");
    auto packed = load_map("maps/test_newurban.yro");
    CHECK(plain.has_value());
    CHECK(packed.has_value());
    if (plain && packed) {
        // The packaged map must expand to exactly the plain one.
        CHECK(plain->theater() == packed->theater());
        CHECK(plain->width() == packed->width());
        CHECK(plain->height() == packed->height());
        CHECK(plain->cells().size() == packed->cells().size());
        CHECK(plain->structures().size() == packed->structures().size());
    }

    auto large = load_map("maps/test_vanilla_engine_limit_256x256.mpr");
    CHECK(large.has_value());
    if (large) {
        CHECK(!large->cells().empty());
        // The declared Size is smaller than the cell extent; the cells are the
        // authority on how big the map really is.
        int max_coord = 0;
        for (const auto& cell : large->cells()) {
            max_coord = std::max(max_coord, std::max(cell.x, cell.y));
        }
        CHECK(max_coord > 200);
    }
}

void test_csf_fixture() {
    const auto bytes = read_file(data_path("strings/test.csf"));
    CHECK(bytes.size() >= 24);
    if (bytes.size() < 24) {
        return;
    }
    CHECK(std::memcmp(bytes.data(), " FSC", 4) == 0);
    std::uint32_t version = 0;
    std::uint32_t strings = 0;
    std::uint32_t tags = 0;
    std::memcpy(&version, bytes.data() + 4, 4);
    std::memcpy(&strings, bytes.data() + 8, 4);
    std::memcpy(&tags, bytes.data() + 12, 4);
    CHECK(version == 3);
    CHECK(strings == 2);
    CHECK(tags == 2);
}

}  // namespace

int main() {
    test_shp_fixtures();
    test_map_fixtures();
    test_csf_fixture();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all data checks passed\n";
    return 0;
}