cmake_minimum_required(VERSION 3.16)

project(vAmigaImgui)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED False)

add_compile_options(-Wno-error)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

option(VAMIGAIMGUUI_FETCHCONTENT "Allow downloading dependencies via FetchContent" ON)
option(ENABLE_TESTS "Build tests" ON)

set(IMGUI_DIR "" CACHE PATH "Path to ImGui source (root contains imgui.h)")

include(FetchContent)
if(IMGUI_DIR STREQUAL "")
    if(NOT VAMIGAIMGUUI_FETCHCONTENT)
        message(FATAL_ERROR "ImGui not found. Set -DIMGUI_DIR=/path/to/imgui or enable FetchContent.")
    endif()
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG        docking
    )
    FetchContent_MakeAvailable(imgui)
else()
    set(imgui_SOURCE_DIR "${IMGUI_DIR}")
endif()
if(NOT EXISTS "${imgui_SOURCE_DIR}/imgui.h")
    message(FATAL_ERROR "ImGui not found at ${imgui_SOURCE_DIR}. Set -DIMGUI_DIR=/path/to/imgui.")
endif()

set(IMGUIFILEDIALOG_DIR "" CACHE PATH "Path to ImGuiFileDialog source")
if(IMGUIFILEDIALOG_DIR STREQUAL "")
    if(NOT VAMIGAIMGUUI_FETCHCONTENT)
        message(FATAL_ERROR "ImGuiFileDialog not found. Set -DIMGUIFILEDIALOG_DIR=/path/to/ImGuiFileDialog or enable FetchContent.")
    endif()
    FetchContent_Declare(
        ImGuiFileDialog
        GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog
        GIT_TAG        v0.6.8
    )
    FetchContent_MakeAvailable(ImGuiFileDialog)
else()
    if(NOT EXISTS "${IMGUIFILEDIALOG_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "ImGuiFileDialog not found at ${IMGUIFILEDIALOG_DIR}. Set -DIMGUIFILEDIALOG_DIR=/path/to/ImGuiFileDialog.")
    endif()
    add_subdirectory("${IMGUIFILEDIALOG_DIR}" ImGuiFileDialog)
endif()
target_include_directories(ImGuiFileDialog PUBLIC ${imgui_SOURCE_DIR})
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(ImGuiFileDialog PRIVATE -Wno-deprecated-enum-enum-conversion)
endif()

if(ENABLE_TESTS)
    set(GTEST_DIR "" CACHE PATH "Path to googletest source")
    if(GTEST_DIR STREQUAL "")
        if(NOT VAMIGAIMGUUI_FETCHCONTENT)
            message(FATAL_ERROR "googletest not found. Set -DGTEST_DIR=/path/to/googletest or enable FetchContent, or disable tests with -DENABLE_TESTS=OFF.")
        endif()
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    else()
        if(NOT EXISTS "${GTEST_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "googletest not found at ${GTEST_DIR}. Set -DGTEST_DIR=/path/to/googletest.")
        endif()
        add_subdirectory("${GTEST_DIR}" googletest)
    endif()
    enable_testing()
endif()

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
    components/hard_disk_creator.cc
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

if(ENABLE_TESTS)
    add_executable(vAmigaTests
        tests/smoke_test.cc
        tests/config_provider_test.cc
        tests/hard_disk_creator_test.cc
        services/config_provider.cc
        components/hard_disk_creator.cc
        components/file_picker.cc
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    )
    set_target_properties(vAmigaTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    target_link_libraries(vAmigaTests PRIVATE
        gtest_main
        VACore
        ImGuiFileDialog
    )
    target_include_directories(vAmigaTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${ImGuiFileDialog_SOURCE_DIR}
    )
    add_test(NAME vAmigaTests COMMAND vAmigaTests)
endif()
