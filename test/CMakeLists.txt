add_executable(decimal_test
  test_jarray.cpp
  test_mutable_bigint.cpp
)

target_compile_definitions(decimal_test PRIVATE
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(decimal_test PRIVATE
  decimal
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(decimal_test
  PROPERTIES ENVIRONMENT "MUTABLE_BIGINT_JAVA=1"
)