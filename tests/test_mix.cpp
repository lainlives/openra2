#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "../src/vfs/blowfish.h"
#include "../src/vfs/mix.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " \
                      << #cond << "\n";                                    \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

std::vector<std::uint8_t> bytes_from_hex(const std::string& hex) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

void test_blowfish_vectors() {
    struct Vector {
        const char* key;
        const char* plain;
        const char* cipher;
    };
    const Vector vectors[] = {
        {"0000000000000000", "0000000000000000", "4EF997456198DD78"},
        {"FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A"},
        {"3000000000000000", "1000000000000001", "7D856F9A613063F2"},
        {"1111111111111111", "1111111111111111", "2466DD878B963C9D"},
        {"0123456789ABCDEF", "1111111111111111", "61F9C3802281B096"},
    };
    for (const Vector& v : vectors) {
        const auto key = bytes_from_hex(v.key);
        const auto plain = bytes_from_hex(v.plain);
        const auto expected = bytes_from_hex(v.cipher);

        ra2yr::vfs::Blowfish bf;
        bf.set_key(key.data(), static_cast<int>(key.size()));

        std::vector<std::uint8_t> cipher(8, 0);
        bf.encrypt(plain.data(), 8, cipher.data());
        CHECK(cipher == expected);

        std::vector<std::uint8_t> roundtrip(8, 0);
        bf.decrypt(cipher.data(), 8, roundtrip.data());
        CHECK(roundtrip == plain);
    }
}

void test_filename_hash() {
    CHECK(ra2yr::vfs::mix_filename_hash("local mix database.dat") == 0x366E051Fu);
    CHECK(ra2yr::vfs::mix_filename_hash("rulesmd.ini") == 0x8218F9F4u);
    CHECK(ra2yr::vfs::mix_filename_hash("isourb.mix") == 0x80E03363u);
    CHECK(ra2yr::vfs::mix_filename_hash("key.ini") == 0x763C81DDu);
    // Case-insensitive.
    CHECK(ra2yr::vfs::mix_filename_hash("RULESMD.INI") ==
          ra2yr::vfs::mix_filename_hash("rulesmd.ini"));
}

std::vector<std::uint8_t> make_old_mix(
    const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& files) {
    std::vector<std::uint8_t> out;
    const auto push16 = [&out](std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>(v >> 8));
    };
    const auto push32 = [&out](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
        }
    };

    std::uint32_t body_size = 0;
    for (const auto& f : files) {
        body_size += static_cast<std::uint32_t>(f.second.size());
    }

    push16(static_cast<std::uint16_t>(files.size()));
    push32(body_size);
    std::uint32_t offset = 0;
    for (const auto& f : files) {
        push32(ra2yr::vfs::mix_filename_hash(f.first));
        push32(offset);
        push32(static_cast<std::uint32_t>(f.second.size()));
        offset += static_cast<std::uint32_t>(f.second.size());
    }
    for (const auto& f : files) {
        out.insert(out.end(), f.second.begin(), f.second.end());
    }
    return out;
}

void test_nested_recursion() {
    const std::vector<std::uint8_t> leaf = {9, 8, 7, 6};
    const auto inner = make_old_mix({{"leaf.bin", leaf}});
    const std::vector<std::uint8_t> top = {1, 2, 3};
    const auto outer = make_old_mix({{"inner.mix", inner}, {"top.txt", top}});

    const auto path =
        std::filesystem::temp_directory_path() / "ra2yr_test_nested.mix";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(outer.data()),
                  static_cast<std::streamsize>(outer.size()));
    }

    std::string error;
    ra2yr::vfs::NameDatabase names;
    names.add("inner.mix");
    names.add("leaf.bin");
    names.add("top.txt");
    const auto leaves = ra2yr::vfs::enumerate_leaves(path, &names, 8, &error);
    std::filesystem::remove(path);

    CHECK(leaves.size() == 2);
    const ra2yr::vfs::MixLeaf* nested = nullptr;
    const ra2yr::vfs::MixLeaf* direct = nullptr;
    for (const auto& l : leaves) {
        if (l.name == "leaf.bin") nested = &l;
        if (l.name == "top.txt") direct = &l;
    }
    CHECK(nested != nullptr);
    CHECK(direct != nullptr);
    if (nested != nullptr) {
        CHECK(nested->chain.size() == 2);
        CHECK(nested->bytes() == leaf);
    }
    if (direct != nullptr) {
        CHECK(direct->chain.size() == 1);
        CHECK(direct->bytes() == top);
    }
}

void test_old_format_roundtrip() {    const std::vector<std::uint8_t> data_a = {'h', 'e', 'l', 'l', 'o'};
    const std::vector<std::uint8_t> data_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const auto archive_bytes = make_old_mix({{"a.txt", data_a}, {"b.bin", data_b}});

    std::string error;
    auto archive = ra2yr::vfs::MixArchive::open_memory(archive_bytes, "test.mix", &error);
    CHECK(archive.has_value());
    if (!archive) {
        std::cerr << "open failed: " << error << "\n";
        return;
    }
    CHECK(archive->entries().size() == 2);

    const auto* entry_a = archive->find(std::uint32_t{0x77976E4F});
    CHECK(entry_a != nullptr);
    if (entry_a != nullptr) {
        CHECK(entry_a->size == data_a.size());
        CHECK(archive->read(*entry_a) == data_a);
    }

    const auto* entry_b = archive->find(std::uint32_t{ra2yr::vfs::mix_filename_hash("b.bin")});
    CHECK(entry_b != nullptr);
    if (entry_b != nullptr) {
        CHECK(archive->read(*entry_b) == data_b);
    }
}

}  // namespace

int main() {
    test_blowfish_vectors();
    test_filename_hash();
    test_nested_recursion();
    test_old_format_roundtrip();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all mix checks passed\n";
    return 0;
}