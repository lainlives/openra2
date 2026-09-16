#include "render/iso.h"

#include <algorithm>

namespace ra2yr::render {

std::vector<IsoCell> build_iso_order(int cols, int rows) {
    std::vector<IsoCell> cells;
    if (cols <= 0 || rows <= 0) {
        return cells;
    }
    cells.reserve(static_cast<std::size_t>(cols) * rows);
    for (int cy = 0; cy < rows; ++cy) {
        for (int cx = 0; cx < cols; ++cx) {
            cells.push_back(IsoCell{cx, cy});
        }
    }
    std::stable_sort(cells.begin(), cells.end(), [](const IsoCell& a, const IsoCell& b) {
        const int depth_a = a.cx + a.cy;
        const int depth_b = b.cx + b.cy;
        if (depth_a != depth_b) {
            return depth_a < depth_b;
        }
        return a.cx < b.cx;
    });
    return cells;
}

}  // namespace ra2yr::render
