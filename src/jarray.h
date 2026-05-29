#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <type_traits>

template <typename T>
class jarray {
  static_assert(std::is_trivially_copyable_v<T>, "jarray<T> only support trivially copyable types");

 public:
  jarray() = default;

  explicit jarray(int len) : data_(len > 0 ? new T[len]() : nullptr), len_(len) {
    assert(len >= 0);
  }

  jarray(std::initializer_list<T> init)
      : data_(init.size() > 0 ? new T[init.size()]() : nullptr), len_(int(init.size())) {
    if (len_ > 0) {
      std::memcpy(data_, init.begin(), static_cast<std::size_t>(len_) * sizeof(T));
    }
  }

  jarray(const jarray& other) : data_(other.len_ > 0 ? new T[other.len_]() : nullptr), len_(other.len_) {
    if (len_ > 0) {
      std::memcpy(data_, other.data_, std::size_t(len_) * sizeof(T));
    }
  }

  jarray& operator=(const jarray& other) {
    if (this == &other) {
      return *this;
    }

    jarray tmp(other);
    swap(tmp);
    return *this;
  }

  jarray(jarray&& other) noexcept : data_(other.data_), len_(other.len_) {
    other.data_ = nullptr;
    other.len_ = 0;
  }

  jarray& operator=(jarray&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    delete[] data_;

    data_ = other.data_;
    len_ = other.len_;

    other.data_ = nullptr;
    other.len_ = 0;

    return *this;
  }

  ~jarray() {
    delete[] data_;
  }

  int length() const {
    return len_;
  }

  bool empty() const {
    return len_ == 0;
  }

  T& operator[](int index) {
    assert(index >= 0 && index < len_);
    return data_[index];
  }

  const T& operator[](int index) const {
    assert(index >= 0 && index < len_);
    return data_[index];
  }

  T* data() {
    return data_;
  }

  const T* data() const {
    return data_;
  }

  // 对应 Java: value = new int[n]
  void alloc(int len) {
    assert(len >= 0);

    T* new_data = len > 0 ? new T[len]() : nullptr;

    delete[] data_;

    data_ = new_data;
    len_ = len;
  }

  void swap(jarray& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(len_, other.len_);
  }

  // 对应 Java: Arrays.fill(arr, from, to, val)
  void fill(T val) {
    if (len_ == 0) {
      return;
    }

    std::fill(data_, data_ + len_, val);
  }

  // 对应 Java: Arrays.fill(arr, from, to, val)
  void fill(int from, int to, T val) {
    assert(from >= 0);
    assert(from <= to);
    assert(to <= len_);
    if (from == to) {
      return;
    }
    std::fill(data_ + from, data_ + to, val);
  }

  // 对应 Java: Arrays.copyOf(arr, newLen)
  jarray copy_of(int new_len) const {
    assert(new_len >= 0);
    jarray result(new_len);
    const int copy_len = std::min(len_, new_len);
    if (copy_len > 0) {
      std::memcpy(result.data_, data_, std::size_t(copy_len) * sizeof(T));
    }
    return result;
  }

  // 对应 Java: Arrays.copyOfRange(arr, from, to)
  jarray copy_of_range(int from, int to) const {
    assert(from >= 0);
    assert(from <= to);
    assert(from <= len_);
    const int new_len = to - from;
    jarray result(new_len);
    const int available = len_ - from;
    const int copy_len = std::min(available, new_len);
    if (copy_len > 0) {
      std::memcpy(result.data(), data_ + from, std::size_t(copy_len) * sizeof(T));
    }
    return result;
  }

  // 对应 Java: arr.clone()
  jarray clone() const {
    return jarray(*this);
  }

  T* begin() {
    return data_;
  }
  T* end() {
    return len_ == 0 ? nullptr : data_ + len_;
  }
  const T* begin() const {
    return data_;
  }
  const T* end() const {
    return len_ == 0 ? nullptr : data_ + len_;
  }

 private:
  T* data_{nullptr};
  int len_{0};
};

// 对应 Java: System.arraycopy(src, srcPos, dst, dstPos, len)
template <typename T>
void jarray_copy(const jarray<T>& src, int src_pos, jarray<T>& dst, int dst_pos, int len) {
  assert(src_pos >= 0);
  assert(dst_pos >= 0);
  assert(len >= 0);
  assert(src_pos + len <= src.length());
  assert(dst_pos + len <= dst.length());
  if (len == 0) {
    return;
  }
  std::memmove(dst.data() + dst_pos, src.data() + src_pos, static_cast<std::size_t>(len) * sizeof(T));
}
