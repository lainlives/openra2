#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/log.h"
#include "core/version.h"
#include "platform/platform.h"
#include "vfs/mix.h"

namespace {

void print_usage() {
    std::fprintf(stderr,
                 "ra2yr %s\n"
                 "\n"
                 "Usage: ra2yr [options]\n"
                 "  --version             print the build identity and exit\n"
                 "  --headless            initialize and exit without a frame loop\n"
                 "  --names FILE          filename database for the MIX operations\n"
                 "  --mix-list FILE       list the direct entries of a MIX archive\n"
                 "  --mix-tree FILE       list the archive tree, recursing into .mix\n"
                 "  --mix-extract FILE DIR  recursively extract every file\n"
                 "  --help                show this message\n",
                 ra2yr::kVersionString);
}

void print_archive_flags(const ra2yr::vfs::MixArchive& archive, std::string* out) {
    if (archive.checksummed()) *out += " checksummed";
    if (archive.encrypted()) *out += " encrypted";
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

}  // namespace

int main(int argc, char** argv) {
    bool headless = false;
    std::string names_file;
    std::string mix_file;
    std::string extract_dir;
    enum class Mode { Game, List, Tree, Extract } mode = Mode::Game;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("%s\n", ra2yr::build_summary().c_str());
            return 0;
        }
        if (std::strcmp(argv[i], "--headless") == 0) {
            headless = true;
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
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
        print_usage();
        return 2;
    }

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

    if (mode == Mode::List) {
        return list_mix(mix_file, names_ptr);
    }
    if (mode == Mode::Tree) {
        return tree_mix(mix_file, names_ptr);
    }
    if (mode == Mode::Extract) {
        return extract_mix(mix_file, extract_dir, names_ptr);
    }

    ra2yr::set_log_level(ra2yr::LogLevel::Info);
    ra2yr::log_info(ra2yr::build_summary(), " starting");

    auto platform = ra2yr::platform::make_null_platform();
    ra2yr::log_info("platform backend: ", platform->name());

    ra2yr::platform::WindowDesc desc;
    desc.width = 800;
    desc.height = 600;
    desc.title = "ra2yr";
    desc.visible = !headless;
    auto window = platform->create_window(desc);
    ra2yr::log_info("window: ", window->width(), "x", window->height());

    while (!window->should_close()) {
        window->poll();
        if (headless) {
            break;
        }
    }

    ra2yr::log_info("shutdown");
    return 0;
}