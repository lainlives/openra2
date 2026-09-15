#include "platform.h"

#include <chrono>

namespace ra2yr::platform {
namespace {

class NullWindow final : public Window {
public:
    explicit NullWindow(const WindowDesc& desc) : width_(desc.width), height_(desc.height) {}

    void poll() override {}
    bool should_close() const override { return closed_; }
    int width() const override { return width_; }
    int height() const override { return height_; }

private:
    int width_;
    int height_;
    bool closed_ = false;
};

class NullPlatform final : public Platform {
public:
    std::string name() const override { return "null"; }

    std::unique_ptr<Window> create_window(const WindowDesc& desc) override {
        return std::make_unique<NullWindow>(desc);
    }

    std::uint64_t ticks_ms() const override {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }
};

}  // namespace

std::unique_ptr<Platform> make_null_platform() {
    return std::make_unique<NullPlatform>();
}

}  // namespace ra2yr::platform
