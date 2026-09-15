#include "blowfish.h"

#include <cstring>

#include "blowfish_tables.h"

namespace ra2yr::vfs {
namespace {

constexpr int kPCount = Blowfish::kRounds + 2;

std::uint32_t load_be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

void store_be32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v >> 24);
    p[1] = static_cast<std::uint8_t>(v >> 16);
    p[2] = static_cast<std::uint8_t>(v >> 8);
    p[3] = static_cast<std::uint8_t>(v);
}

}  // namespace

void Blowfish::set_key(const void* key, int length) {
    if (length > kMaxKeyLength) {
        length = kMaxKeyLength;
    }

    std::memcpy(p_encrypt_, kBfPInit, sizeof(p_encrypt_));
    std::memcpy(p_decrypt_, kBfPInit, sizeof(p_decrypt_));
    std::memcpy(s_, kBfSInit, sizeof(s_));

    if (key == nullptr || length <= 0) {
        keyed_ = false;
        return;
    }

    const auto* key_bytes = static_cast<const std::uint8_t*>(key);
    int j = 0;
    for (int index = 0; index < kPCount; ++index) {
        std::uint32_t data = 0;
        data = (data << 8) | key_bytes[j++ % length];
        data = (data << 8) | key_bytes[j++ % length];
        data = (data << 8) | key_bytes[j++ % length];
        data = (data << 8) | key_bytes[j++ % length];
        p_encrypt_[index] ^= data;
    }

    std::uint32_t left = 0;
    std::uint32_t right = 0;
    int p_en = 0;
    int p_de = kPCount - 1;
    for (int index = 0; index < kPCount; index += 2) {
        sub_key_encrypt(left, right);
        p_encrypt_[p_en++] = left;
        p_encrypt_[p_en++] = right;
        p_decrypt_[p_de--] = left;
        p_decrypt_[p_de--] = right;
    }

    for (int box = 0; box < 4; ++box) {
        for (int i = 0; i < 256; i += 2) {
            sub_key_encrypt(left, right);
            s_[box][i] = left;
            s_[box][i + 1] = right;
        }
    }

    keyed_ = true;
}

void Blowfish::sub_key_encrypt(std::uint32_t& left, std::uint32_t& right) const {
    std::uint32_t l = left;
    std::uint32_t r = right;
    for (int index = 0; index < kRounds; index += 2) {
        l ^= p_encrypt_[index];
        r ^= ((s_[0][(l >> 24) & 0xFF] + s_[1][(l >> 16) & 0xFF]) ^
              s_[2][(l >> 8) & 0xFF]) +
             s_[3][l & 0xFF];
        r ^= p_encrypt_[index + 1];
        l ^= ((s_[0][(r >> 24) & 0xFF] + s_[1][(r >> 16) & 0xFF]) ^
              s_[2][(r >> 8) & 0xFF]) +
             s_[3][r & 0xFF];
    }
    left = r ^ p_encrypt_[kRounds + 1];
    right = l ^ p_encrypt_[kRounds];
}

void Blowfish::process_block(const std::uint8_t* in, std::uint8_t* out,
                             const std::uint32_t* ptable) const {
    std::uint32_t left = load_be32(in);
    std::uint32_t right = load_be32(in + 4);

    const std::uint32_t* p = ptable;
    for (int index = 0; index < kRounds / 2; ++index) {
        left ^= *p++;
        right ^= ((s_[0][(left >> 24) & 0xFF] + s_[1][(left >> 16) & 0xFF]) ^
                  s_[2][(left >> 8) & 0xFF]) +
                 s_[3][left & 0xFF];
        right ^= *p++;
        left ^= ((s_[0][(right >> 24) & 0xFF] + s_[1][(right >> 16) & 0xFF]) ^
                 s_[2][(right >> 8) & 0xFF]) +
                s_[3][right & 0xFF];
    }
    left ^= *p++;
    right ^= *p;

    store_be32(out, right);
    store_be32(out + 4, left);
}

void Blowfish::encrypt(const void* in, int length, void* out) const {
    if (in == nullptr || length <= 0) {
        return;
    }
    if (out == nullptr) {
        out = const_cast<void*>(in);
    }
    if (!keyed_) {
        if (in != out) {
            std::memmove(out, in, static_cast<std::size_t>(length));
        }
        return;
    }
    const auto* src = static_cast<const std::uint8_t*>(in);
    auto* dst = static_cast<std::uint8_t*>(out);
    const int blocks = length / kBytesPerBlock;
    for (int i = 0; i < blocks; ++i) {
        process_block(src, dst, p_encrypt_);
        src += kBytesPerBlock;
        dst += kBytesPerBlock;
    }
    const int processed = blocks * kBytesPerBlock;
    if (processed < length) {
        std::memmove(dst, src, static_cast<std::size_t>(length - processed));
    }
}

void Blowfish::decrypt(const void* in, int length, void* out) const {
    if (in == nullptr || length <= 0) {
        return;
    }
    if (out == nullptr) {
        out = const_cast<void*>(in);
    }
    if (!keyed_) {
        if (in != out) {
            std::memmove(out, in, static_cast<std::size_t>(length));
        }
        return;
    }
    const auto* src = static_cast<const std::uint8_t*>(in);
    auto* dst = static_cast<std::uint8_t*>(out);
    const int blocks = length / kBytesPerBlock;
    for (int i = 0; i < blocks; ++i) {
        process_block(src, dst, p_decrypt_);
        src += kBytesPerBlock;
        dst += kBytesPerBlock;
    }
    const int processed = blocks * kBytesPerBlock;
    if (processed < length) {
        std::memmove(dst, src, static_cast<std::size_t>(length - processed));
    }
}

}  // namespace ra2yr::vfs