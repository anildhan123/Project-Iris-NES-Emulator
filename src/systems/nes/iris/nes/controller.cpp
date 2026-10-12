#include "iris/nes/controller.hpp"

namespace iris::systems::nes {

void Controller::setButtons(std::uint8_t buttons) {
    buttons_ = buttons;
    if (strobe_) {
        shift_ = buttons_;
    }
}

void Controller::writeStrobe(std::uint8_t value) {
    strobe_ = (value & 0x01) != 0;
    if (strobe_) {
        shift_ = buttons_;
    }
}

std::uint8_t Controller::read() {
    const std::uint8_t bit = static_cast<std::uint8_t>(shift_ & 0x01);
    if (!strobe_) {
        shift_ = static_cast<std::uint8_t>((shift_ >> 1) | 0x80);
    }
    return bit;
}

}  // namespace iris::systems::nes
