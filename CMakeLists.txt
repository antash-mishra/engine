cmake_minimum_required(VERSION 3.23)
project(mini_engine VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)

option(
    ENGINE_BUILD_LEGACY_SAMPLES
    "Build the Phase 0 baseline executables while samples migrate to render_samples"
    OFF
)
option(
    ENGINE_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer for maintained targets"
    OFF
)

# Header-only GLM is vendored under include/. Giving it a target keeps that
# dependency explicit instead of relying on incidental include search paths.
add_library(engine_glm INTERFACE)
target_include_directories(engine_glm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(engine_warnings INTERFACE)
target_compile_options(
    engine_warnings
    INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall;-Wextra;-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wconversion>
)

add_library(engine_sanitizers INTERFACE)
if(ENGINE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            engine_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(
            engine_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer
        )
    else()
        message(FATAL_ERROR "ENGINE_ENABLE_SANITIZERS supports GNU and Clang only")
    endif()
endif()

# Portable scene state has no platform or graphics dependency. Render code consumes
# these value types without taking ownership of a scene.
add_library(engine_scene STATIC
    src/engine/scene/camera_state.cpp
)
target_include_directories(engine_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
    engine_scene
    PUBLIC engine_glm
    PRIVATE engine_warnings engine_sanitizers
)

# Asset discovery is deliberately independent of the process working directory.
# It resolves an explicit root and validates all paths before a sample opens files.
add_library(engine_assets STATIC
    src/engine/assets/asset_root.cpp
)
target_include_directories(engine_assets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(engine_assets PRIVATE engine_warnings engine_sanitizers)

# Platform owns GLFW's global lifetime and the native window/context. It does not
# contain renderer or sample behavior.
add_library(engine_platform STATIC
    src/engine/platform/window.cpp
)
target_include_directories(engine_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
    engine_platform
    PUBLIC glfw
    PRIVATE engine_warnings engine_sanitizers
)

# Render owns the one GLAD implementation used by the new target graph and the
# context-bound OpenGL primitives shared by samples.
# GLAD is generated and checksum-locked. Its required object-to-function pointer
# casts are intentionally excluded from project warning policy.
set_source_files_properties(
    src/glad.c
    PROPERTIES COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-w>"
)
add_library(engine_render STATIC
    src/glad.c
    src/engine/render/opengl.cpp
    src/engine/render/fullscreen_pass.cpp
)
target_include_directories(engine_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
    engine_render
    PUBLIC engine_glm engine_platform engine_scene
    PRIVATE OpenGL::GL ${CMAKE_DL_LIBS} engine_warnings engine_sanitizers
)

# Sample registration is a separate layer so adding a sample does not add another
# executable or another copy of the engine lifecycle.
add_library(engine_samples STATIC
    src/engine/sample/sample_registry.cpp
    src/samples/ray_marching_sample.cpp
)
target_include_directories(engine_samples PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
    engine_samples
    PUBLIC engine_assets engine_platform engine_render engine_scene
    PRIVATE engine_warnings engine_sanitizers
)

add_executable(render_samples
    src/apps/render_samples_main.cpp
    src/apps/launcher_cli.cpp
)
target_include_directories(render_samples PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
    render_samples
    PRIVATE engine_samples engine_warnings engine_sanitizers
)

# Only the two small shaders needed by the registered sample are staged beside the
# launcher. The external multi-gigabyte Sponza archive is never copied to a build.
set(ENGINE_LAUNCHER_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders")
file(MAKE_DIRECTORY "${ENGINE_LAUNCHER_ASSET_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/vertexCube.glsl"
    "${ENGINE_LAUNCHER_ASSET_DIR}/vertexCube.glsl"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/fragmentCube.glsl"
    "${ENGINE_LAUNCHER_ASSET_DIR}/fragmentCube.glsl"
    COPYONLY
)

# Phase 0 compatibility targets remain isolated while their behavior and benchmark
# schema are used to verify later migrations.
if(ENGINE_BUILD_LEGACY_SAMPLES)
    engine_find_legacy_dependencies()

    add_executable(game_engine
        src/ray_marching.cpp
        src/glad.c
        src/shader.cpp
        src/stb_image.cpp
        src/camera.cpp
        src/legacy_baseline.cpp
    )
    target_include_directories(game_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(
        game_engine
        PRIVATE glfw assimp::assimp ${CMAKE_DL_LIBS}
    )
    if(draco_FOUND)
        target_link_libraries(game_engine PRIVATE draco)
    elseif(DRACO_LIBRARY)
        target_link_libraries(game_engine PRIVATE "${DRACO_LIBRARY}")
    endif()

    add_executable(legacy_sponza
        src/sponza.cpp
        src/glad.c
        src/shader.cpp
        src/stb_image.cpp
        src/camera.cpp
        src/legacy_baseline.cpp
    )
    target_include_directories(legacy_sponza PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(legacy_sponza PRIVATE glfw ${CMAKE_DL_LIBS})
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_executable(engine_sample_registry_tests
        tests/unit/sample_registry_tests.cpp
    )
    target_link_libraries(
        engine_sample_registry_tests
        PRIVATE engine_samples engine_warnings engine_sanitizers
    )
    add_test(NAME sample_registry_unit COMMAND engine_sample_registry_tests)
    set_tests_properties(
        sample_registry_unit
        PROPERTIES LABELS "unit;samples"
    )

    add_test(
        NAME launcher_cli
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher/test_render_samples_cli.py
            --executable $<TARGET_FILE:render_samples>
            --build-dir ${CMAKE_CURRENT_BINARY_DIR}
            --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties(
        launcher_cli
        PROPERTIES
            LABELS "launcher;negative"
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
            TIMEOUT 120
    )

    add_test(
        NAME baseline_schema
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmarks
                -p test_*.py
                -v
    )
    set_tests_properties(
        baseline_schema
        PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME asset_tool_unit
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/tests/assets
                -p test_*.py
                -v
    )
    set_tests_properties(
        asset_tool_unit
        PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME fixture_gltf
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/manage_assets.py
                verify-gltf
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/gltf/phase0_scene.gltf
                --expect ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/gltf/phase0_scene.expected.json
    )
    set_tests_properties(
        fixture_gltf
        PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME dependency_lock
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/build/verify_dependencies.py
            verify
            --source-root ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties(
        dependency_lock
        PROPERTIES
            LABELS "build;dependencies"
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME dependency_lock_unit
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/tests/build
                -p test_*.py
                -v
    )
    set_tests_properties(
        dependency_lock_unit
        PROPERTIES
            LABELS "build;dependencies;unit"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    set(
        ENGINE_SPONZA_ASSET_ROOT
        ""
        CACHE PATH
        "Opt-in Intel Sponza root containing main_sponza/NewSponza_Main_glTF_003.gltf"
    )
    if(
        ENGINE_SPONZA_ASSET_ROOT
        AND EXISTS
            "${ENGINE_SPONZA_ASSET_ROOT}/main_sponza/NewSponza_Main_glTF_003.gltf"
    )
        add_test(
            NAME asset_manifest
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/manage_assets.py
                    --manifest ${CMAKE_CURRENT_SOURCE_DIR}/assets/manifest.json
                    verify
                    --asset intel-sponza-base
                    --root ${ENGINE_SPONZA_ASSET_ROOT}
        )
        set_tests_properties(
            asset_manifest
            PROPERTIES
                LABELS "integration;asset"
                ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
        )
    endif()
endif()
