#pragma once

#include "iris/config.hpp"

namespace iris {

class DesktopShell {
public:
    explicit DesktopShell(const Config& config);

    // Returns 0 after an orderly close. Returns 1 if the window cannot be created.
    int run();

    [[nodiscard]] int windowWidth() const { return windowWidth_; }
    [[nodiscard]] int windowHeight() const { return windowHeight_; }

private:
    Config config_;
    int windowWidth_;
    int windowHeight_;
};

}  // namespace iris
