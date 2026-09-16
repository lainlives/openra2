// Virtual file system over a retail install.
//
// The game resolves assets by name: compute the Westwood filename hash and ask
// the registered MIX archives. Nested archives (the theater tile mixes, the
// localization mixes, ...) are opened on demand by name, exactly as the engine
// does at startup, so no filename database is needed at runtime.
//
// Lookup order: loose files in the install directory, then opened nested
// archives (most recently opened first), then the top-level archives in
// priority order (expansion/mod archives before the base archives).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "vfs/mix.h"

namespace ra2yr::vfs {

class Vfs {
public:
    static std::optional<Vfs> open_install(const std::filesystem::path& directory,
                                           std::string* error = nullptr);

    // Read a file by name from the loose directory or any known archive.
    std::optional<std::vector<std::uint8_t>> read(std::string_view name) const;

    // Open a nested MIX by name (for example "isourb.mix") from a known archive
    // and register it for subsequent reads. Returns nullptr when it cannot be
    // found or parsed.
    const MixArchive* open_mix(std::string_view name);

    const MixArchive* find_mix(std::string_view name) const;

    const std::filesystem::path& install_dir() const { return install_dir_; }
    const std::vector<std::string>& top_archives() const { return top_names_; }

private:
    std::optional<std::vector<std::uint8_t>> read_loose(std::string_view name) const;

    std::filesystem::path install_dir_;
    std::vector<std::shared_ptr<MixArchive>> top_;
    std::vector<std::string> top_names_;
    std::vector<std::shared_ptr<MixArchive>> opened_;
    std::unordered_map<std::string, std::filesystem::path> loose_;
};

}  // namespace ra2yr::vfs
