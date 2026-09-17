#include "render/sprites.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include "core/log.h"
#include "formats/ini.h"
#include "formats/shp.h"
#include "render/iso.h"

namespace ra2yr::render {
namespace {

constexpr int kMaxAtlasWidth = 2048;
constexpr int kMaxAtlasHeight = 4096;

std::string lowercase(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
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

struct Sprite {
    std::vector<std::uint8_t> rgba;
    int w = 0;
    int h = 0;
    int slot_x = 0;
    int slot_y = 0;
};

struct Placement {
    int slot = -1;
    int x = 0;
    int y = 0;
};

}  // namespace

std::optional<ObjectAtlas> build_map_objects(const formats::MapFile& map, vfs::Vfs& vfs,
                                             const formats::Palette& palette,
                                             int tile_width, int tile_height,
                                             std::string* error) {
    // Bring the mixes that hold art definitions and building SHPs online.
    for (const char* mix : {"localmd.mix", "local.mix", "snow.mix", "isosnow.mix",
                            "urban.mix", "isourb.mix", "temperat.mix", "isotemp.mix",
                            "conquer.mix", "conqmd.mix", "generic.mix", "genermd.mix",
                            "cache.mix", "cachemd.mix"}) {
        vfs.open_mix(mix);
    }

    const auto read_asset = [&vfs](const std::string& name) -> std::vector<std::uint8_t> {
        if (auto bytes = vfs.read(name)) {
            return std::move(*bytes);
        }
        return {};
    };

    std::vector<std::uint8_t> art_bytes = read_asset("artmd.ini");
    if (art_bytes.empty()) {
        art_bytes = read_asset("art.ini");
    }
    if (art_bytes.empty()) {
        if (error != nullptr) {
            *error = "cannot find artmd.ini or art.ini";
        }
        return std::nullopt;
    }
    const formats::IniFile art =
        formats::IniFile::parse(std::string(art_bytes.begin(), art_bytes.end()));

    ObjectAtlas atlas;
    if (map.structures().empty()) {
        return atlas;
    }

    std::vector<Sprite> sprites;
    std::unordered_map<std::string, int> slot_for_image;
    std::vector<Placement> placements;

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
        if (const std::string* art_image = art.get(type, "image")) {
            if (!art_image->empty()) {
                image = *art_image;
            }
        }
        image = lowercase(image);

        auto cached = slot_for_image.find(image);
        if (cached == slot_for_image.end()) {
            int slot = -1;
            const std::vector<std::uint8_t> shp_bytes = read_asset(image + ".shp");
            if (!shp_bytes.empty()) {
                std::string parse_error;
                auto shp = formats::ShpFile::from_bytes(shp_bytes, &parse_error);
                if (shp && shp->frame_count() > 0) {
                    Sprite sprite;
                    sprite.rgba = shp->to_rgba(shp->frame(0), palette);
                    sprite.w = shp->width();
                    sprite.h = shp->height();
                    slot = static_cast<int>(sprites.size());
                    sprites.push_back(std::move(sprite));
                }
            }
            cached = slot_for_image.emplace(image, slot).first;
        }
        if (cached->second < 0) {
            ++atlas.missing;
            continue;
        }
        placements.push_back(Placement{cached->second, x, y});
    }

    if (sprites.empty()) {
        return atlas;
    }

    // Shelf-pack the sprites, tallest first, into one atlas.
    std::vector<int> order(sprites.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    std::stable_sort(order.begin(), order.end(), [&sprites](int a, int b) {
        return sprites[static_cast<std::size_t>(a)].h >
               sprites[static_cast<std::size_t>(b)].h;
    });
    int pen_x = 0;
    int pen_y = 0;
    int row_h = 0;
    for (int index : order) {
        Sprite& sprite = sprites[static_cast<std::size_t>(index)];
        if (pen_x + sprite.w > kMaxAtlasWidth) {
            pen_x = 0;
            pen_y += row_h;
            row_h = 0;
        }
        sprite.slot_x = pen_x;
        sprite.slot_y = pen_y;
        pen_x += sprite.w;
        row_h = std::max(row_h, sprite.h);
    }
    atlas.width = kMaxAtlasWidth;
    atlas.height = pen_y + row_h;
    if (atlas.height > kMaxAtlasHeight) {
        if (error != nullptr) {
            *error = "object atlas exceeds the maximum texture size";
        }
        return std::nullopt;
    }
    atlas.rgba.assign(static_cast<std::size_t>(atlas.width) * atlas.height * 4, 0);
    for (const Sprite& sprite : sprites) {
        for (int y = 0; y < sprite.h; ++y) {
            const std::uint8_t* src =
                sprite.rgba.data() + static_cast<std::size_t>(y) * sprite.w * 4;
            std::uint8_t* dst =
                atlas.rgba.data() +
                (static_cast<std::size_t>(sprite.slot_y + y) * atlas.width + sprite.slot_x) *
                    4;
            std::copy(src, src + static_cast<std::size_t>(sprite.w) * 4, dst);
        }
    }

    atlas.distinct_images = sprites.size();
    {
        int max_w = 0;
        int max_h = 0;
        for (const Sprite& sprite : sprites) {
            max_w = std::max(max_w, sprite.w);
            max_h = std::max(max_h, sprite.h);
        }
        log(LogLevel::Debug, "object atlas ", atlas.width, "x", atlas.height,
            ", ", sprites.size(), " images, largest ", max_w, "x", max_h);

    }
    atlas.instances.reserve(placements.size());
    for (const Placement& placement : placements) {
        const Sprite& sprite = sprites[static_cast<std::size_t>(placement.slot)];
        const IsoPoint p =
            cell_to_screen(placement.x, placement.y, tile_width, tile_height);
        TileInstance instance;
        instance.x = static_cast<float>(p.x) +
                     static_cast<float>(tile_width - sprite.w) * 0.5f;
        instance.y = static_cast<float>(p.y) +
                     static_cast<float>(tile_height - sprite.h);
        instance.width = static_cast<float>(sprite.w);
        instance.height = static_cast<float>(sprite.h);
        instance.u0 = static_cast<float>(sprite.slot_x) / atlas.width;
        instance.v0 = static_cast<float>(sprite.slot_y) / atlas.height;
        instance.u1 =
            static_cast<float>(sprite.slot_x + sprite.w) / atlas.width;
        instance.v1 =
            static_cast<float>(sprite.slot_y + sprite.h) / atlas.height;
        instance.depth = (placement.x + placement.y) * 16 + 1;
        atlas.instances.push_back(instance);
    }
    return atlas;
}

}  // namespace ra2yr::render
