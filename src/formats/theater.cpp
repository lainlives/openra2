#include "formats/theater.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace ra2yr::formats {
namespace {

// Order and names match the YR engine's Theater array (see YRpp Theater.h:
// ID / ControlFileName / ArtFileName / PaletteFileName / Extension).
constexpr TheaterInfo kTheaters[] = {
    {"TEMPERATE", "temperat", "isotemp", "isotem", "tem"},
    {"SNOW", "snow", "isosnow", "isosno", "sno"},
    {"URBAN", "urban", "isourb", "isourb", "urb"},
    {"DESERT", "desert", "isodes", "isodes", "des"},
    {"LUNAR", "lunar", "isolun", "isolun", "lun"},
    {"NEWURBAN", "urbann", "isoubn", "isoubn", "ubn"},
};

}  // namespace

const TheaterInfo* theater_info(std::string_view theater_name) {
    for (const TheaterInfo& info : kTheaters) {
        if (theater_name.size() != std::strlen(info.id)) {
            continue;
        }
        bool match = true;
        for (std::size_t i = 0; i < theater_name.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(theater_name[i])) !=
                info.id[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return &info;
        }
    }
    return nullptr;
}

Theater Theater::from_ini(const IniFile& ini) {
    Theater theater;
    int base_id = 0;
    for (int set_id = 0;; ++set_id) {
        char section[32];
        std::snprintf(section, sizeof(section), "tileset%04d", set_id);
        const int tiles_in_set = ini.get_int(section, "tilesinset", -1);
        if (tiles_in_set < 0) {
            // Only a missing section ends the contiguous load. Yuri's Revenge
            // uses zero-tile sets for theater remapping.
            break;
        }
        const std::string* file = ini.get(section, "filename");
        TheaterTileSet set;
        set.base_id = base_id;
        set.count = tiles_in_set;
        set.file_name = (file != nullptr && !file->empty()) ? *file : "tile";
        theater.sets_.push_back(std::move(set));
        base_id += tiles_in_set;
    }
    return theater;
}

bool Theater::resolve(std::uint16_t tile_id, std::string* file_base,
                      int* index_in_set) const {
    for (const TheaterTileSet& set : sets_) {
        if (tile_id >= set.base_id && tile_id < set.base_id + set.count) {
            if (file_base != nullptr) {
                *file_base = set.file_name;
            }
            if (index_in_set != nullptr) {
                *index_in_set = tile_id - set.base_id;
            }
            return true;
        }
    }
    return false;
}

const char* Theater::tile_suffix(std::string_view theater_name) {
    const TheaterInfo* info = theater_info(theater_name);
    return info != nullptr ? info->extension : nullptr;
}

}  // namespace ra2yr::formats
