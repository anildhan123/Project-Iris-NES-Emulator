if(MSVC)
    # Debug otherwise compiles this core with /Od and /RTC1. A frame then takes
    # about 8 ms on AC power, and battery clocks miss the 16.6 ms NES frame.
    string(REGEX REPLACE "(^| )/RTC(1|su|s|u|c)" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "(^| )/Od" " /O2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "(^| )/Ob0" " /Ob2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

add_library(iris_nes STATIC
    iris/nes/apu.cpp
    iris/nes/boards.cpp
    iris/nes/boards.hpp
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
    iris/nes/mapper_factory.cpp
    iris/nes/mapper_factory.hpp
    iris/nes/mapper_pack.cpp
    iris/nes/mapper_pack.hpp
    iris/nes/mmc1.cpp
    iris/nes/mmc1.hpp
    iris/nes/mmc3.cpp
    iris/nes/mmc3.hpp
    iris/nes/nes_system.cpp
    iris/nes/nes_system.hpp
    iris/nes/nes_state.cpp
    iris/nes/nrom.cpp
    iris/nes/nrom.hpp
    iris/nes/timing.hpp
    iris/nes/uxrom.cpp
    iris/nes/uxrom.hpp
    iris/nes/cnrom.cpp
    iris/nes/cnrom.hpp
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
