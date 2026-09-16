#include "formats/palette.h"

namespace ra2yr::formats {

std::optional<Palette> Palette::from_bytes(const std::vector<std::uint8_t>& data,
                                           std::string* error) {
    if (data.size() < kByteSize) {
        if (error != nullptr) {
            *error = "palette is too short";
        }
        return std::nullopt;
    }
    Palette palette;
    for (int i = 0; i < kColors; ++i) {
        Rgba& entry = palette.entries_[static_cast<std::size_t>(i)];
        entry.r = data[static_cast<std::size_t>(i) * 3 + 0];
        entry.g = data[static_cast<std::size_t>(i) * 3 + 1];
        entry.b = data[static_cast<std::size_t>(i) * 3 + 2];
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
