cmake_minimum_required(VERSION 3.10)
project(game_engine VERSION 0.1.0)

# Find Assimp package
find_package(assimp REQUIRED)

# Source files
add_executable(game_engine 
    src/main.cpp 
    src/glad.c 
    src/shader.cpp
    src/stb_image.cpp
    src/camera.cpp
    src/texture.cpp
    src/model.cpp
    src/mesh.cpp
)

target_include_directories(game_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Copy resources directory to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Find and link libraries
target_link_libraries(game_engine 
    glfw
    assimp::assimp  # Link Assimp using the target provided by find_package
)
