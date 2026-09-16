// Isometric projection and draw ordering.
//
// A map cell (cx, cy) maps to screen pixels with the standard 2:1 diamond:
//   x = (cx - cy) * tile_width  / 2
//   y = (cx + cy) * tile_height / 2
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

namespace ra2yr::render {

struct IsoCell {
    int cx = 0;
    int cy = 0;
};

struct IsoPoint {
    int x = 0;
    int y = 0;
};

inline IsoPoint cell_to_screen(int cx, int cy, int tile_width, int tile_height) {
    return IsoPoint{(cx - cy) * (tile_width / 2), (cx + cy) * (tile_height / 2)};
}

// Build a back-to-front draw order for a cols x rows grid: cell (0,0) is the
// top of the diamond, and cells are emitted by increasing cx+cy so nearer
// cells overwrite farther ones.
std::vector<IsoCell> build_iso_order(int cols, int rows);

}  // namespace ra2yr::render
