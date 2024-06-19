cmake_minimum_required( VERSION 3.16)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_SDK_PATH "/home/dpoyesac/Projects/pico/pico-sdk")
include(pico_sdk_import.cmake)

# fix this
project(eeprom_25XX C CXX ASM)

pico_sdk_init()

# fix this
add_library(eeprom_25XX 
    25LC320A.c
    25LC320A.h
    )

# fix this
target_link_libraries(eeprom_25XX 
        pico_stdlib
        hardware_spi
        hardware_gpio
        )

# fix this
target_include_directories(eeprom_25XX PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
