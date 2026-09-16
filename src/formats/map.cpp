#include "formats/map.h"

#include <cstdio>
#include <cstring>

#include "formats/base64.h"
#include "formats/ini.h"
#include "formats/lzo.h"

namespace ra2yr::formats {
namespace {

constexpr std::size_t kRecordSize = 11;

std::uint16_t read_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1]) << 8;
}

std::int16_t read_i16(const std::uint8_t* p) {
    return static_cast<std::int16_t>(read_u16(p));
}

}  // namespace

std::vector<std::uint8_t> MapFile::unpack_iso_pack(const std::vector<std::uint8_t>& packed,
                                                   std::string* error) {
    std::vector<std::uint8_t> output;
    std::size_t pos = 0;
    while (pos + 4 <= packed.size()) {
        const std::uint16_t packed_len = read_u16(packed.data() + pos);
        const std::uint16_t unpacked_len = read_u16(packed.data() + pos + 2);
        pos += 4;
        if (pos + packed_len > packed.size()) {
            if (error != nullptr) {
                *error = "IsoMapPack5 block runs past the end of the data";
            }
            return {};
        }
        if (unpacked_len == 0) {
            pos += packed_len;
            continue;
        }
        const std::size_t base = output.size();
        output.resize(base + unpacked_len);
        const std::size_t written = lzo1x_decompress(
            packed.data() + pos, packed_len, output.data() + base, unpacked_len);
        if (written != unpacked_len) {
            output.resize(base);
            if (error != nullptr) {
                *error = "IsoMapPack5 block failed to decompress";
            }
            return {};
        }
        pos += packed_len;
    }
    return output;
}

std::vector<MapCell> MapFile::parse_iso_entries(const std::uint8_t* data, std::size_t size) {
    std::vector<MapCell> cells;
    cells.reserve(size / kRecordSize);
    for (std::size_t pos = 0; pos + kRecordSize <= size; pos += kRecordSize) {
        const std::uint8_t* record = data + pos;
        const std::int16_t x = read_i16(record + 0);
        const std::int16_t y = read_i16(record + 2);
        if (x < 0 || y < 0) {
            continue;
        }
        MapCell cell;
        cell.x = x;
        cell.y = y;
        cell.tile = read_u16(record + 4);
        cell.sub_tile = record[8];
        cell.z = record[9];
        cells.push_back(cell);
    }
    return cells;
}

std::optional<MapFile> MapFile::from_bytes(const std::vector<std::uint8_t>& data,
                                           std::string* error) {
    const auto fail = [error](const char* message) -> std::optional<MapFile> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    MapFile map;
    const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    const IniFile ini = IniFile::parse(text);

    if (const std::string* theater = ini.get("map", "theater")) {
        map.theater_ = *theater;
    }
    if (const std::string* size = ini.get("map", "size")) {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        if (std::sscanf(size->c_str(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4) {
            map.width_ = w;
            map.height_ = h;
        }
    }

    const IniFile::Entries* pack = ini.section("isomappack5");
    if (pack == nullptr) {
        return fail("map has no IsoMapPack5 section");
    }

    std::string base64;
    for (const IniFile::Entry& entry : *pack) {
        base64 += entry.second;
    }
    const std::vector<std::uint8_t> packed = base64_decode(base64);
    const std::vector<std::uint8_t> unpacked = unpack_iso_pack(packed, error);
    if (unpacked.empty()) {
        return std::nullopt;
    }
    map.cells_ = parse_iso_entries(unpacked.data(), unpacked.size());
    if (map.cells_.empty()) {
        return fail("map terrain has no cells");
    }
    return map;
}

}  // namespace ra2yr::formats
