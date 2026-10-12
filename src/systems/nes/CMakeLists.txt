add_library(iris_nes STATIC
    iris/nes/apu.cpp
    iris/nes/apu.hpp
    iris/nes/bus.cpp
    iris/nes/bus.hpp
    iris/nes/cartridge.cpp
    iris/nes/cartridge.hpp
    iris/nes/controller.cpp
    iris/nes/controller.hpp
    iris/nes/cpu.cpp
    iris/nes/cpu.hpp
    iris/nes/demo_rom.cpp
    iris/nes/demo_rom.hpp
    iris/nes/mapper.hpp
    iris/nes/nes_system.cpp
    iris/nes/nes_system.hpp
    iris/nes/nrom.cpp
    iris/nes/nrom.hpp
    iris/nes/palette.hpp
    iris/nes/ppu.cpp
    iris/nes/ppu.hpp
)

target_include_directories(iris_nes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_nes PUBLIC iris_common iris_emulator)
iris_set_warnings(iris_nes)

add_executable(iris_make_demo make_demo.cpp)
target_link_libraries(iris_make_demo PRIVATE iris_nes)
iris_set_warnings(iris_make_demo)
set_target_properties(iris_make_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
