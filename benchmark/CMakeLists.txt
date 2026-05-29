find_package(benchmark REQUIRED)

add_executable(mutable_bigint_bench bench_mutable_bigint.cpp)

target_link_libraries(mutable_bigint_bench PRIVATE decimal benchmark::benchmark)

target_compile_options(mutable_bigint_bench PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3>
)
