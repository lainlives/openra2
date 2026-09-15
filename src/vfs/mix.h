// Minimal Westwood MIX archive reader.
//
// Reads old-format and new-format headers, including Blowfish-encrypted
// indexes, and resolves names from the archive's embedded "local mix
// database.dat". No pre-extracted key or key file is required.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ra2yr::vfs {

// Westwood filename hash used for MIX entry IDs.
std::uint32_t mix_filename_hash(std::string_view name);

// Optional external filename database. Retail archives do not embed names, so
// the engine (or a tool) supplies the names it knows; anything unresolved stays
// as "_<ID>".
class NameDatabase {
public:
    void add(std::string_view name);
    bool load(const std::filesystem::path& path, std::string* error = nullptr);
    const std::string* lookup(std::uint32_t id) const;
    std::size_t size() const { return by_id_.size(); }

private:
    std::unordered_map<std::uint32_t, std::string> by_id_;
};

struct MixEntry {
    std::uint32_t id = 0;
    std::uint32_t offset = 0;  // from the start of the body
    std::uint32_t size = 0;
    std::string name;
};

class MixArchive {
public:
    MixArchive() = default;

    static std::optional<MixArchive> open(const std::filesystem::path& path,
                                          std::string* error = nullptr,
                                          const NameDatabase* names = nullptr);
    static std::optional<MixArchive> open_memory(std::vector<std::uint8_t> data,
                                                 std::string name = "<memory>",
                                                 std::string* error = nullptr,
                                                 const NameDatabase* names = nullptr);

    const std::string& name() const { return name_; }
    bool encrypted() const { return encrypted_; }
    bool checksummed() const { return checksummed_; }
    std::uint32_t body_size() const { return body_size_; }
    const std::vector<MixEntry>& entries() const { return entries_; }

    const MixEntry* find(std::string_view name) const;
    const MixEntry* find(std::uint32_t id) const;

    // Read the whole contents of an entry.
    std::vector<std::uint8_t> read(const MixEntry& entry) const;

private:
    bool parse(std::string* error);
    bool read_at(std::uint64_t offset, std::uint32_t size, std::uint8_t* out) const;
    std::vector<std::uint8_t> read_at_vec(std::uint64_t offset, std::uint32_t size) const;

    std::string name_;
    std::filesystem::path path_;
    std::vector<std::uint8_t> memory_;
    bool in_memory_ = false;
    mutable std::ifstream stream_;
    const NameDatabase* names_ = nullptr;

    bool encrypted_ = false;
    bool checksummed_ = false;
    std::uint32_t flags_ = 0;
    std::uint32_t body_size_ = 0;
    std::uint64_t body_offset_ = 0;
    std::vector<MixEntry> entries_;
};

}  // namespace ra2yr::vfs