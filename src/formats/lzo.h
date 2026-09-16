// LZO1X block decompression.
//
// Ported from OpenTS code/lzo1x_d.cpp, which derives from the LZO library and
// Electronic Arts' GPL-released Command & Conquer source. The LZO library is
// free software; redistribution and modification are permitted under the terms
// of the GNU General Public License.
//
// The original routine assumes a destination buffer large enough for the whole
// block and performs no bounds checks; this port validates both input and
// output so malformed data cannot read or write past the buffers.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstddef>

namespace ra2yr::formats {

// Expand one LZO1X block. Returns the number of bytes written, or 0 if the
// stream is malformed or does not fit in `out_capacity`.
std::size_t lzo1x_decompress(const std::uint8_t* in, std::size_t in_len,
                             std::uint8_t* out, std::size_t out_capacity);

}  // namespace ra2yr::formats
