cmake_minimum_required(VERSION 3.22)
project(decimal)

set(CMAKE_CXX_STANDARD 20)

# Header-only library
add_library(decimal INTERFACE)
target_include_directories(decimal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Tests
enable_testing()
find_package(GTest REQUIRED)
add_subdirectory(test)