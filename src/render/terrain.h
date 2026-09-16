// Terrain assembly: turn map cells plus theater tiles into one atlas and a
// list of instances ready for the renderer.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "formats/map.h"
#include "formats/palette.h"
#include "formats/theater.h"

namespace ra2yr::render {

// One axis-aligned tile quad in world pixels, with a sub-rectangle of the
// atlas and a painter-order key. The renderer sorts by `depth` ascending.
struct TileInstance {
    float x = 0.0f;
    float y = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
    int depth = 0;
};

struct TerrainAtlas {
    std::vector<std::uint8_t> rgba;
    int atlas_width = 0;
    int atlas_height = 0;
    int tile_width = 0;
    int tile_height = 0;
    std::vector<TileInstance> tiles;
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
};

// Build a single-tile grid, used by the standalone terrain preview.
TerrainAtlas build_grid_terrain(const std::vector<std::uint8_t>& tile_rgba, int tile_width,
                                int tile_height, int cols, int rows);

// Build terrain from a parsed map and the matching theater. Tile TMPs are read
// from `tiles_dir` by lowercase file name.
std::optional<TerrainAtlas> build_map_terrain(const formats::MapFile& map,
                                              const formats::Theater& theater,
                                              const std::filesystem::path& tiles_dir,
                                              const formats::Palette& palette,
                                              std::string* error = nullptr);

}  // namespace ra2yr::render
