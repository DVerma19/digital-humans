cmake_minimum_required(VERSION 3.24)
project(digital_humans LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- Catch2 ---
FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(catch2)

# --- BLAKE3 ---
FetchContent_Declare(blake3
    GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
    GIT_TAG        1.5.4
    SOURCE_SUBDIR  c
)
FetchContent_MakeAvailable(blake3)

# --- SDL3 (static) ---
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# --- vk-bootstrap ---
FetchContent_Declare(vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.296
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vk-bootstrap)

find_package(Vulkan REQUIRED)

# --- common warnings ---
function(dh_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -ffp-contract=off
        )
    endif()
endfunction()

# --- coords ---
add_library(dh_coords STATIC src/coords.cpp)
target_include_directories(dh_coords PUBLIC include)
dh_set_warnings(dh_coords)

# --- hash ---
add_library(dh_hash STATIC src/hash.cpp)
target_include_directories(dh_hash PUBLIC include)
target_include_directories(dh_hash PRIVATE ${blake3_SOURCE_DIR}/c)
target_link_libraries(dh_hash PRIVATE blake3)
dh_set_warnings(dh_hash)

# --- rng ---
add_library(dh_rng STATIC src/rng.cpp)
target_include_directories(dh_rng PUBLIC include)
target_link_libraries(dh_rng PRIVATE dh_hash)
dh_set_warnings(dh_rng)

# --- noise ---
add_library(dh_noise STATIC src/noise.cpp)
target_include_directories(dh_noise PUBLIC include)
target_link_libraries(dh_noise PRIVATE dh_hash)
dh_set_warnings(dh_noise)

# --- chunk ---
add_library(dh_chunk STATIC src/chunk.cpp)
target_include_directories(dh_chunk PUBLIC include)
target_link_libraries(dh_chunk PRIVATE dh_coords dh_hash dh_noise)
dh_set_warnings(dh_chunk)

# --- chunk_manager ---
add_library(dh_chunk_manager STATIC src/chunk_manager.cpp)
target_include_directories(dh_chunk_manager PUBLIC include)
target_link_libraries(dh_chunk_manager PRIVATE dh_chunk)
dh_set_warnings(dh_chunk_manager)

# --- img_dump ---
add_library(dh_img_dump STATIC src/img_dump.cpp)
target_include_directories(dh_img_dump PUBLIC include)
target_link_libraries(dh_img_dump PRIVATE dh_chunk)
dh_set_warnings(dh_img_dump)

# --- dump tool ---
add_executable(dh_dump src/dump_main.cpp)
target_link_libraries(dh_dump PRIVATE dh_img_dump)
dh_set_warnings(dh_dump)

# --- shader loader ---
add_library(dh_vk_shader STATIC src/vk/shader.cpp)
target_include_directories(dh_vk_shader PUBLIC include)
target_link_libraries(dh_vk_shader PUBLIC Vulkan::Vulkan)
target_include_directories(dh_vk_shader SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
dh_set_warnings(dh_vk_shader)

# --- shader compilation ---
set(DH_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(DH_SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${DH_SHADER_BIN_DIR})

set(DH_SHADER_OUTPUTS
    ${DH_SHADER_BIN_DIR}/triangle.vert.spv
    ${DH_SHADER_BIN_DIR}/triangle.frag.spv
    ${DH_SHADER_BIN_DIR}/terrain.vert.spv
    ${DH_SHADER_BIN_DIR}/terrain.frag.spv
)

add_custom_command(
    OUTPUT ${DH_SHADER_OUTPUTS}
    COMMAND $<TARGET_FILE:Vulkan::glslc>
            -o ${DH_SHADER_BIN_DIR}/triangle.vert.spv
            ${DH_SHADER_SRC_DIR}/triangle.vert
    COMMAND $<TARGET_FILE:Vulkan::glslc>
            -o ${DH_SHADER_BIN_DIR}/triangle.frag.spv
            ${DH_SHADER_SRC_DIR}/triangle.frag
    COMMAND $<TARGET_FILE:Vulkan::glslc>
            -o ${DH_SHADER_BIN_DIR}/terrain.vert.spv
            ${DH_SHADER_SRC_DIR}/terrain.vert
    COMMAND $<TARGET_FILE:Vulkan::glslc>
            -o ${DH_SHADER_BIN_DIR}/terrain.frag.spv
            ${DH_SHADER_SRC_DIR}/terrain.frag
    DEPENDS
        ${DH_SHADER_SRC_DIR}/triangle.vert
        ${DH_SHADER_SRC_DIR}/triangle.frag
        ${DH_SHADER_SRC_DIR}/terrain.vert
        ${DH_SHADER_SRC_DIR}/terrain.frag
    COMMENT "Compiling shaders"
)

add_custom_target(dh_shaders ALL DEPENDS ${DH_SHADER_OUTPUTS})


# --- tests ---
enable_testing()

add_executable(dh_coords_tests tests/coords_test.cpp)
target_link_libraries(dh_coords_tests PRIVATE dh_coords Catch2::Catch2WithMain)
dh_set_warnings(dh_coords_tests)
add_test(NAME coords COMMAND dh_coords_tests)

add_executable(dh_hash_tests tests/hash_test.cpp)
target_link_libraries(dh_hash_tests PRIVATE dh_hash Catch2::Catch2WithMain)
dh_set_warnings(dh_hash_tests)
add_test(NAME hash COMMAND dh_hash_tests)

add_executable(dh_rng_tests tests/rng_test.cpp)
target_link_libraries(dh_rng_tests PRIVATE dh_rng Catch2::Catch2WithMain)
dh_set_warnings(dh_rng_tests)
add_test(NAME rng COMMAND dh_rng_tests)

add_executable(dh_noise_tests tests/noise_test.cpp)
target_link_libraries(dh_noise_tests PRIVATE dh_noise Catch2::Catch2WithMain)
dh_set_warnings(dh_noise_tests)
add_test(NAME noise COMMAND dh_noise_tests)

add_executable(dh_chunk_tests tests/chunk_test.cpp)
target_link_libraries(dh_chunk_tests PRIVATE dh_chunk Catch2::Catch2WithMain)
dh_set_warnings(dh_chunk_tests)
add_test(NAME chunk COMMAND dh_chunk_tests)

add_executable(dh_chunk_manager_tests tests/chunk_manager_test.cpp)
target_link_libraries(dh_chunk_manager_tests PRIVATE dh_chunk_manager Catch2::Catch2WithMain)
dh_set_warnings(dh_chunk_manager_tests)
add_test(NAME chunk_manager COMMAND dh_chunk_manager_tests)

# --- Vulkan G0 smoke test ---
add_executable(dh_vk src/vk_main.cpp)
target_link_libraries(dh_vk PRIVATE
    SDL3::SDL3-static
    vk-bootstrap::vk-bootstrap
    Vulkan::Vulkan
)
target_include_directories(dh_vk SYSTEM PRIVATE
    ${vk-bootstrap_SOURCE_DIR}/src
)
dh_set_warnings(dh_vk)

# --- Vulkan G1 triangle ---
add_executable(dh_vk_triangle src/vk_triangle.cpp)
target_link_libraries(dh_vk_triangle PRIVATE
    SDL3::SDL3-static
    vk-bootstrap::vk-bootstrap
    Vulkan::Vulkan
    dh_vk_shader
)
target_include_directories(dh_vk_triangle SYSTEM PRIVATE
    ${vk-bootstrap_SOURCE_DIR}/src
)
target_compile_definitions(dh_vk_triangle PRIVATE
    DH_SHADER_DIR="${DH_SHADER_BIN_DIR}"
)
add_dependencies(dh_vk_triangle dh_shaders)
dh_set_warnings(dh_vk_triangle)

# --- Vulkan G2a terrain ---
add_executable(dh_vk_terrain src/vk_terrain.cpp)
target_link_libraries(dh_vk_terrain PRIVATE
    SDL3::SDL3-static
    vk-bootstrap::vk-bootstrap
    Vulkan::Vulkan
    dh_vk_shader
    dh_chunk
    dh_coords
)
target_include_directories(dh_vk_terrain SYSTEM PRIVATE
    ${vk-bootstrap_SOURCE_DIR}/src
)
target_compile_definitions(dh_vk_terrain PRIVATE
    DH_SHADER_DIR="${DH_SHADER_BIN_DIR}"
)
add_dependencies(dh_vk_terrain dh_shaders)
dh_set_warnings(dh_vk_terrain)