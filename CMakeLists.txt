cmake_minimum_required(VERSION 3.20)

project(VoX3D VERSION 0.3.7 LANGUAGES CXX)

option(VOX3D_FETCH_RAYLIB "Fetch raylib with CMake FetchContent when no system package is found" ON)
set(VOX3D_DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Persistent dependency cache directory")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(vox3d_core STATIC
    src/vox3d/chunk/chunk_grid.cpp
    src/vox3d/mesh/chunk_mesh_builder.cpp
    src/vox3d/mesh/face_visibility.cpp
    src/vox3d/voxel/voxel_world.cpp
    src/vox3d/map/map_package.cpp
    src/vox3d/map/runtime_map.cpp
)

target_compile_features(vox3d_core PUBLIC cxx_std_20)
target_include_directories(vox3d_core PUBLIC src)

if(MSVC)
    target_compile_options(vox3d_core PRIVATE /W4)
else()
    target_compile_options(vox3d_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(vox3d
    src/app.cpp
    src/config_loader.cpp
    src/confirm_dialog.cpp
    src/logger.cpp
    src/main.cpp
    src/menu.cpp
    src/process_metrics.cpp
    src/ui_draw.cpp
    src/ui_labels.cpp
    src/window_config.cpp
    src/workspace.cpp
)

target_compile_features(vox3d PRIVATE cxx_std_20)
target_include_directories(vox3d PRIVATE src)

target_compile_definitions(vox3d PRIVATE
    VOX3D_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(vox3d PRIVATE /W4)
else()
    target_compile_options(vox3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(raylib QUIET)

if(NOT raylib_FOUND)
    if(NOT VOX3D_FETCH_RAYLIB)
        message(FATAL_ERROR "raylib was not found. Install raylib or configure with -DVOX3D_FETCH_RAYLIB=ON.")
    endif()

    include(FetchContent)
    set(FETCHCONTENT_BASE_DIR "${VOX3D_DEPS_DIR}" CACHE PATH "FetchContent base directory" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

add_library(vox3d_raylib STATIC
    src/vox3d/render_raylib/chunk_mesh_preview.cpp
    src/vox3d/render_raylib/free_fly_camera.cpp
)

target_compile_features(vox3d_raylib PUBLIC cxx_std_20)
target_include_directories(vox3d_raylib PUBLIC src)
target_link_libraries(vox3d_raylib PUBLIC vox3d_core raylib)

if(MSVC)
    target_compile_options(vox3d_raylib PRIVATE /W4)
else()
    target_compile_options(vox3d_raylib PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(vox3d PRIVATE vox3d_core vox3d_raylib raylib)

if(UNIX AND NOT APPLE)
    target_link_libraries(vox3d PRIVATE pthread)
endif()
