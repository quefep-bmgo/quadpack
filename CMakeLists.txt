cmake_minimum_required(VERSION 3.28...4.4)

project(quadpack VERSION 0.1.0 LANGUAGES CXX)

option(QUADPACK_BUILD_TESTS "Build the test suite" ON)
option(QUADPACK_WERROR "Treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(quadpack_warnings INTERFACE)
if(MSVC)
    target_compile_options(quadpack_warnings INTERFACE /W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc)
else()
    target_compile_options(quadpack_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-qual -Wdouble-promotion -Wimplicit-fallthrough)
    if(QUADPACK_WERROR)
        target_compile_options(quadpack_warnings INTERFACE -Werror)
    endif()
endif()

if(QUADPACK_BUILD_TESTS)
    enable_testing()
    add_executable(quadpack_tests tests/test_greedy_2d.cpp src/greedy_2d.cpp)
    target_include_directories(quadpack_tests PRIVATE src)
    target_link_libraries(quadpack_tests PRIVATE quadpack_warnings)
    add_test(NAME greedy_2d COMMAND quadpack_tests)
endif()