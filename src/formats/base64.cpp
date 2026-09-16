#include "formats/base64.h"

#include <array>

namespace ra2yr::formats {
namespace {

constexpr std::array<std::int8_t, 256> make_table() {
    std::array<std::int8_t, 256> table{};
    for (auto& v : table) {
        v = -1;
    }
    const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) {
        table[static_cast<std::size_t>(static_cast<unsigned char>(alphabet[i]))] =
            static_cast<std::int8_t>(i);
    }
    return table;
}

constexpr std::array<std::int8_t, 256> kTable = make_table();

}  // namespace

std::vector<std::uint8_t> base64_decode(std::string_view text) {
    std::vector<std::uint8_t> out;
    out.reserve(text.size() * 3 / 4);
    std::uint32_t accum = 0;
    int bits = 0;
    for (char c : text) {
        const std::int8_t value =
            kTable[static_cast<std::size_t>(static_cast<unsigned char>(c))];
        if (value < 0) {
            continue;
        }
        accum = (accum << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((accum >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace ra2yr::formats
