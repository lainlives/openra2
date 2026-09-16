#include "formats/theater.h"

#include <cctype>
#include <cstdio>

namespace ra2yr::formats {

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
    std::string upper(theater_name);
    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (upper == "TEMPERATE") return "tem";
    if (upper == "SNOW") return "sno";
    if (upper == "URBAN") return "urb";
    if (upper == "DESERT") return "des";
    if (upper == "LUNAR") return "lun";
    if (upper == "NEWURBAN") return "ubn";
    return nullptr;
}

}  // namespace ra2yr::formats
