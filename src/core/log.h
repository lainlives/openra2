#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace ra2yr {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

void set_log_level(LogLevel level);
LogLevel log_level();

void log_message(LogLevel level, std::string_view message);
void log_message(LogLevel level, const std::string& message);

namespace detail {
template <typename... Args>
std::string concat(Args&&... args) {
    std::ostringstream os;
    (os << ... << args);
    return os.str();
}
}  // namespace detail

template <typename... Args>
void log(LogLevel level, Args&&... args) {
    log_message(level, detail::concat(std::forward<Args>(args)...));
}

template <typename... Args>
void log_info(Args&&... args) {
    log(LogLevel::Info, std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn(Args&&... args) {
    log(LogLevel::Warn, std::forward<Args>(args)...);
}

template <typename... Args>
void log_error(Args&&... args) {
    log(LogLevel::Error, std::forward<Args>(args)...);
}

}  // namespace ra2yr
