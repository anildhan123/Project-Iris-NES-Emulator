#pragma once

#include <cstdint>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::nes {

enum class ControllerDevice : std::uint8_t {
    Pad = 0,
    Zapper = 1,
    FourScore = 2,
};

class Controller {
public:
    void setDevice(ControllerDevice device);
    [[nodiscard]] ControllerDevice device() const { return device_; }
    void setButtons(std::uint8_t buttons);
    void setExtraButtons(std::uint8_t buttons);
    void setZapper(bool trigger, bool light);
    void writeStrobe(std::uint8_t value);
    [[nodiscard]] std::uint8_t read();
    void captureState(iris::ByteWriter& writer) const;
    [[nodiscard]] bool applyState(iris::ByteReader& reader);

private:
    void latch();

    ControllerDevice device_ = ControllerDevice::Pad;
    std::uint8_t buttons_ = 0;
    std::uint8_t extra_ = 0;
    std::uint32_t shift_ = 0;
    bool strobe_ = false;
    bool zapperTrigger_ = false;
    bool zapperLight_ = false;
};

}  // namespace iris::systems::nes
