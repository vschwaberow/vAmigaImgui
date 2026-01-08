cmake_minimum_required(VERSION 3.16)
project(vAmigaImgui)

set(CMAKE_CXX_STANDARD 20)

set(VAMIGA_CORE_DIR "" CACHE PATH "Path to vAmiga Core (defaults to external/vAmiga/Core)")
if(VAMIGA_CORE_DIR STREQUAL "")
    set(VAMIGA_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/vAmiga/Core")
endif()
if(NOT EXISTS "${VAMIGA_CORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "vAmiga Core not found. Initialize the submodule or set -DVAMIGA_CORE_DIR=/path/to/vAmiga/Core")
endif()

# Add the Core directory
add_subdirectory("${VAMIGA_CORE_DIR}" vAmigaCore)

# Add the src directory
add_subdirectory(src)
