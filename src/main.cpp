#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/log.h"
#include "core/version.h"
#include "formats/ini.h"
#include "formats/map.h"
#include "formats/palette.h"
#include "formats/shp.h"
#include "formats/theater.h"
#include "formats/tmp.h"
#include "platform/platform.h"
#include "render/iso.h"
#include "render/scene.h"
#include "vfs/mix.h"
#include "vfs/vfs.h"

#if defined(RA2YR_RENDER_BGFX)
#include "render/bgfx_renderer.h"
#endif

namespace {

void print_usage() {
    std::fprintf(stderr,
                 "ra2yr %s\n"
                 "\n"
                 "Usage: ra2yr [options]\n"
                 "  --version             print the build identity and exit\n"
                 "  --headless            initialize and exit without a frame loop\n"
                 "  --verbose             enable debug logging (default in debug builds)\n"
                 "  --log-level LEVEL     trace|debug|info|warn|error\n"
                 "  --names FILE          filename database for the MIX operations\n"
                 "  --mix-list FILE       list the direct entries of a MIX archive\n"
                 "  --mix-tree FILE       list the archive tree, recursing into .mix\n"
                 "  --mix-extract FILE DIR  recursively extract every file\n"
                 "  --terrain TMP PAL     render an isometric grid of a TMP tile\n"
                 "  --grid COLSxROWS      grid size for --terrain (default 16x16)\n"
                 "  --shp-dump FILE PAL OUT  write all SHP frames as a PPM sheet\n"
                 "  --map FILE            render a .map/.mpr/.yrm map\n"
                 "  --install DIR         retail install directory for asset lookup\n"
                 "  --theater NAME        force the theater used for --map assets\n"
                 "  --fit                 fit the whole map in the window\n"
                 "  --palette FILE        optional palette override for --map\n"
                 "  --screenshot FILE.ppm capture a frame and exit\n"
                 "  --help                show this message\n",
                 ra2yr::kVersionString);
}

void print_archive_flags(const ra2yr::vfs::MixArchive& archive, std::string* out) {
    if (archive.checksummed()) *out += " checksummed";
    if (archive.encrypted()) *out += " encrypted";
}

bool ends_with_ci(std::string_view text, std::string_view suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    const std::size_t base = text.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[base + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

bool contains_ci(const std::uint8_t* data, std::size_t size, std::string_view needle) {
    if (needle.empty() || size < needle.size()) {
        return false;
    }
    for (std::size_t i = 0; i + needle.size() <= size; ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (std::tolower(data[i + j]) !=
                std::tolower(static_cast<unsigned char>(needle[j]))) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

int list_mix(const std::string& path, const ra2yr::vfs::NameDatabase* names) {
    std::string error;
    auto archive = ra2yr::vfs::MixArchive::open(path, &error, names);
    if (!archive) {
        ra2yr::log_error(error);
        return 1;
    }
    std::string flags;
    print_archive_flags(*archive, &flags);
    std::printf("%s: %zu entries, %u body bytes,%s\n", archive->name().c_str(),
                archive->entries().size(), archive->body_size(),
                flags.empty() ? "" : flags.c_str());
    for (const auto& entry : archive->entries()) {
        std::printf("  %08X %10u  %s\n", entry.id, entry.size, entry.name.c_str());
    }
    return 0;
}

int tree_mix(const std::string& path, const ra2yr::vfs::NameDatabase* names) {
    std::string error;
    int archives = 0;
    ra2yr::vfs::for_each_archive(
        path,
        [&archives](const ra2yr::vfs::MixArchive& archive, int depth) {
            std::string flags;
            print_archive_flags(archive, &flags);
            std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
            std::printf("%s%s: %zu entries, %u body bytes,%s\n", indent.c_str(),
                        archive.name().c_str(), archive.entries().size(),
                        archive.body_size(), flags.empty() ? "" : flags.c_str());
            ++archives;
        },
        names, 8, &error);
    if (archives == 0) {
        ra2yr::log_error(error.empty() ? "no archives found" : error);
        return 1;
    }
    return 0;
}

int extract_mix(const std::string& path, const std::string& dir,
                const ra2yr::vfs::NameDatabase* names) {
    std::string error;
    const auto leaves = ra2yr::vfs::enumerate_leaves(path, names, 8, &error);
    if (leaves.empty()) {
        ra2yr::log_error(error.empty() ? "no files found" : error);
        return 1;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::unordered_set<std::string> used;
    std::size_t count = 0;
    for (const auto& leaf : leaves) {
        std::string name = leaf.name;
        std::string key = name;
        for (char& c : key) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (!used.insert(key).second) {
            const std::size_t dot = name.rfind('.');
            char suffix[16];
            std::snprintf(suffix, sizeof(suffix), "_%08X", leaf.id);
            if (dot == std::string::npos) {
                name += suffix;
            } else {
                name.insert(dot, suffix);
            }
        }
        const std::filesystem::path dest = std::filesystem::path(dir) / name;
        const std::vector<std::uint8_t> bytes = leaf.bytes();
        if (FILE* f = std::fopen(dest.string().c_str(), "wb")) {
            std::fwrite(bytes.data(), 1, bytes.size(), f);
            std::fclose(f);
            ++count;
        } else {
            ra2yr::log_error("cannot write ", dest.string());
        }
    }
    std::printf("extracted %zu files to %s\n", count, dir.c_str());
    return 0;
}

#if defined(RA2YR_PLATFORM_SDL) && defined(RA2YR_RENDER_BGFX)
int run_viewer(const ra2yr::render::Scene& scene, const std::string& title,
               const std::string& screenshot, bool fit) {
    auto platform = ra2yr::platform::make_sdl_platform();
    if (!platform) {
        ra2yr::log_error("SDL3 unavailable");
        return 1;
    }
    ra2yr::platform::WindowDesc desc;
    desc.width = 1280;
    desc.height = 800;
    desc.title = title;
    desc.visible = true;
    auto window = platform->create_window(desc);
    if (!window) {
        ra2yr::log_error("failed to create a window");
        return 1;
    }

    std::string error;
    ra2yr::render::BgfxRenderer renderer;
    if (!renderer.initialize(window->native_window(), window->width(), window->height(),
                             &error)) {
        ra2yr::log_error(error);
        return 1;
    }
    if (!renderer.set_atlas(scene.rgba, scene.width, scene.height, &error)) {
        ra2yr::log_error(error);
        renderer.shutdown();
        return 1;
    }
    renderer.set_tiles(scene.instances);

    float camera_x = static_cast<float>(scene.min_x + scene.max_x) * 0.5f;
    float camera_y = static_cast<float>(scene.min_y + scene.max_y) * 0.5f;
    float zoom = 1.0f;
    if (fit && scene.max_x > scene.min_x && scene.max_y > scene.min_y) {
        const float world_w = static_cast<float>(scene.max_x - scene.min_x);
        const float world_h = static_cast<float>(scene.max_y - scene.min_y);
        zoom = std::min(static_cast<float>(window->width()) / world_w,
                        static_cast<float>(window->height()) / world_h) *
               0.95f;
        zoom = std::clamp(zoom, 0.05f, 4.0f);
    }
    ra2yr::log_info(title, ": ", scene.instances.size(), " instances, atlas ", scene.width,
                    "x", scene.height, ", renderer ", renderer.backend());
    ra2yr::log_debug("view: world x[", scene.min_x, "..", scene.max_x, "] y[",
                     scene.min_y, "..", scene.max_y, "] zoom ", zoom);

    int frames = 0;
    const bool want_shot = !screenshot.empty();
    bool dragging = false;
    float last_mouse_x = 0.0f;
    float last_mouse_y = 0.0f;
    while (!window->should_close()) {
        window->poll();
        const ra2yr::platform::InputState& input = window->input();
        if (input.escape) {
            break;
        }
        if (input.left) {
            if (!dragging) {
                dragging = true;
                last_mouse_x = input.mouse_x;
                last_mouse_y = input.mouse_y;
            }
            camera_x -= (input.mouse_x - last_mouse_x) / zoom;
            camera_y -= (input.mouse_y - last_mouse_y) / zoom;
            last_mouse_x = input.mouse_x;
            last_mouse_y = input.mouse_y;
        } else {
            dragging = false;
        }
        if (input.wheel != 0.0f) {
            zoom *= std::pow(1.1f, input.wheel);
            zoom = std::clamp(zoom, 0.25f, 4.0f);
        }
        renderer.set_camera(camera_x, camera_y, zoom);

        if (want_shot && frames == 3) {
            renderer.request_screenshot(screenshot);
        }
        renderer.render();
        ++frames;
        if (want_shot && renderer.screenshot_ready()) {
            break;
        }
        if (want_shot && frames > 240) {
            ra2yr::log_warn("screenshot timed out");
            break;
        }
    }
    renderer.shutdown();
    return 0;
}
#endif

int dump_shp(const std::string& shp_path, const std::string& pal_path,
             const std::string& out_path) {
    std::ifstream shp_in(shp_path, std::ios::binary);
    std::ifstream pal_in(pal_path, std::ios::binary);
    if (!shp_in || !pal_in) {
        ra2yr::log_error("cannot read ", shp_path, " or ", pal_path);
        return 1;
    }
    std::vector<std::uint8_t> shp_bytes((std::istreambuf_iterator<char>(shp_in)), {});
    std::vector<std::uint8_t> pal_bytes((std::istreambuf_iterator<char>(pal_in)), {});

    std::string error;
    auto shp = ra2yr::formats::ShpFile::from_bytes(shp_bytes, &error);
    if (!shp) {
        ra2yr::log_error("SHP: ", error);
        return 1;
    }
    auto palette = ra2yr::formats::Palette::from_bytes(pal_bytes, &error);
    if (!palette) {
        ra2yr::log_error("palette: ", error);
        return 1;
    }

    const int frame_w = shp->width();
    const int frame_h = shp->height();
    const int per_row = 8;
    const int rows = static_cast<int>((shp->frame_count() + per_row - 1) / per_row);
    const int sheet_w = frame_w * per_row;
    const int sheet_h = frame_h * rows;
    std::vector<std::uint8_t> sheet(
        static_cast<std::size_t>(sheet_w) * sheet_h * 3, 0);
    for (std::size_t i = 0; i < shp->frame_count(); ++i) {
        const auto rgba = shp->to_rgba(shp->frame(i), *palette);
        const int ox = static_cast<int>(i % per_row) * frame_w;
        const int oy = static_cast<int>(i / per_row) * frame_h;
        for (int y = 0; y < frame_h; ++y) {
            for (int x = 0; x < frame_w; ++x) {
                const std::uint8_t* src =
                    rgba.data() + (static_cast<std::size_t>(y) * frame_w + x) * 4;
                std::uint8_t* dst = sheet.data() +
                                    (static_cast<std::size_t>(oy + y) * sheet_w + ox + x) * 3;
                if (src[3] != 0) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                } else {
                    dst[0] = 255;
                    dst[1] = 0;
                    dst[2] = 255;
                }
            }
        }
    }
    FILE* out = std::fopen(out_path.c_str(), "wb");
    if (out == nullptr) {
        ra2yr::log_error("cannot write ", out_path);
        return 1;
    }
    std::fprintf(out, "P6\n%d %d\n255\n", sheet_w, sheet_h);
    std::fwrite(sheet.data(), 1, sheet.size(), out);
    std::fclose(out);
    ra2yr::log_info("shp ", shp_path, ": ", shp->width(), "x", shp->height(), ", ",
                    shp->frame_count(), " frames -> ", out_path);
    return 0;
}

int run_terrain(const std::string& tmp_path, const std::string& pal_path, int cols, int rows,
                const std::string& screenshot, bool fit) {
    std::ifstream tmp_in(tmp_path, std::ios::binary);
    std::ifstream pal_in(pal_path, std::ios::binary);
    if (!tmp_in || !pal_in) {
        ra2yr::log_error("cannot read ", tmp_path, " or ", pal_path);
        return 1;
    }
    std::vector<std::uint8_t> tmp_bytes((std::istreambuf_iterator<char>(tmp_in)), {});
    std::vector<std::uint8_t> pal_bytes((std::istreambuf_iterator<char>(pal_in)), {});

    std::string error;
    auto tmp = ra2yr::formats::TmpFile::from_bytes(tmp_bytes, &error);
    if (!tmp) {
        ra2yr::log_error("TMP: ", error);
        return 1;
    }
    auto palette = ra2yr::formats::Palette::from_bytes(pal_bytes, &error);
    if (!palette) {
        ra2yr::log_error("palette: ", error);
        return 1;
    }
    if (tmp->tile_count() == 0) {
        ra2yr::log_error("TMP has no tiles");
        return 1;
    }
    const auto scene = ra2yr::render::build_grid_scene(
        tmp->to_rgba(tmp->tile(0), *palette), static_cast<int>(tmp->tile_width()),
        static_cast<int>(tmp->tile_height()), cols, rows);
#if defined(RA2YR_PLATFORM_SDL) && defined(RA2YR_RENDER_BGFX)
    return run_viewer(scene, "ra2yr terrain", screenshot, fit);
#else
    static_cast<void>(screenshot);
    static_cast<void>(fit);
    ra2yr::log_error("terrain rendering requires RA2YR_ENABLE_SDL3 and RA2YR_ENABLE_BGFX");
    return 1;
#endif
}

int run_map(const std::string& map_path, const std::string& install_dir,
            const std::string& palette_override, const std::string& theater_override,
            const std::string& screenshot, bool fit) {
    std::ifstream map_in(map_path, std::ios::binary);
    if (!map_in) {
        ra2yr::log_error("cannot read ", map_path);
        return 1;
    }
    std::vector<std::uint8_t> map_bytes((std::istreambuf_iterator<char>(map_in)), {});

    // .mmx/.yro are MIX archives holding a .map and a .pkt. Retail archives
    // carry no names, so pick the member that contains the map data.
    if (auto member = ra2yr::vfs::extract_mix_member(
            map_bytes, [](std::string_view name, const std::vector<std::uint8_t>& data) {
                if (ends_with_ci(name, ".map")) {
                    return true;
                }
                return contains_ci(data.data(), data.size(), "[isomappack5]");
            })) {
        ra2yr::log_info("unwrapped a MIX-packaged map (", member->size(), " bytes)");
        map_bytes = std::move(*member);
    }

    std::string error;
    auto map = ra2yr::formats::MapFile::from_bytes(map_bytes, &error);
    if (!map) {
        ra2yr::log_error("map: ", error);
        return 1;
    }

    auto vfs = ra2yr::vfs::Vfs::open_install(install_dir, &error);
    if (!vfs) {
        ra2yr::log_error("install: ", error);
        return 1;
    }

    auto scene = ra2yr::render::build_map_scene(*map, *vfs, palette_override,
                                                theater_override, &error);
    if (!scene) {
        ra2yr::log_error("scene: ", error);
        return 1;
    }
    ra2yr::log_info("scene: ", scene->tile_count, " tiles, ", scene->sprite_count,
                    " sprites (", scene->missing_tiles, " missing tiles, ",
                    scene->missing_sprites, " missing sprites)");
    ra2yr::log_info("map ", map_path, ": theater ", map->theater(), ", ", map->width(), "x",
                    map->height(), ", ", map->cells().size(), " cells");
#if defined(RA2YR_PLATFORM_SDL) && defined(RA2YR_RENDER_BGFX)
    return run_viewer(*scene, "ra2yr map", screenshot, fit);
#else
    static_cast<void>(screenshot);
    static_cast<void>(fit);
    ra2yr::log_error("map rendering requires RA2YR_ENABLE_SDL3 and RA2YR_ENABLE_BGFX");
    return 1;
#endif
}

}  // namespace

int main(int argc, char** argv) {
    bool headless = false;
    std::string names_file;
    std::string mix_file;
    std::string extract_dir;
    std::string tmp_file;
    std::string pal_file;
    std::string map_file;
    std::string install_dir;
    std::string theater_override;
    std::string shp_file;
    std::string shp_out;
    std::string screenshot;
    bool fit = false;
    int grid_cols = 16;
    int grid_rows = 16;
    enum class Mode { Game, List, Tree, Extract, Terrain, Map, Shp } mode = Mode::Game;

    // Development builds are verbose by default; release builds stay quiet
    // unless asked otherwise.
    ra2yr::LogLevel log_level = ra2yr::LogLevel::Info;
#ifndef NDEBUG
    log_level = ra2yr::LogLevel::Debug;
#endif

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("%s\n", ra2yr::build_summary().c_str());
            return 0;
        }
        if (std::strcmp(argv[i], "--headless") == 0) {
            headless = true;
            continue;
        }
        if (std::strcmp(argv[i], "--verbose") == 0 || std::strcmp(argv[i], "-v") == 0) {
            log_level = ra2yr::LogLevel::Debug;
            continue;
        }
        if (std::strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--log-level requires a value\n");
                return 2;
            }
            const std::string value = argv[++i];
            if (value == "trace") {
                log_level = ra2yr::LogLevel::Trace;
            } else if (value == "debug") {
                log_level = ra2yr::LogLevel::Debug;
            } else if (value == "info") {
                log_level = ra2yr::LogLevel::Info;
            } else if (value == "warn") {
                log_level = ra2yr::LogLevel::Warn;
            } else if (value == "error") {
                log_level = ra2yr::LogLevel::Error;
            } else {
                std::fprintf(stderr, "unknown log level: %s\n", value.c_str());
                return 2;
            }
            continue;
        }
        if (std::strcmp(argv[i], "--names") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--names requires a file\n");
                return 2;
            }
            names_file = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--mix-list") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--mix-list requires a file\n");
                return 2;
            }
            mix_file = argv[++i];
            mode = Mode::List;
            continue;
        }
        if (std::strcmp(argv[i], "--mix-tree") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--mix-tree requires a file\n");
                return 2;
            }
            mix_file = argv[++i];
            mode = Mode::Tree;
            continue;
        }
        if (std::strcmp(argv[i], "--mix-extract") == 0) {
            if (i + 2 >= argc) {
                std::fprintf(stderr, "--mix-extract requires a file and a directory\n");
                return 2;
            }
            mix_file = argv[++i];
            extract_dir = argv[++i];
            mode = Mode::Extract;
            continue;
        }
        if (std::strcmp(argv[i], "--terrain") == 0) {
            if (i + 2 >= argc) {
                std::fprintf(stderr, "--terrain requires a TMP file and a palette\n");
                return 2;
            }
            tmp_file = argv[++i];
            pal_file = argv[++i];
            mode = Mode::Terrain;
            continue;
        }
        if (std::strcmp(argv[i], "--map") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--map requires a file\n");
                return 2;
            }
            map_file = argv[++i];
            mode = Mode::Map;
            continue;
        }
        if (std::strcmp(argv[i], "--shp-dump") == 0) {
            if (i + 3 >= argc) {
                std::fprintf(stderr, "--shp-dump requires a SHP, a palette, and an output\n");
                return 2;
            }
            shp_file = argv[++i];
            pal_file = argv[++i];
            shp_out = argv[++i];
            mode = Mode::Shp;
            continue;
        }
        if (std::strcmp(argv[i], "--install") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--install requires a directory\n");
                return 2;
            }
            install_dir = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--theater") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--theater requires a name\n");
                return 2;
            }
            theater_override = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--palette") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--palette requires a file\n");
                return 2;
            }
            pal_file = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--grid") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--grid requires COLSxROWS\n");
                return 2;
            }
            const std::string value = argv[++i];
            if (std::sscanf(value.c_str(), "%dx%d", &grid_cols, &grid_rows) != 2 ||
                grid_cols <= 0 || grid_rows <= 0) {
                std::fprintf(stderr, "invalid grid: %s\n", value.c_str());
                return 2;
            }
            continue;
        }
        if (std::strcmp(argv[i], "--fit") == 0) {
            fit = true;
            continue;
        }
        if (std::strcmp(argv[i], "--screenshot") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--screenshot requires a path\n");
                return 2;
            }
            screenshot = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
        print_usage();
        return 2;
    }

    ra2yr::set_log_level(log_level);

    ra2yr::vfs::NameDatabase names;
    const ra2yr::vfs::NameDatabase* names_ptr = nullptr;
    if (!names_file.empty()) {
        std::string error;
        if (!names.load(names_file, &error)) {
            ra2yr::log_error(error);
            return 1;
        }
        ra2yr::log_info("loaded ", names.size(), " names from ", names_file);
        names_ptr = &names;
    }

    if (mode == Mode::Shp) {
        return dump_shp(shp_file, pal_file, shp_out);
    }
    if (mode == Mode::Map) {
        if (install_dir.empty()) {
            ra2yr::log_error("--map needs --install DIR");
            return 2;
        }
        return run_map(map_file, install_dir, pal_file, theater_override, screenshot, fit);
    }
    if (mode == Mode::Terrain) {
        return run_terrain(tmp_file, pal_file, grid_cols, grid_rows, screenshot, fit);
    }
    if (mode == Mode::List) {
        return list_mix(mix_file, names_ptr);
    }
    if (mode == Mode::Tree) {
        return tree_mix(mix_file, names_ptr);
    }
    if (mode == Mode::Extract) {
        return extract_mix(mix_file, extract_dir, names_ptr);
    }

    ra2yr::set_log_level(log_level);
    ra2yr::log_info(ra2yr::build_summary(), " starting");

    std::unique_ptr<ra2yr::platform::Platform> platform;
