#include "iris/nes/demo_rom.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace iris::systems::nes {
namespace {

enum class Mode { Implied, Accumulator, Immediate, ZeroPage, ZeroPageX, ZeroPageY, Absolute, AbsoluteX, AbsoluteY, Relative };

struct Encoding {
    const char* name;
    Mode mode;
    std::uint8_t opcode;
};

constexpr Encoding kEncodings[] = {
    {"sei", Mode::Implied, 0x78}, {"cld", Mode::Implied, 0xD8}, {"clc", Mode::Implied, 0x18},
    {"sec", Mode::Implied, 0x38}, {"nop", Mode::Implied, 0xEA}, {"inx", Mode::Implied, 0xE8},
    {"dex", Mode::Implied, 0xCA}, {"iny", Mode::Implied, 0xC8}, {"dey", Mode::Implied, 0x88},
    {"txs", Mode::Implied, 0x9A}, {"tax", Mode::Implied, 0xAA}, {"tay", Mode::Implied, 0xA8},
    {"txa", Mode::Implied, 0x8A}, {"tya", Mode::Implied, 0x98}, {"pha", Mode::Implied, 0x48},
    {"pla", Mode::Implied, 0x68}, {"rti", Mode::Implied, 0x40}, {"rts", Mode::Implied, 0x60},
    {"lsr", Mode::Accumulator, 0x4A},
    {"lda", Mode::Immediate, 0xA9}, {"ldx", Mode::Immediate, 0xA2}, {"ldy", Mode::Immediate, 0xA0},
    {"and", Mode::Immediate, 0x29}, {"cmp", Mode::Immediate, 0xC9},
    {"sta", Mode::ZeroPage, 0x85}, {"stx", Mode::ZeroPage, 0x86}, {"sty", Mode::ZeroPage, 0x84},
    {"lda", Mode::ZeroPage, 0xA5}, {"ldx", Mode::ZeroPage, 0xA6}, {"ldy", Mode::ZeroPage, 0xA4},
    {"inc", Mode::ZeroPage, 0xE6}, {"dec", Mode::ZeroPage, 0xC6}, {"rol", Mode::ZeroPage, 0x26},
    {"sta", Mode::ZeroPageX, 0x95}, {"lda", Mode::ZeroPageX, 0xB5}, {"sty", Mode::ZeroPageX, 0x94},
    {"stx", Mode::ZeroPageY, 0x96}, {"ldx", Mode::ZeroPageY, 0xB6},
    {"sta", Mode::Absolute, 0x8D}, {"stx", Mode::Absolute, 0x8E}, {"lda", Mode::Absolute, 0xAD},
    {"ldx", Mode::Absolute, 0xAE}, {"bit", Mode::Absolute, 0x2C}, {"jmp", Mode::Absolute, 0x4C},
    {"jsr", Mode::Absolute, 0x20}, {"inc", Mode::Absolute, 0xEE},
    {"sta", Mode::AbsoluteX, 0x9D}, {"lda", Mode::AbsoluteX, 0xBD},
    {"bpl", Mode::Relative, 0x10}, {"bmi", Mode::Relative, 0x30}, {"bvc", Mode::Relative, 0x50},
    {"bvs", Mode::Relative, 0x70}, {"bcc", Mode::Relative, 0x90}, {"bcs", Mode::Relative, 0xB0},
    {"bne", Mode::Relative, 0xD0}, {"beq", Mode::Relative, 0xF0},
};

std::string trim(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.pop_back();
    }
    return text;
}

std::string lower(std::string text) {
    for (char& character : text) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return text;
}

int parseNumber(const std::string& text) {
    if (text.size() > 1 && text[0] == '$') {
        return std::stoi(text.substr(1), nullptr, 16);
    }
    return std::stoi(text, nullptr, 10);
}

const Encoding* findEncoding(const std::string& name, Mode mode) {
    for (const Encoding& encoding : kEncodings) {
        if (name == encoding.name && mode == encoding.mode) {
            return &encoding;
        }
    }
    return nullptr;
}

