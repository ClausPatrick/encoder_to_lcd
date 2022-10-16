cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(flashprint C CXX ASM)
set(CMAKE_C STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})
pico_sdk_init()
add_executable(lcd_1604a
        lcd_1604a.c
    )
target_link_libraries(lcd_1604a  pico_stdlib hardware_dma)
pico_add_extra_outputs(lcd_1604a)
