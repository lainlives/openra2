#include "log.h"

#include <cstdio>
#include <mutex>

namespace ra2yr {
namespace {

LogLevel g_level = LogLevel::Info;
std::mutex& log_mutex() {
    static std::mutex m;
    return m;
}

std::string_view level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warn: return "warn";
        case LogLevel::Error: return "error";
    }
    return "?";
}

}  // namespace

void set_log_level(LogLevel level) {
    g_level = level;
}

LogLevel log_level() {
    return g_level;
}

void log_message(LogLevel level, std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(g_level)) {
        return;
    }
    std::lock_guard<std::mutex> lock(log_mutex());
    std::fprintf(stderr, "[%s] %.*s\n", level_name(level).data(),
                 static_cast<int>(message.size()), message.data());
}

void log_message(LogLevel level, const std::string& message) {
    log_message(level, std::string_view(message));
}

}  // namespace ra2yr
