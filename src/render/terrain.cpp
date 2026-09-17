#include "render/terrain.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include "formats/ini.h"
#include "formats/tmp.h"
#include "render/iso.h"

namespace ra2yr::render {
namespace {

constexpr int kMaxAtlasColumns = 16;
constexpr int kMaxTextureDimension = 4096;

std::string lowercase(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

// Candidate TMP names for a tile: `baseNN.ext`, then the damaged variants
// `baseNNa.ext` .. `baseNNg.ext`, all lowercase.
std::vector<std::string> candidate_names(const std::string& base, int index,
                                         const char* suffix) {
    char stem[256];
    std::snprintf(stem, sizeof(stem), "%s%02d", base.c_str(), index + 1);
    std::vector<std::string> names;
    names.push_back(lowercase(std::string(stem) + "." + suffix));
    for (char letter = 'a'; letter <= 'g'; ++letter) {
        names.push_back(lowercase(std::string(stem) + std::string(1, letter) + "." + suffix));
    }
    return names;
}

}  // namespace

TerrainAtlas build_grid_terrain(const std::vector<std::uint8_t>& tile_rgba, int tile_width,
                                int tile_height, int cols, int rows) {
    TerrainAtlas atlas;
    if (tile_width <= 0 || tile_height <= 0 || cols <= 0 || rows <= 0 ||
        tile_rgba.size() < static_cast<std::size_t>(tile_width) * tile_height * 4) {
        return atlas;
    }
    atlas.atlas_width = tile_width;
    atlas.atlas_height = tile_height;
    atlas.tile_width = tile_width;
    atlas.tile_height = tile_height;
    atlas.rgba = tile_rgba;

    const auto order = build_iso_order(cols, rows);
    atlas.tiles.reserve(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        const IsoPoint p = cell_to_screen(order[i].cx, order[i].cy, tile_width, tile_height);
        TileInstance instance;
        instance.x = static_cast<float>(p.x);
        instance.y = static_cast<float>(p.y);
        instance.depth = static_cast<int>(i);
        atlas.tiles.push_back(instance);
    }
    atlas.min_x = -(rows - 1) * (tile_width / 2);
    atlas.max_x = (cols - 1) * (tile_width / 2) + tile_width;
    atlas.min_y = 0;
    atlas.max_y = (cols - 1 + rows - 1) * (tile_height / 2) + tile_height;
    return atlas;
}

std::optional<TerrainAtlas> build_map_terrain(const formats::MapFile& map,
                                              vfs::Vfs& vfs,
                                              const std::string& palette_override,
                                              const std::string& theater_override,
                                              std::string* error) {
    const std::string theater_name =
        theater_override.empty() ? map.theater() : theater_override;
    const formats::TheaterInfo* info = formats::theater_info(theater_name);
    if (info == nullptr) {
        if (error != nullptr) {
            *error = "unknown theater: " + theater_name;
        }
        return std::nullopt;
    }

    // Bring the theater tile mix, palettes, and localization mixes online, as
    // the engine does during startup.
    vfs.open_mix(std::string(info->art) + ".mix");
    vfs.open_mix("localmd.mix");
    vfs.open_mix("local.mix");
    vfs.open_mix("cachemd.mix");
    vfs.open_mix("cache.mix");

    const auto read_asset = [&vfs](const std::string& name) -> std::vector<std::uint8_t> {
        if (auto bytes = vfs.read(name)) {
            return std::move(*bytes);
        }
        return {};
    };

    // Control INI: Yuri's Revenge uses the MD variant, Red Alert 2 the base.
    const std::string control(info->control);
    std::vector<std::uint8_t> ini_bytes = read_asset(lowercase(control) + "md.ini");
    if (ini_bytes.empty()) {
        ini_bytes = read_asset(lowercase(control) + ".ini");
    }
    if (ini_bytes.empty()) {
        if (error != nullptr) {
            *error = "cannot find theater control file " + control + "md.ini";
        }
        return std::nullopt;
    }
    const std::string ini_text(ini_bytes.begin(), ini_bytes.end());
    const formats::Theater theater =
        formats::Theater::from_ini(formats::IniFile::parse(ini_text));

    formats::Palette palette;
    bool have_palette = false;
    if (!palette_override.empty()) {
        const std::vector<std::uint8_t> bytes = read_file(palette_override);
        std::string palette_error;
        if (auto parsed = formats::Palette::from_bytes(bytes, &palette_error)) {
            palette = *parsed;
            have_palette = true;
        } else if (error != nullptr) {
            *error = "palette override: " + palette_error;
        }
    }
    if (!have_palette) {
        const std::vector<std::uint8_t> bytes =
            read_asset(lowercase(std::string(info->palette)) + ".pal");
        std::string palette_error;
        if (auto parsed = formats::Palette::from_bytes(bytes, &palette_error)) {
            palette = *parsed;
            have_palette = true;
        } else if (error != nullptr) {
            *error = "cannot find theater palette " + std::string(info->palette) + ".pal";
        }
    }
    if (!have_palette) {
        return std::nullopt;
    }

    TerrainAtlas atlas;
    std::unordered_map<std::uint16_t, int> slot_for_tile;
    struct Slot {
        std::vector<std::uint8_t> rgba;
    };
    std::vector<Slot> slots;
    int tile_w = 0;
    int tile_h = 0;
    int missing = 0;

    for (const formats::MapCell& cell : map.cells()) {
        if (slot_for_tile.count(cell.tile) != 0) {
            continue;
        }
        std::string base;
        int index = 0;
        if (!theater.resolve(cell.tile, &base, &index)) {
            continue;
        }
        formats::TmpFile tmp;
        bool loaded = false;
        int sub_tile = 0;
        for (const std::string& name : candidate_names(base, index, info->extension)) {
            const std::vector<std::uint8_t> bytes = read_asset(name);
            if (bytes.empty()) {
                continue;
            }
            std::string parse_error;
            auto parsed = formats::TmpFile::from_bytes(bytes, &parse_error);
            if (!parsed || parsed->tile_count() == 0) {
                continue;
            }
            // Some multi-cell TMPs leave cells empty; pick a cell that has an
            // image, preferring the map's sub-tile.
            int chosen = -1;
            if (cell.sub_tile < parsed->tile_count() &&
                !parsed->tile(cell.sub_tile).image.empty()) {
                chosen = cell.sub_tile;
            } else {
                for (std::size_t c = 0; c < parsed->tile_count(); ++c) {
                    if (!parsed->tile(c).image.empty()) {
                        chosen = static_cast<int>(c);
                        break;
                    }
                }
            }
            if (chosen < 0) {
                continue;
            }
            if (tile_w != 0 &&
                (static_cast<int>(parsed->tile_width()) != tile_w ||
                 static_cast<int>(parsed->tile_height()) != tile_h)) {
                continue;
            }
            tmp = std::move(*parsed);
            sub_tile = chosen;
            loaded = true;
            break;
        }
        if (!loaded) {
            ++missing;
            slot_for_tile[cell.tile] = -1;
            continue;
        }
        if (tile_w == 0) {
            tile_w = static_cast<int>(tmp.tile_width());
            tile_h = static_cast<int>(tmp.tile_height());
        }
        Slot slot;
        slot.rgba = tmp.to_rgba(tmp.tile(static_cast<std::size_t>(sub_tile)), palette);
        if (slot.rgba.size() <
            static_cast<std::size_t>(tile_w) * tile_h * 4) {
            ++missing;
            slot_for_tile[cell.tile] = -1;
            continue;
        }
        slot_for_tile[cell.tile] = static_cast<int>(slots.size());
        slots.push_back(std::move(slot));
    }

    if (slots.empty() || tile_w == 0 || tile_h == 0) {
        if (error != nullptr) {
            *error = "no theater tiles could be resolved for " + map.theater();
        }
        return std::nullopt;
    }

    const int columns = std::min(kMaxAtlasColumns, static_cast<int>(slots.size()));
    const int rows = (static_cast<int>(slots.size()) + columns - 1) / columns;
    atlas.atlas_width = columns * tile_w;
    atlas.atlas_height = rows * tile_h;
    if (atlas.atlas_width > kMaxTextureDimension ||
        atlas.atlas_height > kMaxTextureDimension) {
        if (error != nullptr) {
            *error = "tile atlas exceeds the maximum texture size";
        }
        return std::nullopt;
    }
    atlas.tile_width = tile_w;
    atlas.tile_height = tile_h;
    atlas.rgba.assign(
        static_cast<std::size_t>(atlas.atlas_width) * atlas.atlas_height * 4, 0);

    for (std::size_t i = 0; i < slots.size(); ++i) {
        const int slot_x = (static_cast<int>(i) % columns) * tile_w;
        const int slot_y = (static_cast<int>(i) / columns) * tile_h;
        for (int y = 0; y < tile_h; ++y) {
            const std::uint8_t* src =
                slots[i].rgba.data() + static_cast<std::size_t>(y) * tile_w * 4;
            std::uint8_t* dst = atlas.rgba.data() +
                                (static_cast<std::size_t>(slot_y + y) * atlas.atlas_width +
                                 slot_x) *
                                    4;
            std::copy(src, src + static_cast<std::size_t>(tile_w) * 4, dst);
        }
    }

    atlas.tiles.reserve(map.cells().size());
    bool first = true;
    for (const formats::MapCell& cell : map.cells()) {
        const auto it = slot_for_tile.find(cell.tile);
        if (it == slot_for_tile.end() || it->second < 0) {
            continue;
        }
        const int slot = it->second;
        const int slot_x = (slot % columns) * tile_w;
        const int slot_y = (slot / columns) * tile_h;

        const IsoPoint p = cell_to_screen(cell.x, cell.y, tile_w, tile_h);
        TileInstance instance;
        instance.x = static_cast<float>(p.x);
        instance.y = static_cast<float>(p.y) -
                     static_cast<float>(cell.z) * static_cast<float>(tile_h / 2);
        instance.u0 = static_cast<float>(slot_x) / atlas.atlas_width;
        instance.v0 = static_cast<float>(slot_y) / atlas.atlas_height;
        instance.u1 = static_cast<float>(slot_x + tile_w) / atlas.atlas_width;
        instance.v1 = static_cast<float>(slot_y + tile_h) / atlas.atlas_height;
        instance.depth = (cell.x + cell.y) * 16 + cell.z;
        atlas.tiles.push_back(instance);

        const int x = static_cast<int>(instance.x);
        const int y = static_cast<int>(instance.y);
        if (first) {
            atlas.min_x = x;
            atlas.max_x = x + tile_w;
            atlas.min_y = y;
            atlas.max_y = y + tile_h;
            first = false;
        } else {
            atlas.min_x = std::min(atlas.min_x, x);
            atlas.max_x = std::max(atlas.max_x, x + tile_w);
            atlas.min_y = std::min(atlas.min_y, y);
            atlas.max_y = std::max(atlas.max_y, y + tile_h);
        }
    }

    if (missing > 0) {
        std::fprintf(stderr, "[warn] %d tiles could not be resolved\n", missing);
    }
    return atlas;
}

}  // namespace ra2yr::render
