cmake_minimum_required( VERSION 3.16)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_SDK_PATH "/home/dpoyesac/Projects/pico/pico-sdk")
include(pico_sdk_import.cmake)

# fix this

project(ntp_request C CXX ASM)

pico_sdk_init()

# fix this
add_library(ntp_request
	ntp_request.c
	ntp_request.h
    )

# fix this
target_link_libraries(ntp_request
        pico_stdlib
        hardware_timer
        hardware_rtc
        pico_cyw43_arch_lwip_threadsafe_background
        )

# fix this
target_include_directories(ntp_request PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