#if defined(RA2YR_PLATFORM_SDL)
    if (!headless) {
        platform = ra2yr::platform::make_sdl_platform();
        if (!platform) {
            ra2yr::log_warn("SDL3 unavailable; falling back to headless");
        }
    }
#endif
    if (!platform) {
        platform = ra2yr::platform::make_null_platform();
    }
    ra2yr::log_info("platform backend: ", platform->name());

    ra2yr::platform::WindowDesc desc;
    desc.width = 800;
    desc.height = 600;
    desc.title = "ra2yr";
    desc.visible = !headless;
    auto window = platform->create_window(desc);
    if (!window) {
        ra2yr::log_error("failed to create a window");
        return 1;
    }
    ra2yr::log_info("window: ", window->width(), "x", window->height());

#if defined(RA2YR_RENDER_BGFX)
    ra2yr::render::BgfxRenderer renderer;
    if (!headless) {
        std::string render_error;
        if (!renderer.initialize(window->native_window(), window->width(),
                                 window->height(), &render_error)) {
            ra2yr::log_error(render_error);
            return 1;
        }
        ra2yr::log_info("renderer: ", renderer.backend());
    }
#endif

    [[maybe_unused]] int last_width = window->width();
    [[maybe_unused]] int last_height = window->height();
    if (!headless && window->native_window().handle == nullptr) {
        ra2yr::log_warn("no native window available; exiting after one frame");
        headless = true;
    }
    while (!window->should_close()) {
        window->poll();
        if (headless) {
            break;
        }
#if defined(RA2YR_RENDER_BGFX)
        if (renderer.valid()) {
            if (window->width() != last_width || window->height() != last_height) {
                last_width = window->width();
                last_height = window->height();
                renderer.resize(last_width, last_height);
            }
            renderer.render();
        }
#endif
    }

#if defined(RA2YR_RENDER_BGFX)
    renderer.shutdown();
#endif
    ra2yr::log_info("shutdown");
    return 0;
}