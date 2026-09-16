// Base64 decoding for the map pack sections.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace ra2yr::formats {

std::vector<std::uint8_t> base64_decode(std::string_view text);

}  // namespace ra2yr::formats
