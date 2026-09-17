#include "formats/palette.h"

#include <algorithm>

namespace ra2yr::formats {
namespace {

// Westwood VGA palettes store 6-bit DAC values (0-63). Expand to 8-bit by bit
// replication; an all-values-<=63 palette is treated as 6-bit, anything else as
// already 8-bit.
std::uint8_t expand6(std::uint8_t value) {
    return static_cast<std::uint8_t>((value << 2) | (value >> 4));
}

}  // namespace

std::optional<Palette> Palette::from_bytes(const std::vector<std::uint8_t>& data,
                                           std::string* error) {
    if (data.size() < kByteSize) {
        if (error != nullptr) {
            *error = "palette is too short";
        }
        return std::nullopt;
    }
    const bool six_bit =
        std::all_of(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(kByteSize),
                    [](std::uint8_t value) { return value <= 63; });

    Palette palette;
    for (int i = 0; i < kColors; ++i) {
        Rgba& entry = palette.entries_[static_cast<std::size_t>(i)];
        const std::uint8_t r = data[static_cast<std::size_t>(i) * 3 + 0];
        const std::uint8_t g = data[static_cast<std::size_t>(i) * 3 + 1];
        const std::uint8_t b = data[static_cast<std::size_t>(i) * 3 + 2];
        entry.r = six_bit ? expand6(r) : r;
        entry.g = six_bit ? expand6(g) : g;
        entry.b = six_bit ? expand6(b) : b;
        entry.a = (i == 0) ? 0 : 255;
    }
    return palette;
}

Rgba Palette::color(int index) const {
    if (index < 0 || index >= kColors) {
        return Rgba{0, 0, 0, 0};
    }
    return entries_[static_cast<std::size_t>(index)];
}

std::array<Rgba, Palette::kColors> Palette::colors() const {
    return entries_;
}

}  // namespace ra2yr::formats
