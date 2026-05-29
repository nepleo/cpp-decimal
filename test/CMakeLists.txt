add_executable(decimal_test
  test_jarray.cpp
)

target_link_libraries(decimal_test PRIVATE
  decimal
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(decimal_test)