#include "formats/ini.h"

#include <cctype>
#include <cstdlib>

namespace ra2yr::formats {
namespace {

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

}  // namespace

IniFile IniFile::parse(std::string_view text) {
    IniFile ini;
    std::string current;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = text.find('\n', pos);
        std::string line = trim(text.substr(pos, end == std::string_view::npos
                                                     ? std::string_view::npos
                                                     : end - pos));
        pos = (end == std::string_view::npos) ? text.size() + 1 : end + 1;

        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            current = lower(trim(std::string_view(line).substr(1, line.size() - 2)));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos || current.empty()) {
            continue;
        }
        std::string key = lower(trim(std::string_view(line).substr(0, equals)));
        std::string value = trim(std::string_view(line).substr(equals + 1));
        ini.sections_[current].emplace_back(std::move(key), std::move(value));
    }
    return ini;
}

const IniFile::Entries* IniFile::section(std::string_view name) const {
    const auto it = sections_.find(lower(name));
    return it == sections_.end() ? nullptr : &it->second;
}

const std::string* IniFile::get(std::string_view section, std::string_view key) const {
    const Entries* entries = this->section(section);
    if (entries == nullptr) {
        return nullptr;
    }
    const std::string wanted = lower(key);
    const std::string* result = nullptr;
    for (const Entry& entry : *entries) {
        if (entry.first == wanted) {
            result = &entry.second;
        }
    }
    return result;
}

int IniFile::get_int(std::string_view section, std::string_view key, int fallback) const {
    const std::string* value = get(section, key);
    if (value == nullptr) {
        return fallback;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value->c_str(), &end, 10);
    if (end == value->c_str()) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

}  // namespace ra2yr::formats
