#include "iris/nes/controller.hpp"

#include "iris/byte_io.hpp"

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

void Controller::captureState(ByteWriter& writer) const {
    writer.u8(buttons_);
    writer.u8(shift_);
    writer.boolean(strobe_);
}

bool Controller::applyState(ByteReader& reader) {
    const std::uint8_t buttons = reader.u8();
    const std::uint8_t shift = reader.u8();
    const bool strobe = reader.boolean();
    if (!reader.ok()) {
        return false;
    }
    buttons_ = buttons;
    shift_ = shift;
    strobe_ = strobe;
    return true;
}

}  // namespace iris::systems::nes
