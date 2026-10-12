#pragma once

#include <cstdint>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::nes {

class Controller {
public:
    void setButtons(std::uint8_t buttons);
    void writeStrobe(std::uint8_t value);
    [[nodiscard]] std::uint8_t read();
    void captureState(iris::ByteWriter& writer) const;
    [[nodiscard]] bool applyState(iris::ByteReader& reader);

private:
    std::uint8_t buttons_ = 0;
    std::uint8_t shift_ = 0;
    bool strobe_ = false;
};

}  // namespace iris::systems::nes
