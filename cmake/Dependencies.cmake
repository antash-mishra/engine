# This file is the executable compatibility policy. Exact versions from the
# verification environment and vendored revisions are recorded separately in
# dependencies.lock.json.
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

function(engine_find_legacy_dependencies)
    find_package(assimp 5.2 REQUIRED)
    find_package(draco QUIET)
    if(NOT draco_FOUND)
        find_library(DRACO_LIBRARY NAMES draco)
    endif()
endfunction()
