#include "render/scene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include "core/log.h"
#include "formats/ini.h"
#include "formats/shp.h"
#include "formats/theater.h"
#include "formats/tmp.h"
#include "render/iso.h"

namespace ra2yr::render {
namespace {

constexpr int kAtlasWidth = 4096;
constexpr int kMaxAtlasHeight = 8192;

// Depth buckets: diagonal * 64 + layer. Terrain uses its z level (0..15) as the
// layer, objects use 32 so they sit after their footprint's terrain.
constexpr int kObjectLayer = 32;
constexpr int kDepthStride = 64;

std::string lowercase(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::vector<std::string> candidate_tile_names(const std::string& base, int index,
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

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(separator, start);
        std::string part =
            text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front()))) {
            part.erase(part.begin());
        }
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back()))) {
            part.pop_back();
        }
        parts.push_back(std::move(part));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

bool parse_int(const std::string& text, int* out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || (end != nullptr && *end != '\0')) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

void parse_foundation(const std::string& text, int* width, int* height) {
    const std::size_t x = text.find_first_of("xX*");
    if (x == std::string::npos) {
        return;
    }
    int w = 0;
    int h = 0;
    if (parse_int(text.substr(0, x), &w) && parse_int(text.substr(x + 1), &h) && w > 0 &&
        h > 0) {
        *width = w;
        *height = h;
    }
}

struct Image {
    std::vector<std::uint8_t> rgba;
    int w = 0;
    int h = 0;
    int slot_x = 0;
    int slot_y = 0;
};

struct TilePlacement {
    int slot = -1;
    int x = 0;
    int y = 0;
    int z = 0;
};

struct SpritePlacement {
    int slot = -1;
    int x = 0;
    int y = 0;
    int foundation_w = 1;
    int foundation_h = 1;
    int draw_x = 0;
    int draw_y = 0;
};

void expand_bounds(const TileInstance& instance, bool* first, int* min_x, int* max_x,
                   int* min_y, int* max_y) {
    const int x = static_cast<int>(instance.x);
    const int y = static_cast<int>(instance.y);
    const int right = x + static_cast<int>(instance.width);
    const int bottom = y + static_cast<int>(instance.height);
    if (*first) {
        *min_x = x;
        *max_x = right;
        *min_y = y;
        *max_y = bottom;
        *first = false;
    } else {
        *min_x = std::min(*min_x, x);
        *max_x = std::max(*max_x, right);
        *min_y = std::min(*min_y, y);
        *max_y = std::max(*max_y, bottom);
    }
}

}  // namespace

Scene build_grid_scene(const std::vector<std::uint8_t>& tile_rgba, int tile_width,
                       int tile_height, int cols, int rows) {
    Scene scene;
    if (tile_width <= 0 || tile_height <= 0 || cols <= 0 || rows <= 0 ||
        tile_rgba.size() < static_cast<std::size_t>(tile_width) * tile_height * 4) {
        return scene;
    }
    scene.width = tile_width;
    scene.height = tile_height;
    scene.rgba = tile_rgba;

    const auto order = build_iso_order(cols, rows);
    bool first = true;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const IsoPoint p = cell_to_screen(order[i].cx, order[i].cy, tile_width, tile_height);
        TileInstance instance;
        instance.x = static_cast<float>(p.x);
        instance.y = static_cast<float>(p.y);
        instance.width = static_cast<float>(tile_width);
        instance.height = static_cast<float>(tile_height);
        instance.depth = static_cast<int>(i) * kDepthStride;
        scene.instances.push_back(instance);
        expand_bounds(instance, &first, &scene.min_x, &scene.max_x, &scene.min_y,
                      &scene.max_y);
    }
    scene.tile_count = scene.instances.size();
    return scene;
}

