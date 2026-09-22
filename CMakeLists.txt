cmake_minimum_required(VERSION 3.24)
project(digital_humans LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(catch2)

# A helper to apply warnings only to our targets.
function(dh_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
    endif()
endfunction()

add_library(dh_coords STATIC
    src/coords.cpp
)
target_include_directories(dh_coords PUBLIC include)
dh_set_warnings(dh_coords)

add_executable(dh_coords_tests
    tests/coords_test.cpp
)
target_link_libraries(dh_coords_tests PRIVATE dh_coords Catch2::Catch2WithMain)
dh_set_warnings(dh_coords_tests)

enable_testing()
add_test(NAME coords COMMAND dh_coords_tests)