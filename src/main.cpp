#include <cstdio>
#include <cstring>
#include <string>

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
                 "  --version        print the build identity and exit\n"
                 "  --headless       initialize and exit without a frame loop\n"
                 "  --mix-list FILE  list the entries of a MIX archive\n"
                 "  --help           show this message\n",
                 ra2yr::kVersionString);
}

int list_mix(const std::string& path, const ra2yr::vfs::NameDatabase* names) {
    std::string error;
    auto archive = ra2yr::vfs::MixArchive::open(path, &error, names);
    if (!archive) {
        ra2yr::log_error(error);
        return 1;
    }
    std::string flags;
    if (archive->checksummed()) flags += " checksummed";
    if (archive->encrypted()) flags += " encrypted";
    std::printf("%s: %zu entries, %u body bytes,%s\n", archive->name().c_str(),
                archive->entries().size(), archive->body_size(),
                flags.empty() ? "" : flags.c_str());
    for (const auto& entry : archive->entries()) {
        std::printf("  %08X %10u  %s\n", entry.id, entry.size, entry.name.c_str());
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    bool headless = false;
    std::string mix_file;
    std::string names_file;

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

    if (!mix_file.empty()) {
        ra2yr::vfs::NameDatabase names;
        if (!names_file.empty()) {
            std::string error;
            if (!names.load(names_file, &error)) {
                ra2yr::log_error(error);
                return 1;
            }
            ra2yr::log_info("loaded ", names.size(), " names from ", names_file);
        }
        return list_mix(mix_file, names.size() != 0 ? &names : nullptr);
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