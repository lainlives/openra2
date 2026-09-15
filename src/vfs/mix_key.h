// MIX header key recovery.
//
// An encrypted MIX header stores an 80-byte "keysource": the 56-byte Blowfish
// key, split into two blocks and RSA-processed. Recovery needs only the public
// modulus below and the exponent 65537, so no private key or key file is
// required.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace ra2yr::vfs {

// Derive the 56-byte Blowfish key from an 80-byte MIX keysource.
void derive_blowfish_key(const std::uint8_t keysource[80], std::uint8_t key[56]);

}  // namespace ra2yr::vfs