cmake_minimum_required(VERSION 3.24)
project(digital_humans LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(catch2)

# BLAKE3's CMakeLists lives in c/, not at repo root.
FetchContent_Declare(blake3
    GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
    GIT_TAG        1.5.4
    SOURCE_SUBDIR  c
)
FetchContent_MakeAvailable(blake3)

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