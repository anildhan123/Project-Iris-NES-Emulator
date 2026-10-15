#include "iris/snes/demo_rom.hpp"

#include <string>
#include <unordered_map>

namespace iris::systems::snes {
namespace {

class Program {
public:
    void label(const char* name) { labels_[name] = static_cast<int>(bytes_.size()); }

    void byte(std::uint8_t value) { bytes_.push_back(value); }

    void immediate(std::uint8_t opcode, std::uint8_t value) {
        bytes_.push_back(opcode);
        bytes_.push_back(value);
    }

    void page(std::uint8_t opcode, std::uint8_t address) {
        bytes_.push_back(opcode);
        bytes_.push_back(address);
    }

    void absolute(std::uint8_t opcode, std::uint16_t address) {
        bytes_.push_back(opcode);
        bytes_.push_back(static_cast<std::uint8_t>(address));
        bytes_.push_back(static_cast<std::uint8_t>(address >> 8));
    }

    void branch(std::uint8_t opcode, const char* name) {
        bytes_.push_back(opcode);
        fixes_.push_back(Fix{static_cast<int>(bytes_.size()), name, true});
        bytes_.push_back(0);
    }

    void jump(const char* name) {
        bytes_.push_back(0x4C);
        fixes_.push_back(Fix{static_cast<int>(bytes_.size()), name, false});
        bytes_.push_back(0);
        bytes_.push_back(0);
    }

    [[nodiscard]] int address(const char* name, std::uint16_t origin) const {
        return origin + labels_.at(name);
    }

    [[nodiscard]] std::vector<std::uint8_t> finish(std::uint16_t origin) {
        for (const Fix& fix : fixes_) {
            const int target = origin + labels_.at(fix.name);
            if (fix.relative) {
                const int from = origin + fix.at + 1;
                bytes_[static_cast<std::size_t>(fix.at)] = static_cast<std::uint8_t>(target - from);
            } else {
                bytes_[static_cast<std::size_t>(fix.at)] = static_cast<std::uint8_t>(target);
                bytes_[static_cast<std::size_t>(fix.at) + 1] = static_cast<std::uint8_t>(target >> 8);
            }
        }
        return bytes_;
    }

private:
    struct Fix {
        int at = 0;
        const char* name = nullptr;
        bool relative = false;
    };

