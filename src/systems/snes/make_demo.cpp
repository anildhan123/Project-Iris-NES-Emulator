#include "iris/snes/demo_rom.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: iris_make_snes_demo output.sfc\n";
        return 1;
    }
    const std::vector<std::uint8_t> rom = iris::systems::snes::makeIrisSnesDemo();
    std::ofstream output(std::filesystem::path(argv[1]), std::ios::binary);
    if (!output) {
        std::cerr << "Could not write the Super NES demo.\n";
        return 1;
    }
    output.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
    return output ? 0 : 1;
}
