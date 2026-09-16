// Westwood 256-colour palette (VGA .pal).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ra2yr::formats {

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    bool operator==(const Rgba& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

class Palette {
public:
    static constexpr int kColors = 256;
    static constexpr std::size_t kByteSize = kColors * 3;

    // Parse a raw 768-byte palette. Index 0 is the transparent entry used by
    // sprites and the area outside a terrain tile diamond.
    static std::optional<Palette> from_bytes(const std::vector<std::uint8_t>& data,
                                             std::string* error = nullptr);

    Rgba color(int index) const;
    std::array<Rgba, kColors> colors() const;

private:
    std::array<Rgba, kColors> entries_{};
};

}  // namespace ra2yr::formats
