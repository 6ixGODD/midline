cmake_minimum_required(VERSION 3.10.2)
project(midline)

set(CMAKE_CXX_STANDARD 17)

# Static link MinGW runtime (fix missing DLL issue)
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

add_executable(midline WIN32 main.cpp)

target_link_libraries(midline
    gdiplus
    gdi32
    user32
)