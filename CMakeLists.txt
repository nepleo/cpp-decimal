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

# Benchmarks (Google Benchmark); optional: -DBUILD_BENCHMARKS=OFF
option(BUILD_BENCHMARKS "Build mutable_bigint Google Benchmark targets" ON)
if(BUILD_BENCHMARKS)
  find_package(benchmark QUIET)
  if(benchmark_FOUND)
    add_subdirectory(benchmark)
  else()
    message(WARNING "google-benchmark not found; skip benchmark targets (e.g. brew install google-benchmark)")
  endif()
endif()