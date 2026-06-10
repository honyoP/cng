cmake_minimum_required(VERSION 3.20)

project(cng_worldgen_v0_1 LANGUAGES CXX)

find_package(SDL3 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(vendor/json)

set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL)

add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    vendor/imgui
    vendor/imgui/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 OpenGL::GL)

add_library(cng_game
    src/buildings/BuildingBlueprint.cpp
    src/data/GameData.cpp
    src/entities/Entity.cpp
    src/renderer/Camera.cpp
    src/renderer/ChunkMesh.cpp
    src/systems/Movement.cpp
    src/world/World.cpp
    src/world/WorldGen.cpp
)
target_compile_features(cng_game PUBLIC cxx_std_23)
target_include_directories(cng_game PUBLIC src)
target_link_libraries(cng_game PUBLIC nlohmann_json::nlohmann_json)

add_executable(cng
    src/main.cpp
    src/app/Application.cpp
    src/editor/BuildingEditor.cpp
    src/renderer/Renderer.cpp
)

target_compile_features(cng PRIVATE cxx_std_23)
target_include_directories(cng PRIVATE src)
target_link_libraries(cng PRIVATE cng_game imgui SDL3::SDL3 OpenGL::GL)
target_compile_definitions(cng PRIVATE CNG_DATA_ROOT="${CMAKE_SOURCE_DIR}/data")

include(CTest)
if(BUILD_TESTING)
    add_executable(cng_tests tests/WorldgenTests.cpp)
    target_link_libraries(cng_tests PRIVATE cng_game)
    add_test(NAME worldgen COMMAND cng_tests)
endif()

if(MSVC)
    target_compile_options(cng_game PRIVATE /W4 /permissive-)
    target_compile_options(cng PRIVATE /W4 /permissive-)
else()
    target_compile_options(cng_game PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cng PRIVATE -Wall -Wextra -Wpedantic)
endif()
