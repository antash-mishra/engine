cmake_minimum_required(VERSION 3.10)
project(game_engine VERSION 0.1.0)

add_executable(game_engine src/main.cpp src/glad.c)

target_include_directories(game_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(game_engine glfw)