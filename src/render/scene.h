// Scene assembly: terrain tiles and map object sprites packed into one atlas
// and one depth-sorted instance list, so the renderer can draw everything in a
// single pass with correct overlap.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "formats/map.h"
#include "formats/palette.h"
#include "vfs/vfs.h"

namespace ra2yr::render {

// One axis-aligned quad in world pixels, with a sub-rectangle of the scene
// atlas and a painter-order key. The renderer sorts by `depth` ascending.
//
// Depth layout: diagonal * 64 + layer, where the diagonal is x + y (or the
// object's front corner), terrain uses layer = z (0..15), and objects use
// layer = 32 so they draw after the terrain under their footprint but before
// the next diagonal.
struct TileInstance {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
    int depth = 0;
};

struct Scene {
    std::vector<std::uint8_t> rgba;
    int width = 0;
    int height = 0;
    std::vector<TileInstance> instances;
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    std::size_t tile_count = 0;
    std::size_t sprite_count = 0;
    std::size_t missing_tiles = 0;
    std::size_t missing_sprites = 0;
};

// Load the theater palette for a map through the VFS, honouring an optional
// loose override. Opens the palette and localization mixes.
std::optional<formats::Palette> load_map_palette(const formats::MapFile& map,
                                                 vfs::Vfs& vfs,
                                                 const std::string& palette_override = {},
                                                 std::string* error = nullptr);

// Build a single-tile grid, used by the standalone terrain preview.
Scene build_grid_scene(const std::vector<std::uint8_t>& tile_rgba, int tile_width,
                       int tile_height, int cols, int rows);

// Build the full scene for a map: terrain tiles plus [Structures] sprites from
// art(md).ini/rules(md).ini, packed together. `theater_override` forces the
// theater used to resolve tiles (for prototype maps whose tile ids come from an
// older theater).
std::optional<Scene> build_map_scene(const formats::MapFile& map, vfs::Vfs& vfs,
                                     const formats::Palette& palette,
                                     const std::string& theater_override = {},
                                     std::string* error = nullptr);

}  // namespace ra2yr::render