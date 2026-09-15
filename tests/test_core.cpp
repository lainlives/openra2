#include <cstdint>
#include <iostream>
#include <string>

#include "../src/core/log.h"
#include "../src/core/version.h"
#include "../src/platform/platform.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: "   \
                      << #cond << "\n";                                      \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

void test_version() {
    CHECK(std::string(ra2yr::kVersionString) == "0.1.0");
    CHECK(ra2yr::build_summary().find("ra2yr") != std::string::npos);
}

void test_null_platform() {
    auto platform = ra2yr::platform::make_null_platform();
    CHECK(platform != nullptr);
    CHECK(platform->name() == "null");

    ra2yr::platform::WindowDesc desc;
    desc.width = 320;
    desc.height = 240;
    auto window = platform->create_window(desc);
    CHECK(window != nullptr);
    CHECK(window->width() == 320);
    CHECK(window->height() == 240);
    CHECK(!window->should_close());

    const std::uint64_t a = platform->ticks_ms();
    const std::uint64_t b = platform->ticks_ms();
    CHECK(b >= a);
}

}  // namespace

int main() {
    ra2yr::set_log_level(ra2yr::LogLevel::Error);
    test_version();
    test_null_platform();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
