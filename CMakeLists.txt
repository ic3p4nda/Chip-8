cmake_minimum_required(VERSION 3.16)
project(chip8 CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 CONFIG REQUIRED)

add_executable(chip8 src/main.cpp src/chip8.cpp
        src/chip8.h)
target_link_libraries(chip8 PRIVATE SDL2::SDL2 SDL2::SDL2main)