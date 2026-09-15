#pragma once

#include <string>

namespace ra2yr {

inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;
inline constexpr const char* kVersionString = "0.1.0";

// Human-readable one-line build identity, e.g. "ra2yr 0.1.0 (engine scaffold)".
std::string build_summary();

}  // namespace ra2yr
