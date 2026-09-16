#include "mix.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>

#include "blowfish.h"
#include "mix_key.h"

namespace ra2yr::vfs {
namespace {

constexpr std::string_view kNamesDbName = "local mix database.dat";

std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1]) << 8;
}

std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t crc32_bytes(const std::uint8_t* p, std::size_t n) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        c = table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::uint32_t mix_filename_hash(std::string_view name) {
    const std::size_t length = name.size();
    const std::size_t salt = length & 0xFFFFFFFCu;
    std::string obfuscated;
    obfuscated.reserve(length + 3);
    for (char c : name) {
        obfuscated.push_back(
            static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    const std::size_t remainder = length & 3u;
    if (remainder != 0 && salt < obfuscated.size()) {
        obfuscated.push_back(static_cast<char>(length - salt));
        obfuscated.append(3 - remainder, obfuscated[salt]);
    }
    return crc32_bytes(reinterpret_cast<const std::uint8_t*>(obfuscated.data()),
                       obfuscated.size());
}

void NameDatabase::add(std::string_view name) {
    if (!name.empty()) {
        by_id_[mix_filename_hash(name)] = std::string(name);
    }
}

const std::string* NameDatabase::lookup(std::uint32_t id) const {
    const auto it = by_id_.find(id);
    return it == by_id_.end() ? nullptr : &it->second;
}

bool NameDatabase::load(const std::filesystem::path& path, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error != nullptr) {
            *error = "cannot open name database " + path.string();
        }
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        const std::size_t hash = line.find_first_of(" \t");
        if (hash != std::string::npos) {
            line.resize(hash);
        }
        if (!line.empty() && line[0] != '#') {
            add(line);
        }
    }
    return true;
}

std::optional<MixArchive> MixArchive::open(const std::filesystem::path& path,
                                           std::string* error,
                                           const NameDatabase* names) {
    MixArchive archive;
    archive.path_ = path;
    archive.name_ = path.filename().string();
    archive.names_ = names;
    archive.stream_.open(path, std::ios::binary);
    if (!archive.stream_) {
        if (error != nullptr) {
            *error = "cannot open " + path.string();
        }
        return std::nullopt;
    }
    if (!archive.parse(error)) {
        return std::nullopt;
    }
    return archive;
}

std::optional<MixArchive> MixArchive::open_memory(std::vector<std::uint8_t> data,
                                                  std::string name,
                                                  std::string* error,
                                                  const NameDatabase* names) {
    MixArchive archive;
    archive.memory_ = std::move(data);
    archive.in_memory_ = true;
    archive.name_ = std::move(name);
    archive.names_ = names;
    if (!archive.parse(error)) {
        return std::nullopt;
    }
    return archive;
}

bool MixArchive::read_at(std::uint64_t offset, std::uint32_t size, std::uint8_t* out) const {
    if (size == 0) {
        return true;
    }
    if (in_memory_) {
        if (offset + size > memory_.size()) {
            return false;
        }
        std::memcpy(out, memory_.data() + offset, size);
        return true;
    }
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!stream_) {
        return false;
    }
    stream_.read(reinterpret_cast<char*>(out), size);
    return static_cast<std::uint32_t>(stream_.gcount()) == size;
}

std::vector<std::uint8_t> MixArchive::read_at_vec(std::uint64_t offset,
                                                  std::uint32_t size) const {
    std::vector<std::uint8_t> out(size);
    if (size != 0 && !read_at(offset, size, out.data())) {
        out.clear();
    }
    return out;
}

