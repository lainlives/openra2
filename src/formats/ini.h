// Minimal INI reader for map and theater files.
//
// Within a section, entries keep their file order so numbered keys (the
// IsoMapPack5 lines) can be reassembled. Section and key lookups are
// case-insensitive.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ra2yr::formats {

class IniFile {
public:
    using Entry = std::pair<std::string, std::string>;
    using Entries = std::vector<Entry>;

    static IniFile parse(std::string_view text);

    const Entries* section(std::string_view name) const;
    const std::string* get(std::string_view section, std::string_view key) const;
    int get_int(std::string_view section, std::string_view key, int fallback) const;

private:
    std::unordered_map<std::string, Entries> sections_;
};

}  // namespace ra2yr::formats
