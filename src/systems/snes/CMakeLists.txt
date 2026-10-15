if(MSVC)
    # Debug otherwise compiles this core with /Od and /RTC1. A frame then takes
    # longer than the Super NES period once the machine is on battery, the same
    # failure the NES core avoids by compiling the interpreter with optimizations.
    string(REGEX REPLACE "(^| )/RTC(1|su|s|u|c)" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "(^| )/Od" " /O2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "(^| )/Ob0" " /Ob2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

add_library(iris_snes STATIC
    iris/snes/apu.cpp
    iris/snes/apu.hpp
    iris/snes/bus.cpp
    iris/snes/bus.hpp
    iris/snes/cartridge.cpp
    iris/snes/cartridge.hpp
    iris/snes/cpu.cpp
    iris/snes/cpu.hpp
    iris/snes/dsp1.cpp
    iris/snes/dsp1.hpp
    iris/snes/gsu.cpp
    iris/snes/gsu.hpp
    iris/snes/demo_rom.cpp
    iris/snes/demo_rom.hpp
    iris/snes/ppu.cpp
    iris/snes/ppu.hpp
    iris/snes/snes_system.cpp
    iris/snes/snes_system.hpp
    iris/snes/snes_state.cpp
)

target_include_directories(iris_snes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_snes PUBLIC iris_common iris_emulator)
iris_set_warnings(iris_snes)

add_executable(iris_make_snes_demo make_demo.cpp)
target_link_libraries(iris_make_snes_demo PRIVATE iris_snes)
iris_set_warnings(iris_make_snes_demo)
set_target_properties(iris_make_snes_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