bool MixArchive::parse(std::string* error) {
    std::uint64_t total_size = memory_.size();
    if (!in_memory_) {
        stream_.clear();
        stream_.seekg(0, std::ios::end);
        const std::streamoff end = stream_.tellg();
        if (end < 0) {
            if (error != nullptr) {
                *error = name_ + ": cannot determine size";
            }
            return false;
        }
        total_size = static_cast<std::uint64_t>(end);
    }

    std::uint8_t head[10] = {};
    if (!read_at(0, sizeof(head), head)) {
        if (error != nullptr) {
            *error = name_ + ": too short to be a MIX archive";
        }
        return false;
    }

    std::uint32_t count = 0;
    std::vector<std::uint8_t> index;

    if (le16(head) != 0) {
        // Old format: 2-byte count, 4-byte body size, then the index.
        count = le16(head);
        body_size_ = le32(head + 2);
        body_offset_ = 6 + static_cast<std::uint64_t>(count) * 12;
        index = read_at_vec(6, count * 12);
    } else {
        flags_ = le16(head + 2);
        checksummed_ = (flags_ & 1u) != 0;
        encrypted_ = (flags_ & 2u) != 0;
        if (encrypted_) {
            std::vector<std::uint8_t> keysource = read_at_vec(4, 80);
            if (keysource.size() != 80) {
                if (error != nullptr) {
                    *error = name_ + ": truncated keysource";
                }
                return false;
            }
            std::uint8_t key[56];
            derive_blowfish_key(keysource.data(), key);
            Blowfish bf;
            bf.set_key(key, 56);

            std::uint8_t block[8] = {};
            if (!read_at(84, 8, block)) {
                if (error != nullptr) {
                    *error = name_ + ": truncated encrypted header";
                }
                return false;
            }
            bf.decrypt(block, 8, block);
            count = le16(block);
            body_size_ = le32(block + 2);

            const std::uint32_t index_size = count * 12;
            const std::uint32_t remaining = index_size >= 2 ? index_size - 2 : 0;
            const std::uint32_t padding = (8 - (remaining % 8)) % 8;
            std::vector<std::uint8_t> enc = read_at_vec(92, remaining + padding);
            if (enc.size() != remaining + padding) {
                if (error != nullptr) {
                    *error = name_ + ": truncated encrypted index";
                }
                return false;
            }
            bf.decrypt(enc.data(), static_cast<int>(enc.size()), enc.data());
            index.resize(index_size);
            index[0] = block[6];
            index[1] = block[7];
            if (remaining > 0) {
                std::memcpy(index.data() + 2, enc.data(), remaining);
            }
            body_offset_ = 92 + static_cast<std::uint64_t>(remaining) + padding;
        } else {
            // New unencrypted: flags, then 2-byte count and 4-byte body size.
            count = le16(head + 4);
            body_size_ = le32(head + 6);
            body_offset_ = 10 + static_cast<std::uint64_t>(count) * 12;
            index = read_at_vec(10, count * 12);
        }
    }

    if (index.size() != static_cast<std::size_t>(count) * 12) {
        if (error != nullptr) {
            *error = name_ + ": index read failed";
        }
        return false;
    }

    entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* p = index.data() + static_cast<std::size_t>(i) * 12;
        MixEntry entry;
        entry.id = le32(p);
        entry.offset = le32(p + 4);
        entry.size = le32(p + 8);
        if (static_cast<std::uint64_t>(body_offset_) + entry.offset + entry.size >
            total_size) {
            if (error != nullptr) {
                *error = name_ + ": entry lies outside the archive";
            }
            return false;
        }
        entries_.push_back(std::move(entry));
    }

    // Resolve names from the embedded local mix database.
    const std::uint32_t db_id = mix_filename_hash(kNamesDbName);
    for (const MixEntry& entry : entries_) {
        if (entry.id != db_id) {
            continue;
        }
        const std::vector<std::uint8_t> blob = read(entry);
        if (blob.size() <= 52) {
            break;
        }
        std::size_t start = 52;
        while (start < blob.size()) {
            std::size_t end = start;
            while (end < blob.size() && blob[end] != 0) {
                ++end;
            }
            if (end > start) {
                std::string filename(reinterpret_cast<const char*>(blob.data() + start),
                                     end - start);
                const std::uint32_t id = mix_filename_hash(filename);
                for (MixEntry& candidate : entries_) {
                    if (candidate.id == id) {
                        candidate.name = filename;
                    }
                }
            }
            start = end + 1;
        }
        break;
    }

    for (MixEntry& entry : entries_) {
        if (entry.name.empty() && names_ != nullptr) {
            if (const std::string* name = names_->lookup(entry.id)) {
                entry.name = *name;
            }
        }
        if (entry.name.empty()) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "_%08X", entry.id);
            entry.name = buf;
        }
    }

    return true;
}

