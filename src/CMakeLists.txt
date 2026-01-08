cmake_minimum_required(VERSION 3.16)

project(vAmigaImgui)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED False)

add_compile_options(-Wno-error)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    ImGuiFileDialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog
    GIT_TAG        v0.6.8
)
FetchContent_MakeAvailable(ImGuiFileDialog)
target_include_directories(ImGuiFileDialog PUBLIC ${imgui_SOURCE_DIR})
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(ImGuiFileDialog PRIVATE -Wno-deprecated-enum-enum-conversion)
endif()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(vAmigaImgui
    main.cc
    application.cc
)
set_target_properties(vAmigaImgui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

target_link_libraries(vAmigaImgui PRIVATE
    VACore
    ${SDL2_LIBRARIES}
    ImGuiFileDialog
)

target_include_directories(vAmigaImgui PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${ImGuiFileDialog_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(vAmigaImgui PRIVATE
    components/console.cc
    components/dashboard.cc
    components/disk_creator.cc
    components/disk_inspector.cc
    components/volume_inspector.cc
    components/file_picker.cc
    components/input_manager.cc
    components/inspector.cc
    components/logic_analyzer.cc
    components/settings_window.cc
    components/video_window.cc
    components/virtual_keyboard.cc
    services/config_provider.cc
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_compile_definitions(vAmigaImgui PRIVATE
    USE_STD_FILESYSTEM
)

if(NOT APPLE)
    target_compile_definitions(vAmigaImgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GL3W)
endif()

if(APPLE)
    target_link_libraries(vAmigaImgui PRIVATE "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo")
else()
    target_link_libraries(vAmigaImgui PRIVATE GL dl)
endif()

add_executable(vAmigaTests
    tests/smoke_test.cc
    tests/config_provider_test.cc
    services/config_provider.cc
)
set_target_properties(vAmigaTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(vAmigaTests PRIVATE
    gtest_main
    VACore
)
target_include_directories(vAmigaTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME vAmigaTests COMMAND vAmigaTests)
