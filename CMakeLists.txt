cmake_minimum_required(VERSION 3.10.2)
project(midline)

set(CMAKE_CXX_STANDARD 17)

add_executable(midline WIN32 main.cpp)

target_link_libraries(midline
    gdiplus
    gdi32
    user32
)