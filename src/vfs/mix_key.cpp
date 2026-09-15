#include "mix_key.h"

#include <array>
#include <cstddef>

namespace ra2yr::vfs {
namespace {

// Westwood's public MIX modulus, big-endian.
constexpr std::uint8_t kModulusBe[40] = {
    0x51, 0xBC, 0xDA, 0x08, 0x6D, 0x39, 0xFC, 0xE4, 0x56, 0x51,
    0x60, 0xD6, 0x51, 0x71, 0x3F, 0xA2, 0xE8, 0xAA, 0x54, 0xFA,
    0x66, 0x82, 0xB0, 0x4A, 0xAB, 0xDD, 0x0E, 0x6A, 0xF8, 0xB0,
    0xC1, 0xE6, 0xD1, 0xFB, 0x4F, 0x3D, 0xAA, 0x43, 0x7F, 0x15,
};

constexpr std::uint32_t kPublicExponent = 65537;

// Fixed-size unsigned integer, little-endian 32-bit limbs. Large enough for a
// product of two 352-bit operands.
constexpr int kLimbs = 24;

struct Big {
    std::array<std::uint32_t, kLimbs> w{};
    int n = 0;

    void normalize() {
        while (n > 0 && w[static_cast<std::size_t>(n - 1)] == 0) {
            --n;
        }
    }
};

Big from_be_bytes(const std::uint8_t* p, int len) {
    Big r;
    for (int i = 0; i < len; ++i) {
        const int pos = len - 1 - i;
        r.w[static_cast<std::size_t>(pos / 4)] |=
            static_cast<std::uint32_t>(p[i]) << (8 * (pos % 4));
    }
    r.n = (len + 3) / 4;
    r.normalize();
    return r;
}

int cmp(const Big& a, const Big& b) {
    const int len = a.n > b.n ? a.n : b.n;
    for (int i = len - 1; i >= 0; --i) {
        const std::uint32_t av = i < a.n ? a.w[static_cast<std::size_t>(i)] : 0;
        const std::uint32_t bv = i < b.n ? b.w[static_cast<std::size_t>(i)] : 0;
        if (av < bv) return -1;
        if (av > bv) return 1;
    }
    return 0;
}

Big sub(const Big& a, const Big& b) {
    Big r;
    std::uint64_t borrow = 0;
    const int len = a.n;
    for (int i = 0; i < len; ++i) {
        const std::uint64_t av = a.w[static_cast<std::size_t>(i)];
        const std::uint64_t bv = i < b.n ? b.w[static_cast<std::size_t>(i)] : 0;
        const std::uint64_t cur = av - bv - borrow;
        r.w[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(cur);
        borrow = (cur >> 32) & 1;
    }
    r.n = len;
    r.normalize();
    return r;
}

Big shl1(const Big& a) {
    Big r;
    std::uint32_t carry = 0;
    for (int i = 0; i < a.n; ++i) {
        const std::uint32_t v = a.w[static_cast<std::size_t>(i)];
        r.w[static_cast<std::size_t>(i)] = (v << 1) | carry;
        carry = v >> 31;
    }
    r.n = a.n;
    if (carry) {
        r.w[static_cast<std::size_t>(r.n)] = carry;
        ++r.n;
    }
    return r;
}

Big mul(const Big& a, const Big& b) {
    Big r;
    if (a.n == 0 || b.n == 0) {
        return r;
    }
    for (int i = 0; i < a.n; ++i) {
        std::uint64_t carry = 0;
        const std::uint64_t av = a.w[static_cast<std::size_t>(i)];
        for (int j = 0; j < b.n; ++j) {
            const std::size_t k = static_cast<std::size_t>(i + j);
            const std::uint64_t cur =
                static_cast<std::uint64_t>(r.w[k]) + av * b.w[static_cast<std::size_t>(j)] + carry;
            r.w[k] = static_cast<std::uint32_t>(cur);
            carry = cur >> 32;
        }
        std::size_t k = static_cast<std::size_t>(i + b.n);
        while (carry != 0 && k < static_cast<std::size_t>(kLimbs)) {
            const std::uint64_t cur = static_cast<std::uint64_t>(r.w[k]) + carry;
            r.w[k] = static_cast<std::uint32_t>(cur);
            carry = cur >> 32;
            ++k;
        }
    }
    r.n = a.n + b.n;
    if (r.n > kLimbs) {
        r.n = kLimbs;
    }
    r.normalize();
    return r;
}

// Binary long division remainder.
Big mod(const Big& dividend, const Big& divisor) {
    Big rem;
    if (divisor.n == 0) {
        return rem;
    }
    int bits = dividend.n * 32;
    if (bits == 0) {
        return rem;
    }
    while (bits > 0 && (dividend.w[static_cast<std::size_t>((bits - 1) / 32)] &
                        (1u << ((bits - 1) % 32))) == 0) {
        --bits;
    }
    for (int i = bits - 1; i >= 0; --i) {
        rem = shl1(rem);
        if ((dividend.w[static_cast<std::size_t>(i / 32)] >> (i % 32)) & 1u) {
            rem.w[0] |= 1u;
            if (rem.n == 0) {
                rem.n = 1;
            }
        }
        rem.normalize();
        if (cmp(rem, divisor) >= 0) {
            rem = sub(rem, divisor);
        }
    }
    return rem;
}

Big mulmod(const Big& a, const Big& b, const Big& m) {
    return mod(mul(a, b), m);
}

Big modpow(Big base, std::uint32_t exp, const Big& m) {
    Big result;
    result.w[0] = 1;
    result.n = 1;
    base = mod(base, m);
    while (exp != 0) {
        if (exp & 1u) {
            result = mulmod(result, base, m);
        }
        base = mulmod(base, base, m);
        exp >>= 1;
    }
    return result;
}

// Encode the low `out_len` bytes of `value` in big-endian order.
void to_be_bytes(const Big& value, std::uint8_t* out, int out_len) {
    for (int i = 0; i < out_len; ++i) {
        const int pos = out_len - 1 - i;
        out[pos] = static_cast<std::uint8_t>(
            (value.w[static_cast<std::size_t>(i / 4)] >> (8 * (i % 4))) & 0xFF);
    }
}

}  // namespace

void derive_blowfish_key(const std::uint8_t keysource[80], std::uint8_t key[56]) {
    // Reverse the keysource so it can be read as two big-endian blocks.
    std::uint8_t reversed[80];
    for (int i = 0; i < 80; ++i) {
        reversed[i] = keysource[79 - i];
    }

    const Big modulus = from_be_bytes(kModulusBe, 40);
    const Big block1 = from_be_bytes(reversed, 40);
    const Big block2 = from_be_bytes(reversed + 40, 40);

    const Big dec1 = modpow(block1, kPublicExponent, modulus);
    const Big dec2 = modpow(block2, kPublicExponent, modulus);

    // blow = (dec1 << 312) + dec2, as a 56-byte big-endian integer.
    // dec2 occupies the low 40 bytes; dec1's least significant byte sits at
    // byte 16 (a 39-byte shift), and the two do not overlap for valid keys.
    std::uint8_t blow[56] = {};
    std::uint8_t d1_be[40];
    std::uint8_t d2_be[40];
    to_be_bytes(dec1, d1_be, 40);
    to_be_bytes(dec2, d2_be, 40);

    for (int i = 0; i < 40; ++i) {
        blow[16 + i] = d2_be[i];
    }

    int carry = 0;
    for (int i = 0; i < 40; ++i) {
        const int pos = 16 - i;
        if (pos < 0) {
            break;
        }
        const int val = blow[pos] + d1_be[39 - i] + carry;
        blow[pos] = static_cast<std::uint8_t>(val & 0xFF);
        carry = val >> 8;
    }

    // Reverse the big-endian result into the little-endian Blowfish key.
    for (int i = 0; i < 56; ++i) {
        key[i] = blow[55 - i];
    }
}

}  // namespace ra2yr::vfs