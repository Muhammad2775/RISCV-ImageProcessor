cmake_minimum_required(VERSION 4.3.3)

project(RISCV_Project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Ofast -march=rv64g -mabi=lp64")
set(CXX_EXTENSIONS OFF)

# RISC-V executable
add_executable(app
    c/wrapper.c
    asm/invert.s
    asm/threshold.s
    asm/brightness.s
    asm/blur.s
)

target_include_directories(app PRIVATE c)

# Native bridge library
add_library(bridge SHARED c/bridge.c)

target_include_directories(bridge PRIVATE c)
