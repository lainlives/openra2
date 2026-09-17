// Red Alert 2 / Yuri's Revenge map reader (.map, .mpr, .yrm).
//
// The terrain is an INI section, IsoMapPack5, whose base64 values decode to a
// sequence of LZO1X blocks. Each 11-byte record is a cell's position, global
// tile id, sub-tile, and Z level.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ra2yr::formats {

struct MapCell {
    int x = 0;
    int y = 0;
    std::uint16_t tile = 0;
    std::uint8_t sub_tile = 0;
    std::uint8_t z = 0;
};

class MapFile {
public:
    static std::optional<MapFile> from_bytes(const std::vector<std::uint8_t>& data,
                                             std::string* error = nullptr);

    // Decode an IsoMapPack5 payload (chunk headers + LZO blocks) into raw cell
    // records. Exposed for tests.
    static std::vector<std::uint8_t> unpack_iso_pack(const std::vector<std::uint8_t>& packed,
                                                     std::string* error = nullptr);
    static std::vector<MapCell> parse_iso_entries(const std::uint8_t* data,
                                                  std::size_t size);

    using Entry = std::pair<std::string, std::string>;
    using Section = std::vector<Entry>;

    const Section& structures() const { return structures_; }
    const std::string& theater() const { return theater_; }
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<MapCell>& cells() const { return cells_; }

private:
    std::string theater_;
    int width_ = 0;
    int height_ = 0;
    std::vector<MapCell> cells_;
    Section structures_;
};

}  // namespace ra2yr::formats
