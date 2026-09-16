#include "vfs/vfs.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

#include "core/log.h"

namespace ra2yr::vfs {
namespace {

std::string lowercase(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool has_mix_extension(std::string_view name) {
    if (name.size() < 4) {
        return false;
    }
    return lowercase(name.substr(name.size() - 4)) == ".mix";
}

int numeric_suffix(std::string_view name, std::string_view prefix) {
    if (name.size() <= prefix.size()) {
        return -1;
    }
    int value = 0;
    for (std::size_t i = prefix.size(); i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
            return value;
        }
        value = value * 10 + (name[i] - '0');
    }
    return value;
}

// Priority of a top-level archive name: lower sorts first. Expansion/mod
// archives override the base archives; the mission disk overrides Red Alert 2.
int archive_priority(const std::string& name) {
    const std::string lower = lowercase(name);
    if (lower.rfind("expandmd", 0) == 0) {
        return 100 - numeric_suffix(lower, "expandmd");
    }
    if (lower.rfind("expand", 0) == 0) {
        return 200 - numeric_suffix(lower, "expand");
    }
    if (lower == "ra2md.mix") return 300;
    if (lower == "langmd.mix") return 310;
    if (lower == "ra2.mix") return 320;
    if (lower == "language.mix") return 330;
    if (lower.rfind("ecachemd", 0) == 0) return 400;
    if (lower.rfind("ecache", 0) == 0) return 410;
    return 500;
}

}  // namespace

std::optional<Vfs> Vfs::open_install(const std::filesystem::path& directory,
                                     std::string* error) {
    if (!std::filesystem::is_directory(directory)) {
        if (error != nullptr) {
            *error = "not a directory: " + directory.string();
        }
        return std::nullopt;
    }

    Vfs vfs;
    vfs.install_dir_ = directory;

    std::vector<std::string> archives;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (has_mix_extension(name)) {
            archives.push_back(name);
        } else {
            vfs.loose_[lowercase(name)] = entry.path();
        }
    }
    std::stable_sort(archives.begin(), archives.end(), [](const std::string& a,
                                                          const std::string& b) {
        const int pa = archive_priority(a);
        const int pb = archive_priority(b);
        if (pa != pb) {
            return pa < pb;
        }
        return a < b;
    });

    for (const std::string& name : archives) {
        std::string open_error;
        auto archive = MixArchive::open(directory / name, &open_error);
        if (!archive) {
            log(LogLevel::Debug, "vfs: skipping ", name, ": ", open_error);
            continue;
        }
        vfs.top_.push_back(std::make_shared<MixArchive>(std::move(*archive)));
        vfs.top_names_.push_back(name);
    }
    if (vfs.top_.empty()) {
        if (error != nullptr) {
            *error = "no readable MIX archives in " + directory.string();
        }
        return std::nullopt;
    }
    log(LogLevel::Debug, "vfs: ", vfs.top_.size(), " archives, ", vfs.loose_.size(),
        " loose files in ", directory.string());
    return vfs;
}

std::optional<std::vector<std::uint8_t>> Vfs::read_loose(std::string_view name) const {
    const auto it = loose_.find(lowercase(name));
    if (it == loose_.end()) {
        return std::nullopt;
    }
    std::ifstream in(it->second, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

std::optional<std::vector<std::uint8_t>> Vfs::read(std::string_view name) const {
    if (auto loose = read_loose(name)) {
        return loose;
    }
    const std::uint32_t id = mix_filename_hash(name);
    const auto try_archive = [id](const std::shared_ptr<MixArchive>& archive)
        -> std::optional<std::vector<std::uint8_t>> {
        const MixEntry* entry = archive->find(id);
        if (entry == nullptr) {
            return std::nullopt;
        }
        return archive->read(*entry);
    };
    for (auto it = opened_.rbegin(); it != opened_.rend(); ++it) {
        if (auto data = try_archive(*it)) {
            return data;
        }
    }
    for (const auto& archive : top_) {
        if (auto data = try_archive(archive)) {
            return data;
        }
    }
    return std::nullopt;
}

const MixArchive* Vfs::find_mix(std::string_view name) const {
    const std::string wanted = lowercase(name);
    for (const auto& archive : opened_) {
        if (lowercase(archive->name()) == wanted) {
            return archive.get();
        }
    }
    return nullptr;
}

const MixArchive* Vfs::open_mix(std::string_view name) {
    if (const MixArchive* existing = find_mix(name)) {
        return existing;
    }
    const std::uint32_t id = mix_filename_hash(name);
    const auto search = [id](const std::shared_ptr<MixArchive>& archive)
        -> std::optional<std::vector<std::uint8_t>> {
        const MixEntry* entry = archive->find(id);
        if (entry == nullptr) {
            return std::nullopt;
        }
        return archive->read(*entry);
    };

    std::optional<std::vector<std::uint8_t>> bytes;
    for (const auto& archive : top_) {
        bytes = search(archive);
        if (bytes) {
            break;
        }
    }
    if (!bytes) {
        for (const auto& archive : opened_) {
            bytes = search(archive);
            if (bytes) {
                break;
            }
        }
    }
    if (!bytes) {
        return nullptr;
    }
    std::string open_error;
    auto nested = MixArchive::open_memory(std::move(*bytes), std::string(name), &open_error);
    if (!nested) {
        log(LogLevel::Debug, "vfs: cannot open mix ", name, ": ", open_error);
        return nullptr;
    }
    opened_.push_back(std::make_shared<MixArchive>(std::move(*nested)));
    log(LogLevel::Debug, "vfs: opened mix ", name, " (", opened_.size(), " open)");
    return opened_.back().get();
}

}  // namespace ra2yr::vfs
