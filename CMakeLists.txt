cmake_minimum_required(VERSION 3.20)
project(pbl2_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_executable(pbl2_tests
    tests/test_data_structures.cpp
)

target_include_directories(pbl2_tests PRIVATE . include)

if (MSVC)
    target_compile_options(pbl2_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(pbl2_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME pbl2_data_structures COMMAND pbl2_tests)