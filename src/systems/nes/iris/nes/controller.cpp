#include "iris/nes/controller.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {

void Controller::setDevice(ControllerDevice device) { device_ = device; }

void Controller::setButtons(std::uint8_t buttons) {
    buttons_ = buttons;
    if (strobe_) {
        latch();
    }
}

void Controller::setExtraButtons(std::uint8_t buttons) {
    extra_ = buttons;
    if (strobe_) {
        latch();
    }
}

void Controller::setZapper(bool trigger, bool light) {
    zapperTrigger_ = trigger;
    zapperLight_ = light;
}

void Controller::latch() {
    if (device_ == ControllerDevice::FourScore) {
        // P1/P2 (8) + P3/P4 (8) + signature nibble pattern.
        shift_ = static_cast<std::uint32_t>(buttons_) | (static_cast<std::uint32_t>(extra_) << 8) | (0x10u << 16);
    } else {
        shift_ = buttons_;
    }
}

void Controller::writeStrobe(std::uint8_t value) {
    strobe_ = (value & 0x01) != 0;
    if (strobe_) {
        latch();
    }
}

std::uint8_t Controller::read() {
    if (device_ == ControllerDevice::Zapper) {
        // D3 clear when light is seen; D4 set when trigger is pulled.
        return static_cast<std::uint8_t>((zapperLight_ ? 0 : 0x08) | (zapperTrigger_ ? 0x10 : 0));
    }
    const std::uint8_t bit = static_cast<std::uint8_t>(shift_ & 0x01);
    if (!strobe_) {
        if (device_ == ControllerDevice::FourScore) {
            shift_ = (shift_ >> 1) | 0x800000u;
        } else {
            shift_ = (shift_ >> 1) | 0x80u;
        }
    }
    return bit;
}

void Controller::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(device_));
    writer.u8(buttons_);
    writer.u8(extra_);
    writer.u32(shift_);
    writer.boolean(strobe_);
    writer.boolean(zapperTrigger_);
    writer.boolean(zapperLight_);
}

bool Controller::applyState(ByteReader& reader) {
    const auto device = static_cast<ControllerDevice>(reader.u8());
    const std::uint8_t buttons = reader.u8();
    const std::uint8_t extra = reader.u8();
    const std::uint32_t shift = reader.u32();
    const bool strobe = reader.boolean();
    const bool trigger = reader.boolean();
    const bool light = reader.boolean();
    if (!reader.ok() || static_cast<std::uint8_t>(device) > static_cast<std::uint8_t>(ControllerDevice::FourScore)) {
        return false;
    }
    device_ = device;
    buttons_ = buttons;
    extra_ = extra;
    shift_ = shift;
    strobe_ = strobe;
    zapperTrigger_ = trigger;
    zapperLight_ = light;
    return true;
}

}  // namespace iris::systems::nes
