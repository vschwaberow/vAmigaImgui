cmake_minimum_required(VERSION 3.16)
project(vAmigaImgui)

set(CMAKE_CXX_STANDARD 20)

# Add the Core directory
add_subdirectory(external/vAmiga/Core)

# Add the src directory
add_subdirectory(src)
