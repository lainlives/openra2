// Theater tile control file ([TileSetNNNN] sections).
//
// The map's tile field is a global tile id assigned by walking the theater's
// tilesets in order. Each tileset names a TMP file base plus a count; the
// tile's file is `<FileName><NN>` within the set.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "formats/ini.h"

namespace ra2yr::formats {

struct TheaterTileSet {
    int base_id = 0;
    int count = 0;
    std::string file_name;
};

// Static per-theater names, matching the YR engine's Theater table: the tile
// control INI, the isometric tile MIX, the palette, and the tile extension.
struct TheaterInfo {
    const char* id;
    const char* control;   // e.g. "urban"    -> urbanmd.ini / urban.ini
    const char* art;       // e.g. "isourb"   -> isourb.mix
    const char* palette;   // building/iso palette, e.g. "isourb" -> isourb.pal
    const char* terrain;   // terrain palette, e.g. "urban" -> urban.pal
    const char* extension; // e.g. "urb"; also the suffix for unit/city/lib palettes
};

const TheaterInfo* theater_info(std::string_view theater_name);

class Theater {
public:
    static Theater from_ini(const IniFile& ini);

    // Resolve a global tile id to a TMP file base and the tile's index within
    // the set (0-based).
    bool resolve(std::uint16_t tile_id, std::string* file_base, int* index_in_set) const;

    // Theater name to TMP extension: TEMPERATE -> "tem", SNOW -> "sno", ...
    static const char* tile_suffix(std::string_view theater_name);

    const std::vector<TheaterTileSet>& sets() const { return sets_; }

private:
    std::vector<TheaterTileSet> sets_;
};

}  // namespace ra2yr::formats
