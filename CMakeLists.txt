cmake_minimum_required(VERSION 4.3.2)
project(COAL_Semester_Project C)

set(CMAKE_C_STANDARD 11)

# If you are using a cross toolchain file, CLion will provide the compiler.
# These are the sources for the RISC-V executable.
set(SRC
    c/wrapper.c
    asm/invert.s
    asm/threshold.s
    asm/brightness.s
    asm/blur.s
)

add_executable(app ${SRC})

target_include_directories(app PRIVATE c)