cmake_minimum_required(VERSION 3.20)
project(sdchord_dsp CXX)

# Native build of the DSP tests. The plugin is built from the root CMakeLists.txt.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dsp_tests tests/dsp_tests.cpp)
target_include_directories(dsp_tests PRIVATE include)

if(MSVC)
    target_compile_options(dsp_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(dsp_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME dsp_tests COMMAND dsp_tests)
