// Blowfish block cipher, matching the Westwood/EA engine implementation.
//
// The algorithm and tables derive from OpenTS code/blowfish.cpp, which derives
// from Electronic Arts' GPL-released Command & Conquer source. Blowfish itself
// is in the public domain. This port removes the original's Windows/COM
// dependencies and the interface is modernised; the byte order and key
// schedule are preserved exactly.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace ra2yr::vfs {

class Blowfish {
public:
    static constexpr int kRounds = 16;
    static constexpr int kBytesPerBlock = 8;
    static constexpr int kMaxKeyLength = 56;

    Blowfish() = default;

    // Submit a key of at most 56 bytes. A null/zero-length key clears the
    // engine, after which Encrypt/Decrypt copy their input unchanged.
    void set_key(const void* key, int length);
    bool keyed() const { return keyed_; }

    // Process length bytes in 8-byte blocks. A trailing partial block is
    // copied through unprocessed. out may alias in.
    void encrypt(const void* in, int length, void* out) const;
    void decrypt(const void* in, int length, void* out) const;

private:
    void process_block(const std::uint8_t* in, std::uint8_t* out,
                       const std::uint32_t* ptable) const;
    void sub_key_encrypt(std::uint32_t& left, std::uint32_t& right) const;

    bool keyed_ = false;
    std::uint32_t p_encrypt_[kRounds + 2]{};
    std::uint32_t p_decrypt_[kRounds + 2]{};
    std::uint32_t s_[4][256]{};
};

}  // namespace ra2yr::vfs