std::vector<std::uint8_t> assemble(const char* source) {
    struct Fixup {
        std::size_t offset = 0;
        std::string label;
        bool relative = false;
        std::uint16_t nextPc = 0;
    };
    std::vector<std::uint8_t> prg(32 * 1024, 0);
    std::unordered_map<std::string, std::uint16_t> labels;
    std::vector<Fixup> fixups;
    std::uint16_t pc = 0x8000;
    int lineNumber = 0;

    const auto fail = [&](const std::string& message) {
        throw std::runtime_error("Iris demo line " + std::to_string(lineNumber) + ": " + message);
    };
    const auto emit = [&](std::uint8_t value) {
        if (pc < 0x8000) {
            fail("program counter left the PRG window");
        }
        const std::size_t index = static_cast<std::size_t>(pc) - 0x8000;
        if (index >= prg.size()) {
            fail("program counter left the PRG window");
        }
        prg[index] = value;
        pc = static_cast<std::uint16_t>(pc + 1);
    };

    std::string text = source;
    std::size_t cursor = 0;
    while (cursor <= text.size()) {
        const std::size_t end = text.find('\n', cursor);
        std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
        cursor = end == std::string::npos ? text.size() + 1 : end + 1;
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto comment = line.find(';');
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line.back() == ':') {
            const std::string label = lower(trim(line.substr(0, line.size() - 1)));
            if (labels.contains(label)) {
                fail("duplicate label " + label);
            }
            labels.emplace(label, pc);
            continue;
        }
        const auto space = line.find_first_of(" \t");
        const std::string mnemonic = lower(space == std::string::npos ? line : line.substr(0, space));
        std::string operand = space == std::string::npos ? std::string{} : lower(trim(line.substr(space)));
        operand.erase(std::remove_if(operand.begin(), operand.end(), [](unsigned char character) {
                          return std::isspace(character) != 0;
                      }),
                      operand.end());

        if (mnemonic == ".org") {
            pc = static_cast<std::uint16_t>(parseNumber(operand));
            continue;
        }
        if (mnemonic == ".dw") {
            if (!operand.empty() && operand[0] != '$' && std::isdigit(static_cast<unsigned char>(operand[0])) == 0) {
                fixups.push_back(Fixup{static_cast<std::size_t>(pc - 0x8000), operand, false, 0});
                emit(0);
                emit(0);
            } else {
                const int value = parseNumber(operand);
                emit(static_cast<std::uint8_t>(value & 0xFF));
                emit(static_cast<std::uint8_t>((value >> 8) & 0xFF));
            }
            continue;
        }

        Mode mode = Mode::Implied;
        std::string target;
        int number = 0;
        bool numeric = false;
        if (operand.empty()) {
            mode = Mode::Implied;
        } else if (operand == "a") {
            mode = Mode::Accumulator;
        } else if (operand[0] == '#') {
            mode = Mode::Immediate;
            number = parseNumber(operand.substr(1));
            numeric = true;
        } else {
            std::string body = operand;
            bool indexX = false;
            bool indexY = false;
            if (body.size() > 2 && body.ends_with(",x")) {
                indexX = true;
                body.resize(body.size() - 2);
            } else if (body.size() > 2 && body.ends_with(",y")) {
                indexY = true;
                body.resize(body.size() - 2);
            }
            const bool isNumber = !body.empty() && (body[0] == '$' || std::isdigit(static_cast<unsigned char>(body[0])) != 0);
            if (isNumber) {
                number = parseNumber(body);
                numeric = true;
                if (indexX) {
                    mode = number <= 0xFF ? Mode::ZeroPageX : Mode::AbsoluteX;
                } else if (indexY) {
                    mode = number <= 0xFF ? Mode::ZeroPageY : Mode::AbsoluteY;
                } else if (number <= 0xFF && findEncoding(mnemonic, Mode::ZeroPage) != nullptr) {
                    mode = Mode::ZeroPage;
                } else {
                    mode = Mode::Absolute;
                }
            } else {
                target = body;
                if (findEncoding(mnemonic, Mode::Relative) != nullptr) {
                    mode = Mode::Relative;
                } else if (indexX) {
                    mode = Mode::AbsoluteX;
                } else if (indexY) {
                    mode = Mode::AbsoluteY;
                } else {
                    mode = Mode::Absolute;
                }
            }
        }

        const Encoding* encoding = findEncoding(mnemonic, mode);
        if (encoding == nullptr && numeric && mode == Mode::ZeroPage) {
            mode = Mode::Absolute;
            encoding = findEncoding(mnemonic, mode);
        }
        if (encoding == nullptr) {
            fail("unsupported instruction '" + line + "'");
        }
        emit(encoding->opcode);
        if (mode == Mode::Immediate || mode == Mode::ZeroPage || mode == Mode::ZeroPageX || mode == Mode::ZeroPageY) {
            emit(static_cast<std::uint8_t>(number));
        } else if (mode == Mode::Absolute || mode == Mode::AbsoluteX || mode == Mode::AbsoluteY) {
            if (numeric) {
                emit(static_cast<std::uint8_t>(number & 0xFF));
                emit(static_cast<std::uint8_t>((number >> 8) & 0xFF));
            } else {
                fixups.push_back(Fixup{static_cast<std::size_t>(pc - 0x8000), target, false, 0});
                emit(0);
                emit(0);
            }
        } else if (mode == Mode::Relative) {
            fixups.push_back(Fixup{static_cast<std::size_t>(pc - 0x8000), target, true, static_cast<std::uint16_t>(pc + 1)});
            emit(0);
        }
    }

    for (const Fixup& fixup : fixups) {
        const auto found = labels.find(fixup.label);
        if (found == labels.end()) {
            throw std::runtime_error("Iris demo: unknown label " + fixup.label);
        }
        if (fixup.relative) {
            const int offset = static_cast<int>(found->second) - static_cast<int>(fixup.nextPc);
            if (offset < -128 || offset > 127) {
                throw std::runtime_error("Iris demo: branch to " + fixup.label + " is out of range");
            }
            prg[fixup.offset] = static_cast<std::uint8_t>(offset);
        } else {
            prg[fixup.offset] = static_cast<std::uint8_t>(found->second & 0xFF);
            prg[fixup.offset + 1] = static_cast<std::uint8_t>((found->second >> 8) & 0xFF);
        }
    }
    return prg;
}

