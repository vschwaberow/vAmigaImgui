cmake_minimum_required(VERSION 3.16)
project(vAmigaLinux)

set(CMAKE_CXX_STANDARD 20)

# Add the Core directory
add_subdirectory(external/vAmiga/Core)

# Add the LinuxGUI directory
add_subdirectory(LinuxGUI)