const MixEntry* MixArchive::find(std::string_view name) const {
    for (const MixEntry& entry : entries_) {
        if (iequals(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

const MixEntry* MixArchive::find(std::uint32_t id) const {
    for (const MixEntry& entry : entries_) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<std::uint8_t> MixArchive::read(const MixEntry& entry) const {
    return read_at_vec(body_offset_ + entry.offset, entry.size);
}

bool is_mix_filename(std::string_view name) {
    if (name.size() < 4) {
        return false;
    }
    const std::string_view ext = name.substr(name.size() - 4);
    return std::tolower(static_cast<unsigned char>(ext[0])) == '.' &&
           std::tolower(static_cast<unsigned char>(ext[1])) == 'm' &&
           std::tolower(static_cast<unsigned char>(ext[2])) == 'i' &&
           std::tolower(static_cast<unsigned char>(ext[3])) == 'x';
}

namespace {

void expand_recursive(const std::shared_ptr<MixArchive>& archive,
                      const std::vector<std::string>& chain, int depth, int max_depth,
                      const NameDatabase* names, std::vector<MixLeaf>* leaves,
                      const std::function<void(const MixArchive&, int)>* archive_visitor,
                      std::string* error) {
    if (archive_visitor != nullptr && *archive_visitor) {
        (*archive_visitor)(*archive, depth);
    }
    for (const MixEntry& entry : archive->entries()) {
        if (is_mix_filename(entry.name) && depth < max_depth) {
            std::string nested_error;
            auto nested = MixArchive::open_memory(archive->read(entry), entry.name,
                                                  &nested_error, names);
            if (nested) {
                auto nested_ptr = std::make_shared<MixArchive>(std::move(*nested));
                std::vector<std::string> child_chain = chain;
                child_chain.push_back(entry.name);
                expand_recursive(nested_ptr, child_chain, depth + 1, max_depth, names,
                                 leaves, archive_visitor, error);
                continue;
            }
            if (error != nullptr && !nested_error.empty()) {
                *error = nested_error;
            }
            // Fall through: a .mix that does not parse is kept as a leaf.
        }
        if (leaves != nullptr) {
            MixLeaf leaf;
            leaf.name = entry.name;
            leaf.id = entry.id;
            leaf.size = entry.size;
            leaf.chain = chain;
            std::shared_ptr<MixArchive> keep = archive;
            leaf.read = [keep, entry]() { return keep->read(entry); };
            leaves->push_back(std::move(leaf));
        }
    }
}

std::shared_ptr<MixArchive> open_root(const std::filesystem::path& path,
                                      const NameDatabase* names, std::string* error) {
    auto root = MixArchive::open(path, error, names);
    if (!root) {
        return nullptr;
    }
    return std::make_shared<MixArchive>(std::move(*root));
}

}  // namespace

std::vector<MixLeaf> enumerate_leaves(const std::filesystem::path& path,
                                      const NameDatabase* names, int max_depth,
                                      std::string* error) {
    std::vector<MixLeaf> leaves;
    auto root = open_root(path, names, error);
    if (!root) {
        return leaves;
    }
    const std::function<void(const MixArchive&, int)>* no_visitor = nullptr;
    expand_recursive(root, {root->name()}, 0, max_depth, names, &leaves, no_visitor, error);
    return leaves;
}

void for_each_archive(const std::filesystem::path& path,
                      const std::function<void(const MixArchive&, int depth)>& visitor,
                      const NameDatabase* names, int max_depth, std::string* error) {
    auto root = open_root(path, names, error);
    if (!root) {
        return;
    }
    expand_recursive(root, {root->name()}, 0, max_depth, names, nullptr, &visitor, error);
}

std::optional<std::vector<std::uint8_t>> extract_mix_member(
    const std::vector<std::uint8_t>& bytes,
    const std::function<bool(std::string_view, const std::vector<std::uint8_t>&)>&
        predicate,
    const NameDatabase* names) {
    std::string error;
    auto archive = MixArchive::open_memory(bytes, "<archive>", &error, names);
    if (!archive) {
        return std::nullopt;
    }
    for (const MixEntry& entry : archive->entries()) {
        std::vector<std::uint8_t> data = archive->read(entry);
        if (data.empty()) {
            continue;
        }
        if (predicate(entry.name, data)) {
            return data;
        }
    }
    return std::nullopt;
}

}  // namespace ra2yr::vfs