void paintTile(std::vector<std::uint8_t>& chr, int index, const char* const rows[8], int color) {
    std::uint8_t* tile = chr.data() + static_cast<std::size_t>(index) * 16;
    for (int y = 0; y < 8; ++y) {
        std::uint8_t low = 0;
        std::uint8_t high = 0;
        for (int x = 0; x < 8; ++x) {
            if (rows[y][x] == '#') {
                const std::uint8_t bit = static_cast<std::uint8_t>(0x80 >> x);
                if ((color & 0x01) != 0) {
                    low = static_cast<std::uint8_t>(low | bit);
                }
                if ((color & 0x02) != 0) {
                    high = static_cast<std::uint8_t>(high | bit);
                }
            }
        }
        tile[y] = low;
        tile[y + 8] = high;
    }
}

const char* kProgram = R"(
.org $8000
reset:
    sei
    cld
    ldx #$ff
    txs
    inx
    stx $2000
    stx $2001
    stx $4010
    lda #$40
    sta $4017
wait_vblank_1:
    bit $2002
    bpl wait_vblank_1
    lda #$00
    tax
clear_ram:
    sta $00,x
    sta $0100,x
    sta $0200,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne clear_ram
wait_vblank_2:
    bit $2002
    bpl wait_vblank_2
    lda #$3f
    sta $2006
    lda #$00
    sta $2006
    lda #$02
    sta $2007
    lda #$30
    sta $2007
    lda #$12
    sta $2007
    lda #$16
    sta $2007
    lda #$3f
    sta $2006
    lda #$11
    sta $2006
    lda #$28
    sta $2007
    lda #$30
    sta $2007
    lda #$16
    sta $2007
    lda #$20
    sta $2006
    lda #$00
    sta $2006
    lda #$01
    ldx #$03
    ldy #$00
