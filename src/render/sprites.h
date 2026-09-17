// Map object sprites: resolve [Structures] entries to SHP images through
// art(md).ini and pack them into one atlas for a sprite pass over the terrain.
//
// This is deliberately a first pass: one frame per object, no animations, no
// house remapping, no foundation-aware anchor, and no unit voxels.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "formats/map.h"
#include "formats/palette.h"
#include "render/terrain.h"
#include "vfs/vfs.h"

namespace ra2yr::render {

struct ObjectAtlas {
    std::vector<std::uint8_t> rgba;
    int width = 0;
    int height = 0;
    std::vector<TileInstance> instances;
    std::size_t distinct_images = 0;
    std::size_t missing = 0;
};

// Build sprites for a map's [Structures]. Returns nullopt only on a hard error
// (no art file); a map with no resolvable structures yields empty instances.
std::optional<ObjectAtlas> build_map_objects(const formats::MapFile& map, vfs::Vfs& vfs,
                                             const formats::Palette& palette,
                                             int tile_width, int tile_height,
                                             std::string* error = nullptr);

}  // namespace ra2yr::render