std::optional<Scene> build_map_scene(const formats::MapFile& map, vfs::Vfs& vfs,
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

    for (const char* mix : {"localmd.mix", "local.mix", "snow.mix", "isosnow.mix",
                            "snowmd.mix", "urban.mix", "isourb.mix", "urbann.mix",
                            "temperat.mix", "isotemp.mix", "desert.mix", "isodes.mix",
                            "lunar.mix", "isolun.mix", "conquer.mix", "conqmd.mix",
                            "generic.mix", "genermd.mix", "cache.mix", "cachemd.mix"}) {
        vfs.open_mix(mix);
    }
    vfs.open_mix(std::string(info->art) + ".mix");

    const auto read_asset = [&vfs](const std::string& name) -> std::vector<std::uint8_t> {
        if (auto bytes = vfs.read(name)) {
            return std::move(*bytes);
        }
        return {};
    };

    // Palette cache. Westwood names palettes <base>.pal; terrain, unit, city
    // and lib palettes append the theater suffix.
    std::unordered_map<std::string, std::optional<formats::Palette>> palette_cache;
    const auto get_palette = [&](const std::string& base) -> const formats::Palette* {
        const std::string name = lowercase(base) + ".pal";
        auto it = palette_cache.find(name);
        if (it == palette_cache.end()) {
            std::optional<formats::Palette> parsed;
            if (auto bytes = vfs.read(name)) {
                std::string palette_error;
                parsed = formats::Palette::from_bytes(*bytes, &palette_error);
            }
            it = palette_cache.emplace(name, std::move(parsed)).first;
        }
        return it->second ? &*it->second : nullptr;
    };
    const auto truthy = [](const std::string* value) {
        if (value == nullptr) {
            return false;
        }
        const std::string text = lowercase(*value);
        return text == "yes" || text == "true" || text == "1";
    };

    std::optional<formats::Palette> override_palette;
    if (!palette_override.empty()) {
        std::ifstream in(palette_override, std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        std::string palette_error;
        override_palette = formats::Palette::from_bytes(bytes, &palette_error);
    }
    const formats::Palette* terrain_palette =
        override_palette ? &*override_palette : get_palette(info->terrain);
    if (terrain_palette == nullptr) {
        if (error != nullptr) {
            *error = std::string("cannot find terrain palette ") + info->terrain + ".pal";
        }
        return std::nullopt;
    }
    const formats::Palette* building_palette = get_palette(info->palette);
    const formats::Palette* unit_palette =
        get_palette(std::string("unit") + info->extension);
    const formats::Palette* anim_palette = get_palette("anim");

    std::vector<Image> images;
    std::unordered_map<std::uint16_t, int> slot_for_tile;
    std::vector<TilePlacement> tile_placements;
    int tile_w = 0;
    int tile_h = 0;
    std::size_t missing_tiles = 0;

    // --- Terrain ---------------------------------------------------------
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
    const formats::Theater theater = formats::Theater::from_ini(
        formats::IniFile::parse(std::string(ini_bytes.begin(), ini_bytes.end())));

    for (const formats::MapCell& cell : map.cells()) {
        if (cell.tile == 0xFFFF) {
            continue;  // empty cell marker
        }
        auto cached = slot_for_tile.find(cell.tile);
        if (cached == slot_for_tile.end()) {
            int slot = -1;
            std::string base;
            int index = 0;
            if (theater.resolve(cell.tile, &base, &index)) {
                for (const std::string& name :
                     candidate_tile_names(base, index, info->extension)) {
                    const std::vector<std::uint8_t> bytes = read_asset(name);
                    if (bytes.empty()) {
                        continue;
                    }
                    std::string parse_error;
                    auto tmp = formats::TmpFile::from_bytes(bytes, &parse_error);
                    if (!tmp || tmp->tile_count() == 0) {
                        continue;
                    }
                    int chosen = -1;
                    if (cell.sub_tile < tmp->tile_count() &&
                        !tmp->tile(cell.sub_tile).image.empty()) {
                        chosen = cell.sub_tile;
                    } else {
                        for (std::size_t c = 0; c < tmp->tile_count(); ++c) {
                            if (!tmp->tile(c).image.empty()) {
                                chosen = static_cast<int>(c);
                                break;
                            }
                        }
                    }
                    if (chosen < 0) {
                        continue;
                    }
                    if (tile_w != 0 && (static_cast<int>(tmp->tile_width()) != tile_w ||
                                        static_cast<int>(tmp->tile_height()) != tile_h)) {
                        continue;
                    }
                    if (tile_w == 0) {
                        tile_w = static_cast<int>(tmp->tile_width());
                        tile_h = static_cast<int>(tmp->tile_height());
                    }
                    Image image;
                    image.rgba = tmp->to_rgba(tmp->tile(static_cast<std::size_t>(chosen)),
                                              *terrain_palette);
                    image.w = tile_w;
                    image.h = tile_h;
                    if (image.rgba.size() <
                        static_cast<std::size_t>(tile_w) * tile_h * 4) {
                        continue;
                    }
                    slot = static_cast<int>(images.size());
                    images.push_back(std::move(image));
                    break;
                }
            }
            cached = slot_for_tile.emplace(cell.tile, slot).first;
        }
        if (cached->second < 0) {
            ++missing_tiles;
            continue;
        }
        tile_placements.push_back(
            TilePlacement{cached->second, cell.x, cell.y, cell.z});
    }

    if (tile_w == 0 || tile_placements.empty()) {
        if (error != nullptr) {
            *error = "no theater tiles could be resolved for " + theater_name;
        }
        return std::nullopt;
    }

    // --- Objects ---------------------------------------------------------
    std::vector<std::uint8_t> art_bytes = read_asset("artmd.ini");
    if (art_bytes.empty()) {
        art_bytes = read_asset("art.ini");
    }
    std::vector<std::uint8_t> rules_bytes = read_asset("rulesmd.ini");
    if (rules_bytes.empty()) {
        rules_bytes = read_asset("rules.ini");
    }
    const formats::IniFile art = formats::IniFile::parse(
        std::string(art_bytes.begin(), art_bytes.end()));
    const formats::IniFile rules = formats::IniFile::parse(
        std::string(rules_bytes.begin(), rules_bytes.end()));

    std::unordered_map<std::string, int> slot_for_image;
    std::vector<SpritePlacement> sprite_placements;
    std::size_t missing_sprites = 0;

    if (!art_bytes.empty() || !rules_bytes.empty()) {
        for (const formats::MapFile::Entry& entry : map.structures()) {
            const std::vector<std::string> fields = split(entry.second, ',');
            if (fields.size() < 5) {
                continue;
            }
            const std::string& type = fields[1];
            int x = 0;
            int y = 0;
            if (!parse_int(fields[3], &x) || !parse_int(fields[4], &y)) {
                continue;
            }
            std::string image = type;
            const std::string* declared = art.get(type, "image");
            if (declared == nullptr) {
                declared = rules.get(type, "image");
            }
            if (declared != nullptr && !declared->empty()) {
                image = *declared;
            }
            image = lowercase(image);

            // Palette: TerrainPalette > AltPalette > AnimPalette > Palette= >
            // the building/iso palette.
            std::string palette_name = info->palette;
            if (truthy(art.get(type, "terrainpalette")) ||
                truthy(rules.get(type, "terrainpalette"))) {
                palette_name = info->terrain;
            } else if (truthy(art.get(type, "altpalette")) ||
                       truthy(rules.get(type, "altpalette"))) {
                palette_name = std::string("unit") + info->extension;
            } else if (truthy(art.get(type, "animpalette")) ||
                       truthy(rules.get(type, "animpalette"))) {
                palette_name = "anim";
            } else if (const std::string* named = art.get(type, "palette")) {
                palette_name = lowercase(*named) + info->extension;
            }
            const formats::Palette* palette = get_palette(palette_name);
            if (palette == nullptr) {
                palette = building_palette != nullptr ? building_palette : terrain_palette;
            }

            const std::string* foundation = art.get(type, "foundation");
            if (foundation == nullptr) {
                foundation = rules.get(type, "foundation");
            }
            int foundation_w = 1;
            int foundation_h = 1;
            if (foundation != nullptr) {
                parse_foundation(*foundation, &foundation_w, &foundation_h);
            }

            int draw_x = 0;
            int draw_y = 0;
            const std::string* xdraw = art.get(type, "xdrawoffset");
            const std::string* ydraw = art.get(type, "ydrawoffset");
            if (xdraw != nullptr) {
                parse_int(*xdraw, &draw_x);
            }
            if (ydraw != nullptr) {
                parse_int(*ydraw, &draw_y);
            }

            const std::string cache_key = image + "|" + palette_name;
            auto cached = slot_for_image.find(cache_key);
            if (cached == slot_for_image.end()) {
                int slot = -1;
                const std::vector<std::uint8_t> shp_bytes = read_asset(image + ".shp");
                if (!shp_bytes.empty()) {
                    std::string parse_error;
                    auto shp = formats::ShpFile::from_bytes(shp_bytes, &parse_error);
                    if (shp && shp->frame_count() > 0) {
                        Image sprite;
                        sprite.rgba = shp->to_rgba(shp->frame(0), *palette);
                        sprite.w = shp->width();
                        sprite.h = shp->height();
                        slot = static_cast<int>(images.size());
                        images.push_back(std::move(sprite));
                    }
                }
                cached = slot_for_image.emplace(cache_key, slot).first;
            }
            if (cached->second < 0) {
                ++missing_sprites;
                continue;
            }
            sprite_placements.push_back(
                SpritePlacement{cached->second, x, y, foundation_w, foundation_h,
                                draw_x, draw_y});
        }
    }

    // --- Atlas packing ---------------------------------------------------
    std::vector<int> order(images.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    std::stable_sort(order.begin(), order.end(), [&images](int a, int b) {
        return images[static_cast<std::size_t>(a)].h > images[static_cast<std::size_t>(b)].h;
    });
    int pen_x = 0;
    int pen_y = 0;
    int row_h = 0;
    int max_row_width = 0;
    for (int index : order) {
        Image& image = images[static_cast<std::size_t>(index)];
        if (pen_x + image.w > kAtlasWidth) {
            max_row_width = std::max(max_row_width, pen_x);
            pen_x = 0;
            pen_y += row_h;
            row_h = 0;
        }
        image.slot_x = pen_x;
        image.slot_y = pen_y;
        pen_x += image.w;
        row_h = std::max(row_h, image.h);
    }
    max_row_width = std::max(max_row_width, pen_x);
    const int atlas_h = pen_y + row_h;
    if (max_row_width <= 0 || atlas_h > kMaxAtlasHeight) {
        if (error != nullptr) {
            *error = "scene atlas exceeds the maximum texture size";
        }
        return std::nullopt;
    }

    Scene scene;
    scene.width = max_row_width;
    scene.height = atlas_h;
    scene.rgba.assign(static_cast<std::size_t>(scene.width) * scene.height * 4, 0);
    for (const Image& image : images) {
        for (int y = 0; y < image.h; ++y) {
            const std::uint8_t* src =
                image.rgba.data() + static_cast<std::size_t>(y) * image.w * 4;
            std::uint8_t* dst =
                scene.rgba.data() +
                (static_cast<std::size_t>(image.slot_y + y) * scene.width + image.slot_x) *
                    4;
            std::copy(src, src + static_cast<std::size_t>(image.w) * 4, dst);
        }
    }

    const auto make_uv = [&scene](const Image& image, TileInstance* instance) {
        instance->u0 = static_cast<float>(image.slot_x) / scene.width;
        instance->v0 = static_cast<float>(image.slot_y) / scene.height;
        instance->u1 = static_cast<float>(image.slot_x + image.w) / scene.width;
        instance->v1 = static_cast<float>(image.slot_y + image.h) / scene.height;
    };

    bool first = true;
    for (const TilePlacement& placement : tile_placements) {
        const Image& image = images[static_cast<std::size_t>(placement.slot)];
        const IsoPoint p = cell_to_screen(placement.x, placement.y, tile_w, tile_h);
        TileInstance instance;
        instance.x = static_cast<float>(p.x);
        instance.y = static_cast<float>(p.y) -
                     static_cast<float>(placement.z) * static_cast<float>(tile_h / 2);
        instance.width = static_cast<float>(image.w);
        instance.height = static_cast<float>(image.h);
        make_uv(image, &instance);
        instance.depth = (placement.x + placement.y) * kDepthStride + placement.z;
        scene.instances.push_back(instance);
        expand_bounds(instance, &first, &scene.min_x, &scene.max_x, &scene.min_y,
                      &scene.max_y);
    }
    for (const SpritePlacement& placement : sprite_placements) {
        const Image& image = images[static_cast<std::size_t>(placement.slot)];
        // Placement verified against CNCMaps (which renders retail-identical):
        //   screen = tile_iso_pixel - full_frame/2 + frame.X/Y + XDrawOffset/YDrawOffset
        // The frame X/Y offset is already baked into our full-frame image, so we
        // place the full frame at p - full/2 plus the type's draw offset. The map
        // cell is the top-left anchor; foundation only sets the depth footprint.
        const IsoPoint p = cell_to_screen(placement.x, placement.y, tile_w, tile_h);
        TileInstance instance;
        instance.x = static_cast<float>(p.x) - static_cast<float>(image.w) * 0.5f +
                     static_cast<float>(placement.draw_x);
        instance.y = static_cast<float>(p.y) - static_cast<float>(image.h) * 0.5f +
                     static_cast<float>(placement.draw_y);
        instance.width = static_cast<float>(image.w);
        instance.height = static_cast<float>(image.h);
        make_uv(image, &instance);
        instance.depth = (placement.x + placement.y) * kDepthStride + kObjectLayer;
        scene.instances.push_back(instance);
        expand_bounds(instance, &first, &scene.min_x, &scene.max_x, &scene.min_y,
                      &scene.max_y);
    }

    scene.tile_count = tile_placements.size();
    scene.sprite_count = sprite_placements.size();
    scene.missing_tiles = missing_tiles;
    scene.missing_sprites = missing_sprites;
    return scene;
}

}  // namespace ra2yr::render