fill_pages:
    sta $2007
    iny
    bne fill_pages
    dex
    bne fill_pages
    ldy #$c0
fill_tail:
    sta $2007
    dey
    bne fill_tail
    lda #$23
    sta $2006
    lda #$c0
    sta $2006
    ldx #$40
    lda #$00
fill_attr:
    sta $2007
    dex
    bne fill_attr
    lda #$21
    sta $2006
    lda #$4c
    sta $2006
    lda #$04
    sta $2007
    lda #$05
    sta $2007
    lda #$04
    sta $2007
    lda #$06
    sta $2007
    lda #112
    sta $11
    lda #120
    sta $10
    jsr write_sprite
    lda #$00
    sta $2006
    sta $2006
    lda #$80
    sta $2000
    lda #$1e
    sta $2001
forever:
    jmp forever
write_sprite:
    lda #$00
    sta $2003
    lda $11
    sta $2004
    lda #$03
    sta $2004
    lda #$00
    sta $2004
    lda $10
    sta $2004
    rts
nmi:
    pha
    txa
    pha
    tya
    pha
    lda #$01
    sta $4016
    sta $12
    lsr a
    sta $4016
read_pad:
    lda $4016
    lsr a
    rol $12
    bcc read_pad
    lda $12
    and #$01
    beq skip_right
    inc $10
skip_right:
    lda $12
    and #$02
    beq skip_left
    dec $10
skip_left:
    lda $12
    and #$04
    beq skip_down
    inc $11
skip_down:
    lda $12
    and #$08
    beq skip_up
    dec $11
skip_up:
    jsr write_sprite
    lda $12
    and #$80
    beq note_off
    lda $13
    bne note_done
    lda #$01
    sta $13
    lda #$01
    sta $4015
    lda #$b7
    sta $4000
    lda #$00
    sta $4001
    lda #$fd
    sta $4002
    lda #$08
    sta $4003
    jmp note_done
note_off:
    lda #$00
    sta $13
    sta $4015
note_done:
    pla
    tay
    pla
    tax
    pla
    rti
irq:
    rti
.org $fffa
.dw nmi
.dw reset
.dw irq
)";

}  // namespace

std::vector<std::uint8_t> makeIrisDemoRom() {
    std::vector<std::uint8_t> rom(16 + 32 * 1024 + 8 * 1024, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 2;
    rom[5] = 1;
    const std::vector<std::uint8_t> prg = assemble(kProgram);
    std::copy(prg.begin(), prg.end(), rom.begin() + 16);

    std::vector<std::uint8_t> chr(8 * 1024, 0);
    const char* box[8] = {"########", "#......#", "#......#", "#......#", "#......#", "#......#", "#......#", "########"};
    const char* ball[8] = {"..####..", ".######.", "########", "########", "########", "########", ".######.", "..####.."};
    const char* letterI[8] = {".######.", "...##...", "...##...", "...##...", "...##...", "...##...", "...##...", ".######."};
    const char* letterR[8] = {".#####..", ".##..##.", ".##..##.", ".#####..", ".##.##..", ".##..##.", ".##..##.", ".##..##."};
    const char* letterS[8] = {".#####..", ".##..##.", ".##.....", ".#####..", ".....##.", ".##..##.", ".##..##.", ".#####.."};
    paintTile(chr, 1, box, 1);
    paintTile(chr, 3, ball, 1);
    paintTile(chr, 4, letterI, 1);
    paintTile(chr, 5, letterR, 1);
    paintTile(chr, 6, letterS, 1);
    std::copy(chr.begin(), chr.end(), rom.begin() + 16 + 32 * 1024);
    return rom;
}

}  // namespace iris::systems::nes