    std::vector<std::uint8_t> bytes_;
    std::unordered_map<std::string, int> labels_;
    std::vector<Fix> fixes_;
};

void store(Program& program, std::uint8_t value, std::uint16_t address) {
    program.immediate(0xA9, value);
    program.absolute(0x8D, address);
}

}  // namespace

std::vector<std::uint8_t> makeIrisSnesDemo() {
    Program program;
    const auto fillWords = [&](std::uint8_t low, std::uint8_t high, int words) {
        for (int word = 0; word < words; ++word) {
            store(program, low, 0x2118);
            store(program, high, 0x2119);
        }
    };

    program.byte(0x78);
    program.byte(0xD8);
    store(program, 0x8F, 0x2100);
    store(program, 0x00, 0x2105);
    store(program, 0x00, 0x2107);
    store(program, 0x01, 0x210B);
    store(program, 0x01, 0x2101);
    store(program, 0x80, 0x2115);

    store(program, 0x00, 0x2116);
    store(program, 0x20, 0x2117);
    fillWords(0xFF, 0x00, 8);
    fillWords(0x00, 0x00, 8);

    store(program, 0x00, 0x2116);
    store(program, 0x10, 0x2117);
    fillWords(0xFF, 0x00, 8);
    fillWords(0x00, 0xFF, 8);

    store(program, 0x00, 0x2116);
    store(program, 0x00, 0x2117);
    program.immediate(0xA0, 28);
    program.label("row");
    program.immediate(0xA2, 32);
    program.label("column");
    program.byte(0x8A);
    program.immediate(0x29, 0x01);
    program.absolute(0x8D, 0x2118);
    store(program, 0x00, 0x2119);
    program.byte(0xCA);
    program.branch(0xD0, "column");
    program.byte(0x88);
    program.branch(0xD0, "row");

    store(program, 0x00, 0x2121);
    store(program, 0x00, 0x2122);
    store(program, 0x00, 0x2122);
    store(program, 0x01, 0x2121);
    store(program, 0x04, 0x2122);
    store(program, 0x41, 0x2122);
    store(program, 0x02, 0x2121);
    store(program, 0x08, 0x2122);
    store(program, 0x62, 0x2122);
    store(program, 0x81, 0x2121);
    store(program, 0x1F, 0x2122);
    store(program, 0x13, 0x2122);

    program.immediate(0xA9, 120);
    program.page(0x85, 0x00);
    program.immediate(0xA9, 104);
    program.page(0x85, 0x01);
    store(program, 0x00, 0x2102);
    store(program, 0x00, 0x2103);
    program.page(0xA5, 0x00);
    program.absolute(0x8D, 0x2104);
    program.page(0xA5, 0x01);
    program.absolute(0x8D, 0x2104);
    store(program, 0x00, 0x2104);
    store(program, 0x20, 0x2104);
    store(program, 0x11, 0x212C);
    store(program, 0x0F, 0x2100);
    store(program, 0x81, 0x4200);
    program.label("idle");
    program.jump("idle");

    program.label("nmi");
    program.byte(0x48);
    program.absolute(0xAD, 0x4219);
    program.immediate(0x29, 0x08);
    program.branch(0xF0, "skipUp");
    program.page(0xA5, 0x01);
    program.branch(0xF0, "skipUp");
    program.page(0xC6, 0x01);
    program.label("skipUp");
    program.absolute(0xAD, 0x4219);
    program.immediate(0x29, 0x04);
    program.branch(0xF0, "skipDown");
    program.page(0xA5, 0x01);
    program.immediate(0xC9, 216);
    program.branch(0xB0, "skipDown");
    program.page(0xE6, 0x01);
    program.label("skipDown");
    program.absolute(0xAD, 0x4219);
    program.immediate(0x29, 0x02);
    program.branch(0xF0, "skipLeft");
    program.page(0xA5, 0x00);
    program.branch(0xF0, "skipLeft");
    program.page(0xC6, 0x00);
    program.label("skipLeft");
    program.absolute(0xAD, 0x4219);
    program.immediate(0x29, 0x01);
    program.branch(0xF0, "skipRight");
    program.page(0xA5, 0x00);
    program.immediate(0xC9, 248);
    program.branch(0xB0, "skipRight");
    program.page(0xE6, 0x00);
    program.label("skipRight");
    store(program, 0x00, 0x2102);
    store(program, 0x00, 0x2103);
    program.page(0xA5, 0x00);
    program.absolute(0x8D, 0x2104);
    program.page(0xA5, 0x01);
    program.absolute(0x8D, 0x2104);
    store(program, 0x00, 0x2104);
    store(program, 0x20, 0x2104);
    program.byte(0x68);
    program.byte(0x40);

    std::vector<std::uint8_t> rom(32 * 1024, 0);
    const std::vector<std::uint8_t> code = program.finish(0x8000);
    for (std::size_t index = 0; index < code.size(); ++index) {
        rom[index] = code[index];
    }
    const int nmi = program.address("nmi", 0x8000);
    rom[0x7FFA] = static_cast<std::uint8_t>(nmi);
    rom[0x7FFB] = static_cast<std::uint8_t>(nmi >> 8);
    const std::string title = "IRIS SNES";
    for (std::size_t index = 0; index < title.size(); ++index) {
        rom[0x7FC0 + index] = static_cast<std::uint8_t>(title[index]);
    }
    rom[0x7FD5] = 0x20;
    rom[0x7FD6] = 0x00;
    rom[0x7FD7] = 0x05;
    rom[0x7FD8] = 0x00;
    rom[0x7FD9] = 0x01;
    rom[0x7FFC] = 0x00;
    rom[0x7FFD] = 0x80;
    std::uint32_t sum = 0;
    for (std::uint8_t byte : rom) {
        sum += byte;
    }
    const auto checksum = static_cast<std::uint16_t>(sum + 0x01FE);
    rom[0x7FDC] = static_cast<std::uint8_t>(checksum ^ 0xFF);
    rom[0x7FDD] = static_cast<std::uint8_t>((checksum >> 8) ^ 0xFF);
    rom[0x7FDE] = static_cast<std::uint8_t>(checksum);
    rom[0x7FDF] = static_cast<std::uint8_t>(checksum >> 8);
    return rom;
}

}  // namespace iris::systems::snes

