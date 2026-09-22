cmake_minimum_required(VERSION 3.24)
project(digital_humans LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(catch2)

add_library(dh_coords STATIC
    src/coords.cpp
)
target_include_directories(dh_coords PUBLIC include)

add_executable(dh_coords_tests
    tests/coords_test.cpp
)
target_link_libraries(dh_coords_tests PRIVATE dh_coords Catch2::Catch2WithMain)

enable_testing()
add_test(NAME coords COMMAND dh_coords_tests)