#include "iris/nes/demo_rom.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: iris_make_demo output.nes\n";
        return 1;
    }
    try {
        const auto rom = iris::systems::nes::makeIrisDemoRom();
        std::ofstream output(argv[1], std::ios::binary);
        if (!output) {
            std::cerr << "Could not write the demo ROM.\n";
            return 1;
        }
        output.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
        if (!output) {
            std::cerr << "Could not finish writing the demo ROM.\n";
            return 1;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
