cmake_minimum_required(VERSION 3.10)
project(midi_player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局定义 NOMINMAX，避免 windows.h 的 min/max 宏
add_definitions(-DNOMINMAX)

include_directories(include)

add_executable(midi_player
    src/main.cpp
    src/MidiParser.cpp
    src/EventScheduler.cpp
    src/Player.cpp
    src/PianoRollRenderer.cpp
)

target_link_libraries(midi_player winmm)