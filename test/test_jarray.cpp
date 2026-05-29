#include <gtest/gtest.h>

#include "bigdecimal.h"

TEST(JArrayTest, DefaultConstructor) {
  jarray<int> arr;
  EXPECT_TRUE(arr.empty());
  EXPECT_EQ(arr.length(), 0);
  EXPECT_EQ(arr.data(), nullptr);
}

TEST(JArrayTest, LengthConstructor) {
  jarray<int> arr(5);
  EXPECT_FALSE(arr.empty());
  EXPECT_EQ(arr.length(), 5);
  for (int i = 0; i < arr.length(); i++) {
    EXPECT_EQ(arr[i], 0);
  }
}

TEST(JArrayTest, ZeroLengthConstructor) {
  jarray<int> arr(0);
  EXPECT_TRUE(arr.empty());
  EXPECT_EQ(arr.length(), 0);
  EXPECT_EQ(arr.data(), nullptr);
}

TEST(JArrayTest, InitializerList) {
  jarray<int> arr = {1, 2, 3, 4, 5};
  EXPECT_EQ(arr.length(), 5);
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(arr[i], i + 1);
  }
}

TEST(JArrayTest, EmptyInitializerList) {
  jarray<int> arr = {};
  EXPECT_TRUE(arr.empty());
}

TEST(JArrayTest, CopyConstructor) {
  jarray<int> a = {10, 20, 30};
  jarray<int> b(a);

  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 10);
  EXPECT_EQ(b[1], 20);
  EXPECT_EQ(b[2], 30);

  b[0] = 99;
  EXPECT_EQ(a[0], 10);  // deep copy
}

TEST(JArrayTest, CopyAssignment) {
  jarray<int> a = {1, 2, 3};
  jarray<int> b = {4, 5};
  b = a;

  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 1);
  EXPECT_EQ(b[1], 2);
  EXPECT_EQ(b[2], 3);
}

TEST(JArrayTest, SelfCopyAssignment) {
  jarray<int> a = {1, 2, 3};
  a = a;
  EXPECT_EQ(a.length(), 3);
  EXPECT_EQ(a[0], 1);
}

TEST(JArrayTest, MoveConstructor) {
  jarray<int> a = {10, 20, 30};
  jarray<int> b(std::move(a));

  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 10);
  EXPECT_EQ(a.empty(), true);
}

TEST(JArrayTest, MoveAssignment) {
  jarray<int> a = {1, 2, 3};
  jarray<int> b = {4};
  b = std::move(a);

  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(a.empty(), true);
}

TEST(JArrayTest, SelfMoveAssignment) {
  jarray<int> a = {1, 2, 3};
  a = std::move(a);
  EXPECT_EQ(a.length(), 3);
}

TEST(JArrayTest, IndexOperator) {
  jarray<int> arr(3);
  arr[0] = 10;
  arr[1] = 20;
  arr[2] = 30;

  EXPECT_EQ(arr[0], 10);
  EXPECT_EQ(arr[1], 20);
  EXPECT_EQ(arr[2], 30);

  const auto& carr = arr;
  EXPECT_EQ(carr[0], 10);
}

TEST(JArrayTest, Data) {
  jarray<int> arr = {1, 2, 3};
  int* p = arr.data();
  EXPECT_EQ(p[0], 1);
  EXPECT_EQ(p[1], 2);

  const auto& carr = arr;
  const int* cp = carr.data();
  EXPECT_EQ(cp[0], 1);
}

TEST(JArrayTest, Alloc) {
  jarray<int> arr = {1, 2, 3};
  arr.alloc(5);
  EXPECT_EQ(arr.length(), 5);
  // new memory is zero-initialized
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(arr[i], 0);
  }
}

TEST(JArrayTest, AllocZero) {
  jarray<int> arr = {1, 2, 3};
  arr.alloc(0);
  EXPECT_TRUE(arr.empty());
  EXPECT_EQ(arr.data(), nullptr);
}

TEST(JArrayTest, Swap) {
  jarray<int> a = {1, 2, 3};
  jarray<int> b = {4, 5};
  a.swap(b);

  EXPECT_EQ(a.length(), 2);
  EXPECT_EQ(a[0], 4);
  EXPECT_EQ(a[1], 5);

  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 1);
  EXPECT_EQ(b[1], 2);
  EXPECT_EQ(b[2], 3);
}

TEST(JArrayTest, FillAll) {
  jarray<int> arr(5);
  arr.fill(42);
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(arr[i], 42);
  }
}

TEST(JArrayTest, FillRange) {
  jarray<int> arr = {1, 2, 3, 4, 5};
  arr.fill(1, 4, 99);
  EXPECT_EQ(arr[0], 1);
  EXPECT_EQ(arr[1], 99);
  EXPECT_EQ(arr[2], 99);
  EXPECT_EQ(arr[3], 99);
  EXPECT_EQ(arr[4], 5);
}

TEST(JArrayTest, FillEmptyRange) {
  jarray<int> arr = {1, 2, 3};
  arr.fill(1, 1, 99);  // from == to, no-op
  EXPECT_EQ(arr[0], 1);
  EXPECT_EQ(arr[1], 2);
  EXPECT_EQ(arr[2], 3);
}

TEST(JArrayTest, CopyOf) {
  jarray<int> a = {1, 2, 3};
  auto b = a.copy_of(5);
  EXPECT_EQ(b.length(), 5);
  EXPECT_EQ(b[0], 1);
  EXPECT_EQ(b[1], 2);
  EXPECT_EQ(b[2], 3);
  EXPECT_EQ(b[3], 0);
  EXPECT_EQ(b[4], 0);
}

TEST(JArrayTest, CopyOfTruncate) {
  jarray<int> a = {1, 2, 3, 4, 5};
  auto b = a.copy_of(3);
  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 1);
  EXPECT_EQ(b[1], 2);
  EXPECT_EQ(b[2], 3);
}

TEST(JArrayTest, CopyOfRange) {
  jarray<int> a = {10, 20, 30, 40, 50};
  auto b = a.copy_of_range(1, 4);
  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 20);
  EXPECT_EQ(b[1], 30);
  EXPECT_EQ(b[2], 40);
}

TEST(JArrayTest, Clone) {
  jarray<int> a = {1, 2, 3};
  auto b = a.clone();

  EXPECT_EQ(b.length(), 3);
  EXPECT_EQ(b[0], 1);

  b[0] = 99;
  EXPECT_EQ(a[0], 1);  // deep copy
}

TEST(JArrayTest, BeginEnd) {
  jarray<int> arr = {1, 2, 3};
  int sum = 0;
  for (auto it = arr.begin(); it != arr.end(); ++it) {
    sum += *it;
  }
  EXPECT_EQ(sum, 6);

  const auto& carr = arr;
  int csum = 0;
  for (auto it = carr.begin(); it != carr.end(); ++it) {
    csum += *it;
  }
  EXPECT_EQ(csum, 6);
}

TEST(JArrayTest, BeginEndEmpty) {
  jarray<int> arr;
  EXPECT_EQ(arr.begin(), arr.end());
}

TEST(JArrayTest, JarrayCopy) {
  jarray<int> src = {1, 2, 3, 4, 5};
  jarray<int> dst(5);
  jarray_copy(src, 1, dst, 2, 3);

  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 2);
  EXPECT_EQ(dst[3], 3);
  EXPECT_EQ(dst[4], 4);
}

TEST(JArrayTest, JarrayCopyZeroLen) {
  jarray<int> src = {1, 2, 3};
  jarray<int> dst(3);
  jarray_copy(src, 0, dst, 0, 0);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
}
