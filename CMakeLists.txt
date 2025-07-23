cmake_minimum_required(VERSION 3.10)
project(game_engine VERSION 0.1.0)

# Find Assimp package
find_package(assimp REQUIRED)

# Find draco package (optional, for glTF support)
find_package(draco QUIET)

# ImGui source files
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Source files
add_executable(game_engine 
    src/scene.cpp
    src/glad.c 
    src/shader.cpp
    src/stb_image.cpp
    src/camera.cpp
)

target_include_directories(game_engine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Copy resources directory to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Find and link libraries
target_link_libraries(game_engine 
    glfw
    assimp::assimp  # Link Assimp using the target provided by find_package
)

# Link draco if found, otherwise try system library
if(draco_FOUND)
    target_link_libraries(game_engine draco)
else()
    # Try to link system draco library
    find_library(DRACO_LIB draco)
    if(DRACO_LIB)
        target_link_libraries(game_engine ${DRACO_LIB})
    endif()
endif()
