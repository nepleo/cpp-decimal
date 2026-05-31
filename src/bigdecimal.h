#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// round_mode
// 舍入模式: 用于可能丢弃精度的十进制 (或其它定点/高精度) 运算.
//
// 每种模式规定: 在缩小精度后, 结果最低有效位应如何确定.
// 若返回的有效位数少于精确值所需位数, 则被去掉的尾部称为 "丢弃部分";
// 不论这些位在数值上占多大权重, 丢弃部分的绝对值可以大于 1.
//
// 下列汇总表给出典型行为: 将两位有效十进制数舍入为一位时的结果
// (正数示例为主; 负数时 UP/DOWN/CEILING/FLOOR 的对称关系见各模式说明).
//
//   输入   UP  DOWN  CEILING  FLOOR  HALF_UP  HALF_DOWN  HALF_EVEN  UNNECESSARY
//   5.5    6    5      6       5       6         5          6         抛异常
//   2.5    3    2      3       2       3         2          2         抛异常
//   1.6    2    1      2       1       2         2          2         抛异常
//   1.1    2    1      2       1       1         1          1         抛异常
//   1.0    1    1      1       1       1         1          1         1
//  -1.0   -1   -1     -1      -1      -1        -1         -1        -1
//  -1.1   -2   -1     -1      -2      -1        -1         -1        抛异常
//  -1.6   -2   -1     -1      -2      -2        -2         -2        抛异常
//  -2.5   -3   -2     -2      -3      -3        -2         -2        抛异常
//  -5.5   -6   -5     -5      -6      -6        -5         -6        抛异常
//
// 枚举常量说明:
//
//   UP
//     远离零舍入. 丢弃部分非零时, 对保留的最后一位进 1.
//     绝对值不会变小.
//
//   DOWN
//     向零舍入 (截断). 丢弃部分非零时也不进位.
//     绝对值不会变大. 等价于向 0 取整.
//
//   CEILING
//     向正无穷舍入. 结果 >= 0 时行为同 UP; 结果 < 0 时同 DOWN.
//     数值不会变小.
//
//   FLOOR
//     向负无穷舍入. 结果 >= 0 时行为同 DOWN; 结果 < 0 时同 UP.
//     数值不会变大.
//
//   HALF_UP
//     向最近邻舍入; 两侧等距时向上 (远离零) 舍入.
//     丢弃部分 >= 0.5 时按 UP, 否则按 DOWN.
//     常见的 "四舍五入".
//
//   HALF_DOWN
//     向最近邻舍入; 两侧等距时向下 (向零) 舍入.
//     丢弃部分 > 0.5 时按 UP, 否则按 DOWN (恰好 0.5 不进位).
//
//   HALF_EVEN
//     向最近邻舍入; 两侧等距时向偶数邻舍入 (银行家舍入).
//     被舍位左侧为奇数时等同 HALF_UP, 为偶数时等同 HALF_DOWN.
//     在重复舍入序列中可减小累积偏差.
//
//   UNNECESSARY
//     要求运算结果在目标精度下完全精确, 不允许舍入.
//     若存在非零丢弃部分, 调用方应报错 (例如抛 std::invalid_argument 或
//     项目约定的算术异常类型).
//
enum class round_mode {
  UP = 0,
  DOWN = 1,
  CEILING = 2,
  FLOOR = 3,
  HALF_UP = 4,
  HALF_DOWN = 5,
  HALF_EVEN = 6,
  UNNECESSARY = 7,
};

// 将整型常量转换为 round_mode.
//
// 合法范围为 [UP, UNNECESSARY] (含端点), 与上述枚举底层值一致.
// 越界时抛出 std::invalid_argument.
inline round_mode value_of(int v) {
  if (v < static_cast<int>(round_mode::UP) || v > static_cast<int>(round_mode::UNNECESSARY)) {
    throw std::invalid_argument("argument out of range");
  }
  return static_cast<round_mode>(v);
}

// math_context
// 不可变的数值运算上下文.
//
// math_context 封装会影响数值运算的上下文设置，例如 big_decimal 运算使用的
// 精度和舍入模式. precision 表示运算使用的十进制有效位数，0 表示不限精度；
// rounding_mode 表示舍入算法.
struct math_context {
  // 精度为 0，舍入模式为 HALF_UP.
  static const math_context UNLIMITED;

  // IEEE 754 decimal32 对应的 7 位精度，舍入模式为 HALF_EVEN.
  static const math_context DECIMAL32;

  // IEEE 754 decimal64 对应的 16 位精度，舍入模式为 HALF_EVEN.
  static const math_context DECIMAL64;

  // IEEE 754 decimal128 对应的 34 位精度，舍入模式为 HALF_EVEN.
  static const math_context DECIMAL128;

  static constexpr int min_digits = 0;
  static constexpr round_mode default_rounding_mode = round_mode::HALF_UP;

  // 使用指定精度和默认 HALF_UP 舍入模式构造上下文.
  math_context(int set_precision) : math_context(set_precision, default_rounding_mode) {
  }

  // 使用指定精度和舍入模式构造上下文.
  //
  // set_precision 必须非负；否则抛出 std::invalid_argument("Digits < 0").
  math_context(int set_precision, round_mode set_rounding_mode)
      : precision_(set_precision), rounding_mode_(set_rounding_mode) {
    if (set_precision < min_digits) {
      throw std::invalid_argument("Digits < 0");
    }
  }

  // 显式声明拷贝构造，避免接口行为依赖编译器隐式生成.
  math_context(const math_context& other) = default;

  // 显式声明拷贝赋值，保持值对象语义.
  math_context& operator=(const math_context& other) = default;

  // 显式声明移动构造，移动后源对象恢复为 UNLIMITED 状态.
  math_context(math_context&& other) noexcept : precision_(other.precision_), rounding_mode_(other.rounding_mode_) {
    other.precision_ = 0;
    other.rounding_mode_ = default_rounding_mode;
  }

  // 显式声明移动赋值，移动后源对象恢复为 UNLIMITED 状态.
  math_context& operator=(math_context&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    precision_ = other.precision_;
    rounding_mode_ = other.rounding_mode_;
    other.precision_ = 0;
    other.rounding_mode_ = default_rounding_mode;
    return *this;
  }

  // 返回 precision 设置，始终非负.
  int precision() const {
    return precision_;
  }

  // 返回 rounding_mode 设置.
  round_mode get_rounding_mode() const {
    return rounding_mode_;
  }

  // 判断两个上下文是否具有完全相同的设置.
  bool operator==(const math_context& other) const {
    return precision_ == other.precision_ && rounding_mode_ == other.rounding_mode_;
  }

  bool operator!=(const math_context& other) const {
    return !(*this == other);
  }

  // 返回 hash code，公式对应 Java MathContext#hashCode.
  int hash_code() const {
    return precision_ + static_cast<int>(rounding_mode_) * 59;
  }

  int precision_{0};
  round_mode rounding_mode_{default_rounding_mode};
};

inline const math_context math_context::UNLIMITED{0, round_mode::HALF_UP};
inline const math_context math_context::DECIMAL32{7, round_mode::HALF_EVEN};
inline const math_context math_context::DECIMAL64{16, round_mode::HALF_EVEN};
inline const math_context math_context::DECIMAL128{34, round_mode::HALF_EVEN};

// jarray
template <typename T>
struct jarray {
  static_assert(std::is_trivially_copyable_v<T>, "jarray<T> only support trivially copyable types");

  jarray() = default;

  jarray(int len) : data_(len > 0 ? new T[len]() : nullptr), len_(len) {
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

// mutable_bigint
struct mutable_bigint {
  // 大端 limb 数组，存放绝对值；有效区间由 offset_ 与 int_len_ 界定.
  // limb = 大整数绝对值在数组里的一个 32(uint32_t)位存储单元
  jarray<uint32_t> value_;
  // value_ 数组中当前用于保存本数绝对值的 int 个数.数值从 offset_ 开始，
  // offset_ + int_len_ 可以小于 value_.length().
  int32_t int_len_{0};
  // 本数绝对值在 value_ 数组中的起始偏移量.
  int32_t offset_{0};

  // 常量声明
  static constexpr int32_t KNUTH_POW2_THRESH_LEN = 6;
  static constexpr int32_t KNUTH_POW2_THRESH_ZEROS = 3;
  static constexpr int32_t BURNIKEL_ZIEGLER_THRESHOLD = 80;
  static constexpr int32_t BURNIKEL_ZIEGLER_OFFSET = 40;
  static const mutable_bigint ONE;

  // 默认构造函数，创建一个容量为 1 个 limb 的空 MutableBigInteger.
  mutable_bigint() {
    value_ = jarray<uint32_t>(1);
    int_len_ = 0;
  }

  // 使用一个 32 位 limb 构造数值.
  mutable_bigint(uint32_t val) {
    value_ = jarray<uint32_t>(1);
    int_len_ = 1;
    value_[0] = val;
  }

  // 使用给定的大端 limb 数组构造数值，数组全长作为有效长度.
  mutable_bigint(const jarray<uint32_t>& val) {
    value_ = jarray<uint32_t>(val);
    int_len_ = val.length();
  }

  // 使用另一个 mutable_bigint 的有效数值构造副本.
  mutable_bigint(const mutable_bigint& val) {
    int_len_ = val.int_len_;
    value_ = val.value_.copy_of_range(val.offset_, val.offset_ + int_len_);
  }

  // 显式声明拷贝赋值，复制另一个对象的有效数值.
  mutable_bigint& operator=(const mutable_bigint& val) {
    if (this == &val) {
      return *this;
    }
    int_len_ = val.int_len_;
    offset_ = 0;
    value_ = val.value_.copy_of_range(val.offset_, val.offset_ + int_len_);
    return *this;
  }

  // 显式声明移动构造，移动后源对象恢复为零.
  mutable_bigint(mutable_bigint&& val) noexcept
      : value_(std::move(val.value_)), int_len_(val.int_len_), offset_(val.offset_) {
    val.int_len_ = 0;
    val.offset_ = 0;
  }

  // 显式声明移动赋值，移动后源对象恢复为零.
  mutable_bigint& operator=(mutable_bigint&& val) noexcept {
    if (this == &val) {
      return *this;
    }
    value_ = std::move(val.value_);
    int_len_ = val.int_len_;
    offset_ = val.offset_;
    val.int_len_ = 0;
    val.offset_ = 0;
    return *this;
  }

  // 清空当前对象以便复用，并把已分配数组中的内容全部置零.
  void clear() {
    offset_ = 0;
    int_len_ = 0;
    value_.fill(0);
  }

  // 将当前对象设为零，并去掉偏移，但不清空底层数组.
  void reset() {
    offset_ = 0;
    int_len_ = 0;
  }

  // 设置有效数值中指定下标的 limb.
  void set_int(int32_t index, uint32_t val) {
    value_[offset_ + index] = val;
  }

  // 将 value_ 设置为指定数组，并把 int_len_ 设置为指定长度.
  void set_value(const jarray<uint32_t>& val, int32_t length) {
    assert(length >= 0);
    assert(length <= val.length());
    value_ = val;
    int_len_ = length;
    offset_ = 0;
  }

  // 将 src 的有效数值复制到当前对象中.
  void copy_value(const mutable_bigint& src) {
    const int32_t len = src.int_len_;
    if (value_.length() < len) {
      value_.alloc(len);
    }
    jarray_copy(src.value_, src.offset_, value_, 0, len);
    int_len_ = len;
    offset_ = 0;
  }

  // 将指定数组完整复制到当前对象中，并以数组长度作为有效长度.
  void copy_value(const jarray<uint32_t>& val) {
    const int32_t len = val.length();
    if (value_.length() < len) {
      value_.alloc(len);
    }
    jarray_copy(val, 0, value_, 0, len);
    int_len_ = len;
    offset_ = 0;
  }

  // 将当前数转换为紧凑的大端 limb 数组，长度等于 int_len_，不包含前导零.
  jarray<uint32_t> to_int_array() const {
    jarray<uint32_t> result(int_len_);
    for (int32_t i = 0; i < int_len_; ++i) {
      result[i] = value_[offset_ + i];
    }
    return result;
  }

  // 当前数是否等于 0.
  bool is_zero() const {
    return int_len_ == 0;
  }

  // 当前数是否等于 1.
  bool is_one() const {
    return int_len_ == 1 && value_[offset_] == 1;
  }

  // 当前数是否为偶数.
  bool is_even() const {
    return int_len_ == 0 || ((value_[offset_ + int_len_ - 1] & 1U) == 0);
  }

  // 当前数是否为奇数.
  bool is_odd() const {
    return !is_zero() && ((value_[offset_ + int_len_ - 1] & 1U) == 1);
  }

  // 确保对象处于规范形式：去掉前导 0 limb；如果数值为 0，则 int_len_ 为 0 且 offset_ 置 0.
  void normalize() {
    if (int_len_ == 0) {
      offset_ = 0;
      return;
    }

    int32_t index = offset_;
    if (value_[index] != 0) {
      return;
    }

    const int32_t index_bound = index + int_len_;
    do {
      ++index;
    } while (index < index_bound && value_[index] == 0);

    const int32_t num_zeros = index - offset_;
    int_len_ -= num_zeros;
    offset_ = int_len_ == 0 ? 0 : offset_ + num_zeros;
  }

  // 当前对象是否处于规范形式：没有前导 0，且 int_len_ + offset_ 不越界.
  bool is_normal() const {
    if (int_len_ + offset_ > value_.length()) {
      return false;
    }
    if (int_len_ == 0) {
      return true;
    }
    return value_[offset_] != 0;
  }

  static int32_t number_of_trailing_zeros(uint32_t limb) {
    return limb == 0 ? 32 : __builtin_ctz(limb);
  }

  static int32_t number_of_leading_zeros(uint32_t limb) {
    return limb == 0 ? 32 : __builtin_clz(limb);
  }

  static int32_t bit_length_for_limb(uint32_t limb) {
    return 32 - number_of_leading_zeros(limb);
  }

  // 用一个 32-bit limb d 去除 64-bit n.
  // 返回值高 32 bit 为余数，低 32 bit 为商.
  static uint64_t div_word(uint64_t n, uint32_t d) {
    const uint64_t d_long = d & 0xffffffffULL;
    uint64_t q = 0;
    uint64_t r = 0;
    if (d_long == 1) {
      q = (uint32_t)n;
      return (r << 32) | (q & 0xffffffffULL);
    }

    q = n / d_long;
    r = n - q * d_long;
    return (r << 32) | (q & 0xffffffffULL);
  }

  // 按无符号 64-bit 值比较 one 和 two；当 one 大于 two 时返回 true.
  static bool unsigned_long_compare(uint64_t one, uint64_t two) {
    return one > two;
  }

  // long 除法专用的乘减辅助函数；dh 为除数高 32 bit，dl 为除数低 32 bit.
  int32_t mulsub_long(jarray<uint32_t>& q, uint32_t dh, uint32_t dl, uint32_t x, int32_t offset) {
    const uint64_t x_long = x & 0xffffffffULL;
    offset += 2;

    uint64_t product = (dl & 0xffffffffULL) * x_long;
    int64_t difference = (int64_t)(int32_t)q[offset] - (int64_t)product;
    q[offset--] = (uint32_t)difference;
    uint64_t carry =
        (product >> 32) + ((((uint64_t)difference & 0xffffffffULL) > ((~(uint32_t)product) & 0xffffffffULL)) ? 1 : 0);

    product = (dh & 0xffffffffULL) * x_long + carry;
    difference = (int64_t)(int32_t)q[offset] - (int64_t)product;
    q[offset--] = (uint32_t)difference;
    carry =
        (product >> 32) + ((((uint64_t)difference & 0xffffffffULL) > ((~(uint32_t)product) & 0xffffffffULL)) ? 1 : 0);
    return (int32_t)carry;
  }

  // 除法用辅助函数：从 q 的指定 offset 位置减去 a*x，返回 borrow.
  int32_t mulsub(jarray<uint32_t>& q, const jarray<uint32_t>& a, uint32_t x, int32_t len, int32_t offset) {
    const uint64_t x_long = x & 0xffffffffULL;
    uint64_t carry = 0;
    offset += len;

    for (int32_t j = len - 1; j >= 0; --j) {
      const uint64_t product = (a[j] & 0xffffffffULL) * x_long + carry;
      const int64_t difference = (int64_t)(int32_t)q[offset] - (int64_t)product;
      q[offset--] = (uint32_t)difference;
      carry =
          (product >> 32) + ((((uint64_t)difference & 0xffffffffULL) > ((~(uint32_t)product) & 0xffffffffULL)) ? 1 : 0);
    }

    return (int32_t)carry;
  }

  // 与 mulsub 类似，但不更新 q 数组，只返回最终 borrow.
  int32_t mulsub_borrow(const jarray<uint32_t>& q, const jarray<uint32_t>& a, uint32_t x, int32_t len, int32_t offset) {
    const uint64_t x_long = x & 0xffffffffULL;
    uint64_t carry = 0;
    offset += len;

    for (int32_t j = len - 1; j >= 0; --j) {
      const uint64_t product = (a[j] & 0xffffffffULL) * x_long + carry;
      const int64_t difference = (int64_t)(int32_t)q[offset--] - (int64_t)product;
      carry =
          (product >> 32) + ((((uint64_t)difference & 0xffffffffULL) > ((~(uint32_t)product) & 0xffffffffULL)) ? 1 : 0);
    }

    return (int32_t)carry;
  }

  // 除法校正用辅助函数：把除数 a 加回 result 的指定 offset 位置.
  int32_t divadd(const jarray<uint32_t>& a, jarray<uint32_t>& result, int32_t offset) {
    uint64_t carry = 0;

    for (int32_t j = a.length() - 1; j >= 0; --j) {
      const uint64_t sum = (a[j] & 0xffffffffULL) + (result[j + offset] & 0xffffffffULL) + carry;
      result[j + offset] = (uint32_t)sum;
      carry = sum >> 32;
    }

    return (int32_t)carry;
  }

  // long 除法专用的 divadd 版本；dh 为除数高 32 bit，dl 为除数低 32 bit.
  int32_t divadd_long(uint32_t dh, uint32_t dl, jarray<uint32_t>& result, int32_t offset) {
    uint64_t carry = 0;

    uint64_t sum = (dl & 0xffffffffULL) + (result[1 + offset] & 0xffffffffULL);
    result[1 + offset] = (uint32_t)sum;

    sum = (dh & 0xffffffffULL) + (result[offset] & 0xffffffffULL) + carry;
    result[offset] = (uint32_t)sum;
    carry = sum >> 32;
    return (int32_t)carry;
  }

  // 将 src 指定范围左移 shift bit 后复制到 dst.
  static void copy_and_shift(const jarray<uint32_t>& src, int32_t src_from, int32_t src_len, jarray<uint32_t>& dst,
                             int32_t dst_from, int32_t shift) {
    const int32_t n2 = 32 - shift;
    uint32_t c = src[src_from];
    for (int32_t i = 0; i < src_len - 1; ++i) {
      const uint32_t b = c;
      c = src[++src_from];
      dst[dst_from + i] = (b << shift) | (c >> n2);
    }
    dst[dst_from + src_len - 1] = c << shift;
  }

  // 用一个 32-bit limb divisor 除当前对象，商写入 quotient，返回余数.
  uint32_t divide_one_word(uint32_t divisor, mutable_bigint& quotient) const {
    const uint64_t divisor_long = divisor & 0xffffffffULL;

    if (int_len_ == 1) {
      const uint64_t dividend_value = value_[offset_] & 0xffffffffULL;
      const uint32_t q = (uint32_t)(dividend_value / divisor_long);
      const uint32_t r = (uint32_t)(dividend_value - (q & 0xffffffffULL) * divisor_long);
      if (quotient.value_.length() < 1) {
        quotient.value_ = jarray<uint32_t>(1);
      }
      quotient.value_[0] = q;
      quotient.int_len_ = q == 0 ? 0 : 1;
      quotient.offset_ = 0;
      return r;
    }

    if (quotient.value_.length() < int_len_) {
      quotient.value_ = jarray<uint32_t>(int_len_);
    }
    quotient.offset_ = 0;
    quotient.int_len_ = int_len_;

    const int32_t shift = number_of_leading_zeros(divisor);

    uint32_t rem = value_[offset_];
    uint64_t rem_long = rem & 0xffffffffULL;
    if (rem_long < divisor_long) {
      quotient.value_[0] = 0;
    } else {
      quotient.value_[0] = (uint32_t)(rem_long / divisor_long);
      rem = (uint32_t)(rem_long - (quotient.value_[0] & 0xffffffffULL) * divisor_long);
      rem_long = rem & 0xffffffffULL;
    }

    int32_t xlen = int_len_;
    while (--xlen > 0) {
      const uint64_t dividend_estimate = (rem_long << 32) | (value_[offset_ + int_len_ - xlen] & 0xffffffffULL);
      uint32_t q = 0;
      if ((dividend_estimate >> 63) == 0) {
        q = (uint32_t)(dividend_estimate / divisor_long);
        rem = (uint32_t)(dividend_estimate - (q & 0xffffffffULL) * divisor_long);
      } else {
        const uint64_t tmp = div_word(dividend_estimate, divisor);
        q = (uint32_t)(tmp & 0xffffffffULL);
        rem = (uint32_t)(tmp >> 32);
      }
      quotient.value_[int_len_ - xlen] = q;
      rem_long = rem & 0xffffffffULL;
    }

    quotient.normalize();
    return shift > 0 ? (uint32_t)(rem % divisor) : rem;
  }

  // Knuth Algorithm D 的多 limb 除法主体；商写入 quotient，按需返回余数.
  mutable_bigint divide_magnitude(const mutable_bigint& div, mutable_bigint& quotient, bool need_remainder) {
    const int32_t shift = number_of_leading_zeros(div.value_[div.offset_]);
    const int32_t dlen = div.int_len_;
    jarray<uint32_t> divisor;
    mutable_bigint rem;

    if (shift > 0) {
      divisor = jarray<uint32_t>(dlen);
      copy_and_shift(div.value_, div.offset_, dlen, divisor, 0, shift);
      if (number_of_leading_zeros(value_[offset_]) >= shift) {
        jarray<uint32_t> remarr(int_len_ + 1);
        rem = mutable_bigint(remarr);
        rem.int_len_ = int_len_;
        rem.offset_ = 1;
        copy_and_shift(value_, offset_, int_len_, remarr, 1, shift);
        rem.value_ = std::move(remarr);
      } else {
        jarray<uint32_t> remarr(int_len_ + 2);
        rem = mutable_bigint(remarr);
        rem.int_len_ = int_len_ + 1;
        rem.offset_ = 1;
        int32_t r_from = offset_;
        uint32_t c = 0;
        const int32_t n2 = 32 - shift;
        for (int32_t i = 1; i < int_len_ + 1; ++i, ++r_from) {
          const uint32_t b = c;
          c = value_[r_from];
          remarr[i] = (b << shift) | (c >> n2);
        }
        remarr[int_len_ + 1] = c << shift;
        rem.value_ = std::move(remarr);
      }
    } else {
      divisor = div.value_.copy_of_range(div.offset_, div.offset_ + div.int_len_);
      rem = mutable_bigint(jarray<uint32_t>(int_len_ + 1));
      jarray_copy(value_, offset_, rem.value_, 1, int_len_);
      rem.int_len_ = int_len_;
      rem.offset_ = 1;
    }

    const int32_t nlen = rem.int_len_;
    const int32_t limit = nlen - dlen + 1;
    if (quotient.value_.length() < limit) {
      quotient.value_ = jarray<uint32_t>(limit);
      quotient.offset_ = 0;
    }
    quotient.int_len_ = limit;
    jarray<uint32_t>& q = quotient.value_;

    if (rem.int_len_ == nlen) {
      rem.offset_ = 0;
      rem.value_[0] = 0;
      ++rem.int_len_;
    }

    const uint32_t dh = divisor[0];
    const uint64_t dh_long = dh & 0xffffffffULL;
    const uint32_t dl = divisor[1];

    for (int32_t j = 0; j < limit; ++j) {
      uint32_t qhat = 0;
      uint32_t qrem = 0;
      bool skip_correction = false;
      const uint32_t nh = rem.value_[j + rem.offset_];
      const int32_t nh2 = (int32_t)(nh + 0x80000000U);
      const uint32_t nm = rem.value_[j + 1 + rem.offset_];

      if (nh == dh) {
        qhat = ~0U;
        qrem = nh + nm;
        skip_correction = (int32_t)(qrem + 0x80000000U) < nh2;
      } else {
        const uint64_t n_chunk = ((uint64_t)nh << 32) | (nm & 0xffffffffULL);
        if ((n_chunk >> 63) == 0) {
          qhat = (uint32_t)(n_chunk / dh_long);
          qrem = (uint32_t)(n_chunk - (qhat & 0xffffffffULL) * dh_long);
        } else {
          const uint64_t tmp = div_word(n_chunk, dh);
          qhat = (uint32_t)(tmp & 0xffffffffULL);
          qrem = (uint32_t)(tmp >> 32);
        }
      }

      if (qhat == 0) {
        continue;
      }

      if (!skip_correction) {
        const uint64_t nl = rem.value_[j + 2 + rem.offset_] & 0xffffffffULL;
        uint64_t rs = ((qrem & 0xffffffffULL) << 32) | nl;
        uint64_t est_product = (dl & 0xffffffffULL) * (qhat & 0xffffffffULL);

        if (unsigned_long_compare(est_product, rs)) {
          --qhat;
          qrem = (uint32_t)((qrem & 0xffffffffULL) + dh_long);
          if ((qrem & 0xffffffffULL) >= dh_long) {
            est_product -= (dl & 0xffffffffULL);
            rs = ((qrem & 0xffffffffULL) << 32) | nl;
            if (unsigned_long_compare(est_product, rs)) {
              --qhat;
            }
          }
        }
      }

      const bool last = j == limit - 1;
      rem.value_[j + rem.offset_] = 0;
      int32_t borrow = 0;
      if (!last || need_remainder) {
        borrow = mulsub(rem.value_, divisor, qhat, dlen, j + rem.offset_);
      } else {
        borrow = mulsub_borrow(rem.value_, divisor, qhat, dlen, j + rem.offset_);
      }

      if ((int32_t)(borrow + 0x80000000U) > nh2) {
        if (!last || need_remainder) {
          divadd(divisor, rem.value_, j + 1 + rem.offset_);
        }
        --qhat;
      }

      q[j] = qhat;
    }

    if (need_remainder) {
      if (shift > 0) {
        rem.right_shift(shift);
      }
      rem.normalize();
    }
    quotient.normalize();
    return need_remainder ? rem : mutable_bigint();
  }

  mutable_bigint divide_knuth(const mutable_bigint& b, mutable_bigint& quotient) {
    return divide_knuth(b, quotient, true);
  }

  // 计算当前对象除以 b 的商和余数；商写入 quotient，返回余数.
  mutable_bigint divide_knuth(const mutable_bigint& b, mutable_bigint& quotient, bool need_remainder) {
    if (b.int_len_ == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }

    if (int_len_ == 0) {
      quotient.int_len_ = 0;
      quotient.offset_ = 0;
      return mutable_bigint();
    }

    const int32_t cmp = compare(b);
    if (cmp < 0) {
      quotient.int_len_ = 0;
      quotient.offset_ = 0;
      return need_remainder ? mutable_bigint(*this) : mutable_bigint();
    }
    if (cmp == 0) {
      if (quotient.value_.length() < 1) {
        quotient.value_ = jarray<uint32_t>(1);
      }
      quotient.value_[0] = 1;
      quotient.int_len_ = 1;
      quotient.offset_ = 0;
      return mutable_bigint();
    }

    quotient.clear();
    if (b.int_len_ == 1) {
      const uint32_t r = divide_one_word(b.value_[b.offset_], quotient);
      if (!need_remainder || r == 0) {
        return mutable_bigint();
      }
      return mutable_bigint(r);
    }

    if (int_len_ >= KNUTH_POW2_THRESH_LEN) {
      const int32_t trailing_zero_bits = std::min(get_lowest_set_bit(), b.get_lowest_set_bit());
      if (trailing_zero_bits >= KNUTH_POW2_THRESH_ZEROS * 32) {
        mutable_bigint a(*this);
        mutable_bigint b_shifted(b);
        a.right_shift(trailing_zero_bits);
        b_shifted.right_shift(trailing_zero_bits);
        mutable_bigint r = a.divide_knuth(b_shifted, quotient);
        r.left_shift(trailing_zero_bits);
        return r;
      }
    }

    return divide_magnitude(b, quotient, need_remainder);
  }

  // 将当前对象设为 n 个 limb，且每个 bit 都为 1.
  void ones(int32_t n) {
    if (n > value_.length()) {
      value_ = jarray<uint32_t>(n);
    }
    value_.fill(0xffffffffU);
    offset_ = 0;
    int_len_ = n;
  }

  // 只保留当前对象的低 n 个 limb.
  void keep_lower(int32_t n) {
    if (int_len_ >= n) {
      offset_ += int_len_ - n;
      int_len_ = n;
    }
  }

  // 返回当前对象低 n 个 limb 组成的 mutable_bigint.
  mutable_bigint get_lower(int32_t n) const {
    if (is_zero()) {
      return mutable_bigint();
    }
    if (int_len_ < n) {
      return mutable_bigint(*this);
    }

    int32_t len = n;
    while (len > 0 && value_[offset_ + int_len_ - len] == 0) {
      --len;
    }
    if (len == 0) {
      return mutable_bigint();
    }
    return mutable_bigint(value_.copy_of_range(offset_ + int_len_ - len, offset_ + int_len_));
  }

  // 返回从 index*block_length 开始的 block_length 个 limb，供 Burnikel-Ziegler 除法使用.
  mutable_bigint get_block(int32_t index, int32_t num_blocks, int32_t block_length) const {
    const int32_t block_start = index * block_length;
    if (block_start >= int_len_) {
      return mutable_bigint();
    }

    int32_t block_end = 0;
    if (index == num_blocks - 1) {
      block_end = int_len_;
    } else {
      block_end = (index + 1) * block_length;
    }
    if (block_end > int_len_) {
      return mutable_bigint();
    }

    return mutable_bigint(value_.copy_of_range(offset_ + int_len_ - block_end, offset_ + int_len_ - block_start));
  }

  // Burnikel-Ziegler 算法 1：用 2n limb 的当前对象除以 n limb 的 b.
  mutable_bigint divide2n1n(const mutable_bigint& b, mutable_bigint& quotient) {
    const int32_t n = b.int_len_;

    if (n % 2 != 0 || n < BURNIKEL_ZIEGLER_THRESHOLD) {
      return divide_knuth(b, quotient);
    }

    mutable_bigint a_upper(*this);
    a_upper.safe_right_shift(32 * (n / 2));
    keep_lower(n / 2);

    mutable_bigint q1;
    mutable_bigint r1 = a_upper.divide3n2n(b, q1);

    add_disjoint(r1, n / 2);
    mutable_bigint r2 = divide3n2n(b, quotient);

    quotient.add_disjoint(q1, n / 2);
    return r2;
  }

  // Burnikel-Ziegler 算法 2：用 3n limb 的当前对象除以 2n limb 的 b.
  mutable_bigint divide3n2n(const mutable_bigint& b, mutable_bigint& quotient) {
    const int32_t n = b.int_len_ / 2;

    mutable_bigint a12(*this);
    a12.safe_right_shift(32 * n);

    mutable_bigint b1(b);
    b1.safe_right_shift(32 * n);
    mutable_bigint b2 = b.get_lower(n);

    mutable_bigint r;
    mutable_bigint d;
    if (compare_shifted(b, n) < 0) {
      r = a12.divide2n1n(b1, quotient);
      quotient.multiply(b2, d);
    } else {
      quotient.ones(n);
      a12.add(b1);
      b1.left_shift(32 * n);
      a12.subtract(b1);
      r = a12;

      d = b2;
      d.left_shift(32 * n);
      d.subtract(b2);
    }

    r.left_shift(32 * n);
    r.add_lower(*this, n);

    while (r.compare(d) < 0) {
      r.add(b);
      quotient.subtract(ONE);
    }
    r.subtract(d);

    return r;
  }

  // 使用 Burnikel-Ziegler 算法计算当前对象除以 b 的商和余数.
  mutable_bigint divide_and_remainder_burnikel_ziegler(const mutable_bigint& b, mutable_bigint& quotient) {
    const int32_t r = int_len_;
    const int32_t s = b.int_len_;

    quotient.offset_ = 0;
    quotient.int_len_ = 0;

    if (r < s) {
      return *this;
    }

    const int32_t m = 1 << (32 - number_of_leading_zeros(s / BURNIKEL_ZIEGLER_THRESHOLD));

    const int32_t j = (s + m - 1) / m;
    const int32_t n = j * m;
    const uint64_t n32 = 32ULL * n;
    const int32_t sigma = (int32_t)std::max<uint64_t>(0, n32 - b.bit_length());

    mutable_bigint b_shifted(b);
    b_shifted.safe_left_shift(sigma);
    mutable_bigint a_shifted(*this);
    a_shifted.safe_left_shift(sigma);

    int32_t t = (int32_t)((a_shifted.bit_length() + n32) / n32);
    if (t < 2) {
      t = 2;
    }

    mutable_bigint a1 = a_shifted.get_block(t - 1, t, n);
    mutable_bigint z = a_shifted.get_block(t - 2, t, n);
    z.add_disjoint(a1, n);

    mutable_bigint qi;
    mutable_bigint ri;
    for (int32_t i = t - 2; i > 0; --i) {
      ri = z.divide2n1n(b_shifted, qi);

      z = a_shifted.get_block(i - 1, t, n);
      z.add_disjoint(ri, n);
      quotient.add_shifted(qi, i * n);
    }

    ri = z.divide2n1n(b_shifted, qi);
    quotient.add(qi);

    ri.right_shift(sigma);
    return ri;
  }

  mutable_bigint divide(const mutable_bigint& b, mutable_bigint& quotient) {
    return divide(b, quotient, true);
  }

  // 计算当前对象除以 b 的商和余数；根据规模选择 Knuth 或 Burnikel-Ziegler.
  mutable_bigint divide(const mutable_bigint& b, mutable_bigint& quotient, bool need_remainder) {
    if (b.int_len_ < BURNIKEL_ZIEGLER_THRESHOLD || int_len_ - b.int_len_ < BURNIKEL_ZIEGLER_OFFSET) {
      return divide_knuth(b, quotient, need_remainder);
    }
    return divide_and_remainder_burnikel_ziegler(b, quotient);
  }

  // 用一个正 64-bit divisor 除当前对象，商写入 quotient，返回余数.
  uint64_t divide(uint64_t v, mutable_bigint& quotient) {
    if (v == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }

    if (int_len_ == 0) {
      quotient.int_len_ = 0;
      quotient.offset_ = 0;
      return 0;
    }

    const uint32_t d = (uint32_t)(v >> 32);
    quotient.clear();
    if (d == 0) {
      return divide_one_word((uint32_t)v, quotient) & 0xffffffffULL;
    }
    return divide_long_magnitude(v, quotient).to_long();
  }

  // 用正 64-bit divisor 除当前对象，商写入 quotient，返回余数对象.
  mutable_bigint divide_long_magnitude(uint64_t ldivisor, mutable_bigint& quotient) {
    mutable_bigint rem(jarray<uint32_t>(int_len_ + 1));
    jarray_copy(value_, offset_, rem.value_, 1, int_len_);
    rem.int_len_ = int_len_;
    rem.offset_ = 1;

    const int32_t nlen = rem.int_len_;
    const int32_t limit = nlen - 2 + 1;
    if (quotient.value_.length() < limit) {
      quotient.value_ = jarray<uint32_t>(limit);
      quotient.offset_ = 0;
    }
    quotient.int_len_ = limit;
    jarray<uint32_t>& q = quotient.value_;

    const int32_t shift = ldivisor == 0 ? 64 : __builtin_clzll(ldivisor);
    if (shift > 0) {
      ldivisor <<= shift;
      rem.left_shift(shift);
    }

    if (rem.int_len_ == nlen) {
      rem.offset_ = 0;
      rem.value_[0] = 0;
      ++rem.int_len_;
    }

    const uint32_t dh = (uint32_t)(ldivisor >> 32);
    const uint64_t dh_long = dh & 0xffffffffULL;
    const uint32_t dl = (uint32_t)(ldivisor & 0xffffffffULL);

    for (int32_t j = 0; j < limit; ++j) {
      uint32_t qhat = 0;
      uint32_t qrem = 0;
      bool skip_correction = false;
      const uint32_t nh = rem.value_[j + rem.offset_];
      const int32_t nh2 = (int32_t)(nh + 0x80000000U);
      const uint32_t nm = rem.value_[j + 1 + rem.offset_];

      if (nh == dh) {
        qhat = ~0U;
        qrem = nh + nm;
        skip_correction = (int32_t)(qrem + 0x80000000U) < nh2;
      } else {
        const uint64_t n_chunk = ((uint64_t)nh << 32) | (nm & 0xffffffffULL);
        if ((n_chunk >> 63) == 0) {
          qhat = (uint32_t)(n_chunk / dh_long);
          qrem = (uint32_t)(n_chunk - (qhat & 0xffffffffULL) * dh_long);
        } else {
          const uint64_t tmp = div_word(n_chunk, dh);
          qhat = (uint32_t)(tmp & 0xffffffffULL);
          qrem = (uint32_t)(tmp >> 32);
        }
      }

      if (qhat == 0) {
        continue;
      }

      if (!skip_correction) {
        const uint64_t nl = rem.value_[j + 2 + rem.offset_] & 0xffffffffULL;
        uint64_t rs = ((qrem & 0xffffffffULL) << 32) | nl;
        uint64_t est_product = (dl & 0xffffffffULL) * (qhat & 0xffffffffULL);

        if (unsigned_long_compare(est_product, rs)) {
          --qhat;
          qrem = (uint32_t)((qrem & 0xffffffffULL) + dh_long);
          if ((qrem & 0xffffffffULL) >= dh_long) {
            est_product -= (dl & 0xffffffffULL);
            rs = ((qrem & 0xffffffffULL) << 32) | nl;
            if (unsigned_long_compare(est_product, rs)) {
              --qhat;
            }
          }
        }
      }

      rem.value_[j + rem.offset_] = 0;
      const int32_t borrow = mulsub_long(rem.value_, dh, dl, qhat, j + rem.offset_);

      if ((int32_t)(borrow + 0x80000000U) > nh2) {
        divadd_long(dh, dl, rem.value_, j + 1 + rem.offset_);
        --qhat;
      }

      q[j] = qhat;
    }

    if (shift > 0) {
      rem.right_shift(shift);
    }

    quotient.normalize();
    rem.normalize();
    return rem;
  }

  // 返回当前对象的整数平方根.
  mutable_bigint sqrt() {
    if (is_zero()) {
      return mutable_bigint(0);
    }
    if (int_len_ == 1 && (value_[offset_] & 0xffffffffULL) < 4) {
      return ONE;
    }

    const uint64_t bits = bit_length();
    if (bits <= 63) {
      const uint64_t v = to_long();
      uint64_t xk = 1ULL << ((bits + 1) / 2);
      while (true) {
        const uint64_t xk1 = (xk + v / xk) / 2;
        if (xk1 >= xk) {
          jarray<uint32_t> result(2);
          result[0] = (uint32_t)(xk >> 32);
          result[1] = (uint32_t)(xk & 0xffffffffULL);
          mutable_bigint out(result);
          out.normalize();
          return out;
        }
        xk = xk1;
      }
    }

    mutable_bigint xk(1);
    xk.left_shift((int32_t)((bits + 1) / 2));

    mutable_bigint xk1;
    while (true) {
      divide(xk, xk1, false);
      xk1.add(xk);
      xk1.right_shift(1);

      if (xk1.compare(xk) >= 0) {
        return xk;
      }

      xk.copy_value(xk1);
      xk1.reset();
    }
  }

  // 将 a 和 b 作为无符号 32-bit 整数计算 GCD.
  static uint32_t binary_gcd(uint32_t a, uint32_t b) {
    if (b == 0) {
      return a;
    }
    if (a == 0) {
      return b;
    }

    const int32_t a_zeros = number_of_trailing_zeros(a);
    const int32_t b_zeros = number_of_trailing_zeros(b);
    a >>= a_zeros;
    b >>= b_zeros;

    const int32_t t = a_zeros < b_zeros ? a_zeros : b_zeros;

    while (a != b) {
      if (a > b) {
        a -= b;
        a >>= number_of_trailing_zeros(a);
      } else {
        b -= a;
        b >>= number_of_trailing_zeros(b);
      }
    }
    return a << t;
  }

  // 使用二进制 GCD 算法计算当前对象和 v 的 GCD.
  mutable_bigint binary_gcd(mutable_bigint& v) {
    mutable_bigint u(*this);
    mutable_bigint vv(v);
    mutable_bigint r;

    const int32_t s1 = u.get_lowest_set_bit();
    const int32_t s2 = vv.get_lowest_set_bit();
    const int32_t k = s1 < s2 ? s1 : s2;
    if (k != 0) {
      u.right_shift(k);
      vv.right_shift(k);
    }

    const bool u_odd = k == s1;
    mutable_bigint* t = u_odd ? &vv : &u;
    int32_t tsign = u_odd ? -1 : 1;

    int32_t lb = 0;
    while ((lb = t->get_lowest_set_bit()) >= 0) {
      t->right_shift(lb);
      if (tsign > 0) {
        u = *t;
      } else {
        vv = *t;
      }

      if (u.int_len_ < 2 && vv.int_len_ < 2) {
        const uint32_t x = binary_gcd(u.value_[u.offset_], vv.value_[vv.offset_]);
        r.value_[0] = x;
        r.int_len_ = 1;
        r.offset_ = 0;
        if (k > 0) {
          r.left_shift(k);
        }
        return r;
      }

      tsign = u.difference(vv);
      if (tsign == 0) {
        break;
      }
      t = tsign >= 0 ? &u : &vv;
    }

    if (k > 0) {
      u.left_shift(k);
    }
    return u;
  }

  // 先用欧几里得算法缩小规模，再切换到二进制 GCD.
  mutable_bigint hybrid_gcd(mutable_bigint b) {
    mutable_bigint a(*this);
    mutable_bigint q;

    while (b.int_len_ != 0) {
      const int32_t diff = a.int_len_ > b.int_len_ ? a.int_len_ - b.int_len_ : b.int_len_ - a.int_len_;
      if (diff < 2) {
        return a.binary_gcd(b);
      }

      mutable_bigint r = a.divide(b, q);
      a = b;
      b = r;
    }
    return a;
  }

  // 返回 val 在 mod 2^32 下的乘法逆元，要求 val 为奇数.
  static uint32_t inverse_mod32(uint32_t val) {
    uint32_t t = val;
    t *= 2 - val * t;
    t *= 2 - val * t;
    t *= 2 - val * t;
    t *= 2 - val * t;
    return t;
  }

  // 返回 val 在 mod 2^64 下的乘法逆元，要求 val 为奇数.
  static uint64_t inverse_mod64(uint64_t val) {
    uint64_t t = val;
    t *= 2 - val * t;
    t *= 2 - val * t;
    t *= 2 - val * t;
    t *= 2 - val * t;
    t *= 2 - val * t;
    return t;
  }

  // Fixup 算法：计算 c * 2^(-k) mod p，要求 c < p 且 p 为奇数.
  static mutable_bigint fixup(mutable_bigint c, mutable_bigint p, int32_t k) {
    mutable_bigint temp;
    const uint32_t r = (uint32_t)(0U - inverse_mod32(p.value_[p.offset_ + p.int_len_ - 1]));

    for (int32_t i = 0, num_words = k >> 5; i < num_words; ++i) {
      const uint32_t v = r * c.value_[c.offset_ + c.int_len_ - 1];
      p.mul(v, temp);
      c.add(temp);
      --c.int_len_;
    }

    const int32_t num_bits = k & 0x1f;
    if (num_bits != 0) {
      uint32_t v = r * c.value_[c.offset_ + c.int_len_ - 1];
      v &= (1U << num_bits) - 1;
      p.mul(v, temp);
      c.add(temp);
      c.right_shift(num_bits);
    }

    if (c.compare(p) >= 0) {
      mutable_bigint q;
      c = c.divide(p, q);
    }

    return c;
  }

  // 计算 2^k 在 mod 下的乘法逆元，mod 必须为奇数.
  static mutable_bigint mod_inverse_bp2(const mutable_bigint& mod, int32_t k) {
    return fixup(mutable_bigint(1), mutable_bigint(mod), k);
  }

  // 使用扩展欧几里得算法计算当前对象在 mod 2^k 下的逆元.
  mutable_bigint euclid_mod_inverse(int32_t k) {
    mutable_bigint b(1);
    b.left_shift(k);
    mutable_bigint mod(b);

    mutable_bigint a(*this);
    mutable_bigint q;
    mutable_bigint r = b.divide(a, q);

    mutable_bigint swapper = b;
    b = r;
    r = swapper;

    mutable_bigint t1(q);
    mutable_bigint t0(1);
    mutable_bigint temp;

    while (!b.is_one()) {
      r = a.divide(b, q);
      if (r.int_len_ == 0) {
        throw std::runtime_error("BigInteger not invertible.");
      }

      swapper = r;
      a = swapper;

      if (q.int_len_ == 1) {
        t1.mul(q.value_[q.offset_], temp);
      } else {
        q.multiply(t1, temp);
      }
      swapper = q;
      q = temp;
      temp = swapper;
      t0.add(q);

      if (a.is_one()) {
        return t0;
      }

      r = b.divide(a, q);
      if (r.int_len_ == 0) {
        throw std::runtime_error("BigInteger not invertible.");
      }

      swapper = b;
      b = r;

      if (q.int_len_ == 1) {
        t0.mul(q.value_[q.offset_], temp);
      } else {
        q.multiply(t0, temp);
      }
      swapper = q;
      q = temp;
      temp = swapper;

      t1.add(q);
    }
    mod.subtract(t1);
    return mod;
  }

  // 计算当前对象在 mod 2^k 下的乘法逆元.
  mutable_bigint mod_inverse_mp2(int32_t k) {
    if (is_even()) {
      throw std::runtime_error("Non-invertible. (GCD != 1)");
    }

    if (k > 64) {
      return euclid_mod_inverse(k);
    }

    uint32_t t = inverse_mod32(value_[offset_ + int_len_ - 1]);

    if (k < 33) {
      t = k == 32 ? t : t & ((1U << k) - 1);
      return mutable_bigint(t);
    }

    uint64_t p_long = value_[offset_ + int_len_ - 1] & 0xffffffffULL;
    if (int_len_ > 1) {
      p_long |= (uint64_t)value_[offset_ + int_len_ - 2] << 32;
    }
    uint64_t t_long = t & 0xffffffffULL;
    t_long = t_long * (2 - p_long * t_long);
    t_long = k == 64 ? t_long : t_long & ((1ULL << k) - 1);

    jarray<uint32_t> result(2);
    result[0] = (uint32_t)(t_long >> 32);
    result[1] = (uint32_t)t_long;
    mutable_bigint out(result);
    out.int_len_ = 2;
    out.normalize();
    return out;
  }

  // 返回当前对象 mod p 的乘法逆元.
  mutable_bigint mutable_mod_inverse(const mutable_bigint& p);

  // Schroeppel 几乎逆算法，mod 必须为奇数.
  mutable_bigint mod_inverse(const mutable_bigint& mod);

  // 返回有效数值中指定下标的 limb.
  uint32_t get_int(int32_t index) const {
    return value_[offset_ + index];
  }

  // 返回有效数值中指定下标的 limb，并按无符号值扩展为 uint64_t.
  uint64_t get_long(int32_t index) const {
    return value_[offset_ + index] & 0xffffffffULL;
  }

  // 若 value_ 数组无法容纳 len 个 limb，则扩容为 len 个 limb.
  void ensure_capacity(int32_t len) {
    if (value_.length() < len) {
      value_.alloc(len);
      offset_ = 0;
      int_len_ = len;
    }
  }

  // 内部辅助方法，返回数值数组.调用方不应修改返回的数组.
  // 转换成 bigint 时,使对象变成规范紧凑形态
  jarray<uint32_t> get_magnitude_array() {
    if (offset_ > 0 || value_.length() != int_len_) {
      // 缩减 value 使其恰好等于有效数值
      jarray<uint32_t> tmp = value_.copy_of_range(offset_, offset_ + int_len_);
      value_.fill(0);
      offset_ = 0;
      int_len_ = tmp.length();
      value_ = tmp;
    }
    return value_;
  }

  // 返回最低 set bit 的 bit 下标；如果数值为 0，则返回 -1.
  int32_t get_lowest_set_bit() const {
    if (int_len_ == 0) {
      return -1;
    }

    int32_t j = int_len_ - 1;
    while (j > 0 && value_[j + offset_] == 0) {
      --j;
    }

    const uint32_t b = value_[j + offset_];
    if (b == 0) {
      return -1;
    }

    return ((int_len_ - 1 - j) << 5) + number_of_trailing_zeros(b);
  }

  // 将当前对象转换为 uint64_t.调用方必须保证当前数不超过 uint64_t 范围.
  uint64_t to_long() const {
    assert(int_len_ <= 2);
    if (int_len_ == 0) {
      return 0;
    }
    const uint64_t d = value_[offset_] & 0xffffffffULL;
    return int_len_ == 2 ? (d << 32) | (value_[offset_ + 1] & 0xffffffffULL) : d;
  }

  // 将当前数右移 n bit（n < 32）.假定 int_len_ > 0, n > 0.
  void primitive_right_shift(int32_t n) {
    assert(int_len_ > 0);
    assert(n > 0 && n < 32);

    const int32_t n2 = 32 - n;
    uint32_t c = value_[offset_ + int_len_ - 1];
    for (int32_t i = offset_ + int_len_ - 1; i > offset_; --i) {
      const uint32_t b = c;
      c = value_[i - 1];
      value_[i] = (c << n2) | (b >> n);
    }
    value_[offset_] >>= n;
  }

  // 将当前数左移 n bit（n < 32）.假定 int_len_ > 0, n > 0.
  void primitive_left_shift(int32_t n) {
    assert(int_len_ > 0);
    assert(n > 0 && n < 32);

    const int32_t n2 = 32 - n;
    uint32_t c = value_[offset_];
    const int32_t end = offset_ + int_len_ - 1;
    for (int32_t i = offset_; i < end; ++i) {
      const uint32_t b = c;
      c = value_[i + 1];
      value_[i] = (b << n) | (c >> n2);
    }
    value_[offset_ + int_len_ - 1] <<= n;
  }

  // 用较大的数减去较小的数，并把结果写回较大的那个操作数.
  // 如果结果写回当前对象，返回 1；如果结果写回 b，返回 -1；如果相等，返回 0.
  int32_t difference(mutable_bigint& b) {
    mutable_bigint* a = this;
    mutable_bigint* subtrahend = &b;
    const int32_t sign = compare(b);

    if (sign == 0) {
      return 0;
    }
    if (sign < 0) {
      std::swap(a, subtrahend);
    }

    int32_t x = a->int_len_;
    int32_t y = subtrahend->int_len_;
    int32_t borrow = 0;

    while (y > 0) {
      --x;
      --y;
      const int64_t diff = (int64_t)(a->value_[a->offset_ + x] & 0xffffffffULL) -
                           (int64_t)(subtrahend->value_[subtrahend->offset_ + y] & 0xffffffffULL) - borrow;
      a->value_[a->offset_ + x] = (uint32_t)diff;
      borrow = diff < 0 ? 1 : 0;
    }

    while (x > 0) {
      --x;
      const int64_t diff = (int64_t)(a->value_[a->offset_ + x] & 0xffffffffULL) - borrow;
      a->value_[a->offset_ + x] = (uint32_t)diff;
      borrow = diff < 0 ? 1 : 0;
    }

    a->normalize();
    return sign;
  }

  // 比较两个数的绝对值大小.
  // 返回 -1、0 或 1，分别表示当前数小于、等于或大于 b.
  // C++ 中 value_ 为 uint32_t，无需像 Java 那样加 0x80000000 模拟无符号比较.
  int32_t compare(const mutable_bigint& b) const {
    if (int_len_ < b.int_len_) {
      return -1;
    }
    if (int_len_ > b.int_len_) {
      return 1;
    }

    for (int32_t i = offset_, j = b.offset_; i < offset_ + int_len_; ++i, ++j) {
      if (value_[i] < b.value_[j]) {
        return -1;
      }
      if (value_[i] > b.value_[j]) {
        return 1;
      }
    }
    return 0;
  }

  // 等价于 b.left_shift(32*ints) 后 compare(b)，但不修改 b 的值.
  int32_t compare_shifted(const mutable_bigint& b, int32_t ints) const {
    const int32_t blen = b.int_len_;
    const int32_t alen = int_len_ - ints;
    if (alen < blen) {
      return -1;
    }
    if (alen > blen) {
      return 1;
    }

    for (int32_t i = offset_, j = b.offset_; i < offset_ + alen; ++i, ++j) {
      if (value_[i] < b.value_[j]) {
        return -1;
      }
      if (value_[i] > b.value_[j]) {
        return 1;
      }
    }
    return 0;
  }

  // 将当前数与 b 的一半比较（余数判断需要）.
  // 假定没有前导零，这对 divide() 的结果成立.
  int32_t compare_half(const mutable_bigint& b) const {
    const int32_t blen = b.int_len_;
    const int32_t len = int_len_;
    if (len <= 0) {
      return blen <= 0 ? 0 : -1;
    }
    if (len > blen) {
      return 1;
    }
    if (len < blen - 1) {
      return -1;
    }

    int32_t bstart = b.offset_;
    uint32_t carry = 0;
    if (len != blen) {
      if (b.value_[bstart] == 1) {
        ++bstart;
        carry = 0x80000000U;
      } else {
        return -1;
      }
    }

    for (int32_t i = offset_, j = bstart; i < offset_ + len;) {
      const uint32_t bv = b.value_[j++];
      const uint64_t hb = (uint64_t)(bv >> 1) + carry;
      const uint64_t v = value_[i++];
      if (v != hb) {
        return v < hb ? -1 : 1;
      }
      carry = (bv & 1U) << 31;
    }

    return carry == 0 ? 0 : -1;
  }

  // 返回当前绝对值的 bit 长度.
  uint64_t bit_length() const {
    if (int_len_ == 0) {
      return 0;
    }
    return (uint64_t)(int_len_)*32 - number_of_leading_zeros(value_[offset_]);
  }

  // 将当前数右移 n bit，结果保持规范形式.
  // 类似 safe_right_shift，但 n 不能超过当前数的 bit 长度.
  void right_shift(int32_t n) {
    if (int_len_ == 0) {
      return;
    }

    const int32_t n_ints = n >> 5;
    const int32_t n_bits = n & 0x1f;
    int_len_ -= n_ints;
    if (n_bits == 0) {
      return;
    }

    const int32_t bits_in_high_limb = bit_length_for_limb(value_[offset_]);
    if (n_bits >= bits_in_high_limb) {
      primitive_left_shift(32 - n_bits);
      --int_len_;
    } else {
      primitive_right_shift(n_bits);
    }
  }

  // 将当前数左移 n bit.
  // 如果 value_ 中已有足够的存储空间则会优先复用.使用 value_ 数组右侧的空间
  // 比左侧更快，因此在可能的情况下会优先从右侧扩展.
  void left_shift(int32_t n) {
    if (int_len_ == 0) {
      return;
    }

    const int32_t n_ints = n >> 5;
    const int32_t n_bits = n & 0x1f;
    const int32_t bits_in_high_limb = bit_length_for_limb(value_[offset_]);

    if (n <= 32 - bits_in_high_limb) {
      if (n_bits != 0) {
        primitive_left_shift(n_bits);
      }
      return;
    }

    int32_t new_len = int_len_ + n_ints + 1;
    if (n_bits <= 32 - bits_in_high_limb) {
      --new_len;
    }

    if (value_.length() < new_len) {
      jarray<uint32_t> result(new_len);
      for (int32_t i = 0; i < int_len_; ++i) {
        result[i] = value_[offset_ + i];
      }
      set_value(result, new_len);
    } else if (value_.length() - offset_ >= new_len) {
      value_.fill(offset_ + int_len_, offset_ + new_len, 0);
    } else {
      for (int32_t i = 0; i < int_len_; ++i) {
        value_[i] = value_[offset_ + i];
      }
      value_.fill(int_len_, new_len, 0);
      offset_ = 0;
    }

    int_len_ = new_len;
    if (n_bits == 0) {
      return;
    }
    if (n_bits <= 32 - bits_in_high_limb) {
      primitive_left_shift(n_bits);
    } else {
      primitive_right_shift(32 - n_bits);
    }
  }

  // 与 right_shift 类似，但允许 n 大于当前数的 bit 长度.
  // 如果右移覆盖了整个数，则直接重置为 0.
  void safe_right_shift(int32_t n) {
    if (n / 32 >= int_len_) {
      reset();
    } else {
      right_shift(n);
    }
  }

  // 与 left_shift 类似，但允许 n 为 0.
  void safe_left_shift(int32_t n) {
    if (n > 0) {
      left_shift(n);
    }
  }

  // 将 addend 加到当前对象中，结果保存在当前对象.
  // addend 的内容不会被修改.
  void add(const mutable_bigint& addend) {
    int32_t x = int_len_;
    int32_t y = addend.int_len_;
    int32_t result_len = std::max(int_len_, addend.int_len_);
    const bool result_is_value = value_.length() >= result_len;
    jarray<uint32_t> result = result_is_value ? value_ : jarray<uint32_t>(result_len);

    int32_t rstart = result.length() - 1;
    uint64_t sum = 0;
    uint64_t carry = 0;

    while (x > 0 && y > 0) {
      --x;
      --y;
      sum = (value_[x + offset_] & 0xffffffffULL) + (addend.value_[y + addend.offset_] & 0xffffffffULL) + carry;
      result[rstart--] = (uint32_t)(sum);
      carry = sum >> 32;
    }

    while (x > 0) {
      --x;
      if (carry == 0 && result_is_value && rstart == x + offset_) {
        value_ = result;
        return;
      }
      sum = (value_[x + offset_] & 0xffffffffULL) + carry;
      result[rstart--] = (uint32_t)(sum);
      carry = sum >> 32;
    }

    while (y > 0) {
      --y;
      sum = (addend.value_[y + addend.offset_] & 0xffffffffULL) + carry;
      result[rstart--] = (uint32_t)(sum);
      carry = sum >> 32;
    }

    if (carry > 0) {
      ++result_len;
      if (result.length() < result_len) {
        jarray<uint32_t> temp(result_len);
        jarray_copy(result, 0, temp, 1, result.length());
        temp[0] = 1;
        result = std::move(temp);
      } else {
        result[rstart--] = 1;
      }
    }

    value_ = std::move(result);
    int_len_ = result_len;
    offset_ = value_.length() - result_len;
  }

  // 将 addend 左移 32*n bit 后加到当前对象中，结果保存在当前对象.
  // 等价于先执行 addend.left_shift(32*n) 再 add(addend)，但不修改 addend.
  void add_shifted(const mutable_bigint& addend, int32_t n) {
    if (addend.is_zero()) {
      return;
    }

    int32_t x = int_len_;
    int32_t y = addend.int_len_ + n;
    int32_t result_len = std::max(int_len_, y);
    const bool result_is_value = value_.length() >= result_len;
    jarray<uint32_t> result = result_is_value ? value_ : jarray<uint32_t>(result_len);

    int32_t rstart = result.length() - 1;
    uint64_t sum = 0;
    uint64_t carry = 0;

    while (x > 0 && y > 0) {
      --x;
      --y;
      const uint32_t bval = y + addend.offset_ < addend.value_.length() ? addend.value_[y + addend.offset_] : 0;
      sum = (value_[x + offset_] & 0xffffffffULL) + (bval & 0xffffffffULL) + carry;
      result[rstart--] = (uint32_t)(sum);
      carry = sum >> 32;
    }

    while (x > 0) {
      --x;
      if (carry == 0 && result_is_value && rstart == x + offset_) {
        value_ = result;
        return;
      }
      sum = (value_[x + offset_] & 0xffffffffULL) + carry;
      result[rstart--] = (uint32_t)(sum);
      carry = sum >> 32;
    }

    while (y > 0) {
      --y;
      const uint32_t bval = y + addend.offset_ < addend.value_.length() ? addend.value_[y + addend.offset_] : 0;
      sum = (bval & 0xffffffffULL) + carry;
      result[rstart--] = (uint32_t)(sum);
      carry = sum >> 32;
    }

    if (carry > 0) {
      ++result_len;
      if (result.length() < result_len) {
        jarray<uint32_t> temp(result_len);
        jarray_copy(result, 0, temp, 1, result.length());
        temp[0] = 1;
        result = std::move(temp);
      } else {
        result[rstart--] = 1;
      }
    }

    value_ = std::move(result);
    int_len_ = result_len;
    offset_ = value_.length() - result_len;
  }

  // 类似 add_shifted(addend, n)，但要求当前数的有效长度不大于 n.
  // 即结果为 addend 放在高位、当前数放在低位，中间空出的 limb 补 0.
  void add_disjoint(const mutable_bigint& addend, int32_t n) {
    assert(int_len_ <= n);
    if (addend.is_zero()) {
      return;
    }

    int32_t x = int_len_;
    int32_t y = addend.int_len_ + n;
    const int32_t result_len = std::max(int_len_, y);
    jarray<uint32_t> result;
    if (value_.length() < result_len) {
      result = jarray<uint32_t>(result_len);
    } else {
      result = value_;
      result.fill(offset_ + int_len_, result.length(), 0);
    }

    int32_t rstart = result.length() - 1;

    jarray_copy(value_, offset_, result, rstart + 1 - x, x);
    y -= x;
    rstart -= x;

    const int32_t len = std::min(y, addend.value_.length() - addend.offset_);
    jarray_copy(addend.value_, addend.offset_, result, rstart + 1 - y, len);

    result.fill(rstart + 1 - y + len, rstart + 1, 0);

    value_ = std::move(result);
    int_len_ = result_len;
    offset_ = value_.length() - result_len;
  }

  // 用较大的数减去较小的数，并把结果保存在当前对象中.
  // 如果当前对象较大，返回 1；如果 b 较大，返回 -1；如果相等，返回 0.
  int32_t subtract(const mutable_bigint& b) {
    const mutable_bigint* a = this;
    const mutable_bigint* subtrahend = &b;
    const int32_t sign = compare(b);

    if (sign == 0) {
      reset();
      return 0;
    }
    if (sign < 0) {
      std::swap(a, subtrahend);
    }

    const int32_t result_len = a->int_len_;
    jarray<uint32_t> result = value_.length() < result_len ? jarray<uint32_t>(result_len) : value_;

    int32_t x = a->int_len_;
    int32_t y = subtrahend->int_len_;
    int32_t rstart = result.length() - 1;
    int32_t borrow = 0;

    while (y > 0) {
      --x;
      --y;
      const int64_t diff = (int64_t)(a->value_[x + a->offset_] & 0xffffffffULL) -
                           (int64_t)(subtrahend->value_[y + subtrahend->offset_] & 0xffffffffULL) - borrow;
      result[rstart--] = (uint32_t)diff;
      borrow = diff < 0 ? 1 : 0;
    }

    while (x > 0) {
      --x;
      const int64_t diff = (int64_t)(a->value_[x + a->offset_] & 0xffffffffULL) - borrow;
      result[rstart--] = (uint32_t)diff;
      borrow = diff < 0 ? 1 : 0;
    }

    value_ = std::move(result);
    int_len_ = result_len;
    offset_ = value_.length() - result_len;
    normalize();
    return sign;
  }

  // 将 addend 的低 n 个 limb 加到当前对象中，addend 本身不会被修改.
  void add_lower(const mutable_bigint& addend, int32_t n) {
    mutable_bigint a(addend);
    if (a.offset_ + a.int_len_ >= n) {
      a.offset_ = a.offset_ + a.int_len_ - n;
      a.int_len_ = n;
    }
    a.normalize();
    add(a);
  }

  // 将当前对象乘以 32-bit limb y，结果写入 z.
  void mul(uint32_t y, mutable_bigint& z) const {
    if (y == 1) {
      z.copy_value(*this);
      return;
    }

    if (y == 0) {
      z.clear();
      return;
    }

    const uint64_t ylong = y & 0xffffffffULL;
    jarray<uint32_t> zval = z.value_.length() < int_len_ + 1 ? jarray<uint32_t>(int_len_ + 1) : z.value_;
    uint64_t carry = 0;
    for (int32_t i = int_len_ - 1; i >= 0; --i) {
      const uint64_t product = ylong * (value_[i + offset_] & 0xffffffffULL) + carry;
      zval[i + 1] = (uint32_t)product;
      carry = product >> 32;
    }

    if (carry == 0) {
      z.offset_ = 1;
      z.int_len_ = int_len_;
    } else {
      z.offset_ = 0;
      z.int_len_ = int_len_ + 1;
      zval[0] = (uint32_t)carry;
    }
    z.value_ = std::move(zval);
  }

  // 将当前对象和 y 相乘，结果写入 z；y 本身不会被修改.
  void multiply(const mutable_bigint& y, mutable_bigint& z) const {
    const int32_t x_len = int_len_;
    const int32_t y_len = y.int_len_;
    const int32_t new_len = x_len + y_len;

    if (z.value_.length() < new_len) {
      z.value_ = jarray<uint32_t>(new_len);
    }
    z.offset_ = 0;
    z.int_len_ = new_len;

    uint64_t carry = 0;
    for (int32_t j = y_len - 1, k = y_len + x_len - 1; j >= 0; --j, --k) {
      const uint64_t product =
          (y.value_[j + y.offset_] & 0xffffffffULL) * (value_[x_len - 1 + offset_] & 0xffffffffULL) + carry;
      z.value_[k] = (uint32_t)product;
      carry = product >> 32;
    }
    z.value_[x_len - 1] = (uint32_t)carry;

    for (int32_t i = x_len - 2; i >= 0; --i) {
      carry = 0;
      for (int32_t j = y_len - 1, k = y_len + i; j >= 0; --j, --k) {
        const uint64_t product = (y.value_[j + y.offset_] & 0xffffffffULL) * (value_[i + offset_] & 0xffffffffULL) +
                                 (z.value_[k] & 0xffffffffULL) + carry;
        z.value_[k] = (uint32_t)product;
        carry = product >> 32;
      }
      z.value_[i] = (uint32_t)carry;
    }

    z.normalize();
  }
};
// static 常量定义
inline const mutable_bigint mutable_bigint::ONE{1};

// signed_mutable_bigint
// 带符号的可变多精度整数.
//
// 这个类只在 mutable_bigint 的基础上增加有符号加法和减法；其他运算仍然
// 按 mutable_bigint 的无符号绝对值逻辑执行，对应 Java SignedMutableBigInteger
// 的职责边界.
struct signed_mutable_bigint : mutable_bigint {
  // 默认构造函数.创建一个容量为 1 个 limb 的空 mutable_bigint，符号为正.
  signed_mutable_bigint() : mutable_bigint() {
  }

  // 使用一个 32 位 limb 构造数值，符号为正.
  signed_mutable_bigint(uint32_t val) : mutable_bigint(val) {
  }

  // 使用指定 mutable_bigint 的绝对值构造数值，符号为正.
  signed_mutable_bigint(const mutable_bigint& val) : mutable_bigint(val) {
  }

  // 显式声明拷贝构造，同时复制符号和有效数值.
  signed_mutable_bigint(const signed_mutable_bigint& val) : mutable_bigint(val), sign_(val.sign_) {
  }

  // 显式声明拷贝赋值，同时复制符号和有效数值.
  signed_mutable_bigint& operator=(const signed_mutable_bigint& val) {
    if (this == &val) {
      return *this;
    }
    mutable_bigint::operator=(val);
    sign_ = val.sign_;
    return *this;
  }

  // 显式声明移动构造，移动后源对象恢复为正零.
  signed_mutable_bigint(signed_mutable_bigint&& val) noexcept : mutable_bigint(std::move(val)), sign_(val.sign_) {
    val.sign_ = 1;
  }

  // 显式声明移动赋值，移动后源对象恢复为正零.
  signed_mutable_bigint& operator=(signed_mutable_bigint&& val) noexcept {
    if (this == &val) {
      return *this;
    }
    mutable_bigint::operator=(std::move(val));
    sign_ = val.sign_;
    val.sign_ = 1;
    return *this;
  }

  // 返回当前符号.1 表示正，-1 表示负，0 表示零结果的临时符号.
  int32_t sign() const {
    return sign_;
  }

  // 设置当前符号.只允许 -1、0、1，便于测试和后续算法构造中间状态.
  void set_sign(int32_t sign) {
    assert(sign >= -1 && sign <= 1);
    sign_ = sign;
  }

  // 基于无符号 add/subtract 实现有符号加法.
  void signed_add(const signed_mutable_bigint& addend) {
    if (sign_ == addend.sign_) {
      add(addend);
    } else {
      sign_ = sign_ * subtract(addend);
    }
  }

  // 基于无符号 add/subtract 实现有符号加法，addend 按正数处理.
  void signed_add(const mutable_bigint& addend) {
    if (sign_ == 1) {
      add(addend);
    } else {
      sign_ = sign_ * subtract(addend);
    }
  }

  // 基于无符号 add/subtract 实现有符号减法.
  void signed_subtract(const signed_mutable_bigint& addend) {
    if (sign_ == addend.sign_) {
      sign_ = sign_ * subtract(addend);
    } else {
      add(addend);
    }
  }

  // 基于无符号 add/subtract 实现有符号减法，addend 按正数处理.
  // 与 Java 实现一致，如果结果为 0，则把符号恢复为正.
  void signed_subtract(const mutable_bigint& addend) {
    if (sign_ == 1) {
      sign_ = sign_ * subtract(addend);
    } else {
      add(addend);
    }
    if (is_zero()) {
      sign_ = 1;
    }
  }

  int32_t sign_{1};
};

// mutable_bigint (out-of-class definitions)
inline mutable_bigint mutable_bigint::mutable_mod_inverse(const mutable_bigint& p) {
  if (p.is_odd()) {
    return mod_inverse(p);
  }

  if (is_even()) {
    throw std::runtime_error("BigInteger not invertible.");
  }

  const int32_t powers_of_2 = p.get_lowest_set_bit();

  mutable_bigint odd_mod(p);
  odd_mod.right_shift(powers_of_2);

  if (odd_mod.is_one()) {
    return mod_inverse_mp2(powers_of_2);
  }

  mutable_bigint odd_part = mod_inverse(odd_mod);
  mutable_bigint even_part = mod_inverse_mp2(powers_of_2);

  mutable_bigint y1 = mod_inverse_bp2(odd_mod, powers_of_2);
  mutable_bigint y2 = odd_mod.mod_inverse_mp2(powers_of_2);

  mutable_bigint temp1;
  mutable_bigint temp2;
  mutable_bigint result;

  odd_part.left_shift(powers_of_2);
  odd_part.multiply(y1, result);
  odd_part.clear();

  even_part.multiply(odd_mod, temp1);
  temp1.multiply(y2, temp2);

  result.add(temp2);
  return result.divide(p, temp1);
}

inline mutable_bigint mutable_bigint::mod_inverse(const mutable_bigint& mod) {
  mutable_bigint p(mod);
  mutable_bigint f(*this);
  mutable_bigint g(p);
  signed_mutable_bigint c(1);
  signed_mutable_bigint d;

  int32_t k = 0;
  if (f.is_even()) {
    const int32_t trailing_zeros = f.get_lowest_set_bit();
    f.right_shift(trailing_zeros);
    d.left_shift(trailing_zeros);
    k = trailing_zeros;
  }

  while (!f.is_one()) {
    if (f.is_zero()) {
      throw std::runtime_error("BigInteger not invertible.");
    }

    if (f.compare(g) < 0) {
      std::swap(f, g);
      std::swap(c, d);
    }

    if (((f.value_[f.offset_ + f.int_len_ - 1] ^ g.value_[g.offset_ + g.int_len_ - 1]) & 3U) == 0) {
      f.subtract(g);
      c.signed_subtract(d);
    } else {
      f.add(g);
      c.signed_add(d);
    }

    const int32_t trailing_zeros = f.get_lowest_set_bit();
    f.right_shift(trailing_zeros);
    d.left_shift(trailing_zeros);
    k += trailing_zeros;
  }

  if (c.compare(p) >= 0) {
    mutable_bigint q;
    mutable_bigint remainder = c.divide(p, q);
    c.copy_value(remainder);
  }

  if (c.sign() < 0) {
    c.signed_add(p);
  }

  return fixup(c, p, k);
}

// bigint
// 不可变的有符号多精度整数，对应 Java BigInteger.
//
// mag_ 使用大端 uint32_t limb 保存绝对值，保持规范形式：零必须是
// signum_ == 0 且 mag_.length() == 0；非零数的 mag_[0] 必须非零.
// 这个 struct 按当前项目要求保持全公有，内部辅助函数也放在 public 区域.
struct bigint {
  static constexpr int32_t MIN_RADIX = 2;
  static constexpr int32_t MAX_RADIX = 36;
  static constexpr int32_t MAX_CONSTANT = 16;
  static constexpr int32_t MAX_MAG_LENGTH = std::numeric_limits<int32_t>::max() / 32 + 1;
  static constexpr int32_t PRIME_SEARCH_BIT_LENGTH_LIMIT = 500000000;
  static constexpr int32_t KARATSUBA_THRESHOLD = 80;
  static constexpr int32_t TOOM_COOK_THRESHOLD = 240;
  static constexpr int32_t KARATSUBA_SQUARE_THRESHOLD = 128;
  static constexpr int32_t TOOM_COOK_SQUARE_THRESHOLD = 216;
  static constexpr int32_t SMALL_PRIME_THRESHOLD = 95;
  static constexpr int32_t DEFAULT_PRIME_CERTAINTY = 100;
  static constexpr int32_t MULTIPLY_SQUARE_THRESHOLD = 20;
  static constexpr int32_t MONTGOMERY_INTRINSIC_THRESHOLD = 512;
  static constexpr int32_t SCHOENHAGE_BASE_CONVERSION_THRESHOLD = 20;
  static constexpr int32_t NUM_ZEROS = 63;
  static constexpr double LOG_TWO = 0.693147180559945309417232121458176568;
  static constexpr std::array<int32_t, 7> bn_exp_mod_thresh_table = {
      7, 25, 81, 241, 673, 1793, std::numeric_limits<int32_t>::max()};
  static const bigint ZERO;
  static const bigint ONE;
  static const bigint TWO;
  static const bigint TEN;
  static const bigint NEGATIVE_ONE;

  static constexpr std::array<uint64_t, 37> bits_per_digit = {
      0,    0,    1024, 1624, 2048, 2378, 2648, 2875, 3072, 3247, 3402, 3543, 3672, 3790, 3899, 4001, 4096, 4186, 4271,
      4350, 4426, 4498, 4567, 4633, 4696, 4756, 4814, 4870, 4923, 4975, 5025, 5074, 5120, 5166, 5210, 5253, 5295};

  static constexpr std::array<int32_t, 37> digits_per_int = {0, 0, 30, 19, 15, 13, 11, 11, 10, 9, 9, 8, 8,
                                                             8, 8, 7,  7,  7,  7,  7,  7,  7,  6, 6, 6, 6,
                                                             6, 6, 6,  6,  6,  6,  6,  6,  6,  6, 5};

  static constexpr std::array<int32_t, 37> digits_per_long = {0,  0,  62, 39, 31, 27, 24, 22, 20, 19, 18, 18, 17,
                                                              17, 16, 16, 15, 15, 15, 14, 14, 14, 14, 13, 13, 13,
                                                              13, 13, 13, 12, 12, 12, 12, 12, 12, 12, 12};

  static constexpr std::array<uint32_t, 37> int_radix = {
      0,           0,           0x40000000U, 0x4546b3dbU, 0x40000000U, 0x48c27395U, 0x159fd800U, 0x75db9c97U,
      0x40000000U, 0x17179149U, 0x3b9aca00U, 0x0cc6db61U, 0x19a10000U, 0x309f1021U, 0x57f6c100U, 0x0a2f1b6fU,
      0x10000000U, 0x18754571U, 0x247dbc80U, 0x3547667bU, 0x4c4b4000U, 0x6b5a6e1dU, 0x06c20a40U, 0x08d2d931U,
      0x0b640000U, 0x0e8d4a51U, 0x1269ae40U, 0x17179149U, 0x1cb91000U, 0x23744899U, 0x2b73a840U, 0x34e63b41U,
      0x40000000U, 0x4cfa3cc1U, 0x5c13d840U, 0x6d91b519U, 0x039aa400U,
  };

  int32_t signum_{0};
  jarray<uint32_t> mag_{};
  mutable int32_t bit_count_plus_one_{0};
  mutable int32_t bit_length_plus_one_{0};
  mutable int32_t lowest_set_bit_plus_two_{0};
  mutable int32_t first_nonzero_int_num_plus_two_{0};

  // 创建 BigInteger 常量 0.
  bigint() = default;

  // 使用已规范化的 sign-magnitude limb 数组构造内部值.
  bigint(int32_t signum, jarray<uint32_t> magnitude)
      : signum_(signum), mag_(trusted_strip_leading_zero_limbs(magnitude)) {
    if (signum < -1 || signum > 1) {
      throw std::invalid_argument("Invalid signum value");
    }
    if (mag_.length() == 0) {
      signum_ = 0;
    } else if (signum == 0) {
      throw std::invalid_argument("signum-magnitude mismatch");
    }
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 从大端二进制补码 limb 数组构造；对应 Java BigInteger(int[] val).
  bigint(const jarray<uint32_t>& val) {
    if (val.length() == 0) {
      signum_ = 0;
      return;
    }
    if ((val[0] & 0x80000000U) != 0) {
      mag_ = make_positive_limbs(val);
      signum_ = -1;
    } else {
      mag_ = trusted_strip_leading_zero_limbs(val);
      signum_ = mag_.length() == 0 ? 0 : 1;
    }
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 将 int64_t 转换为 BigInteger；对应 Java 的私有 long 构造器.
  bigint(int64_t val) {
    if (val == 0) {
      signum_ = 0;
      return;
    }

    uint64_t abs_val = 0;
    if (val < 0) {
      signum_ = -1;
      abs_val = uint64_t(0) - static_cast<uint64_t>(val);
    } else {
      signum_ = 1;
      abs_val = static_cast<uint64_t>(val);
    }

    const uint32_t high_word = static_cast<uint32_t>(abs_val >> 32);
    if (high_word == 0) {
      mag_ = jarray<uint32_t>(1);
      mag_[0] = static_cast<uint32_t>(abs_val);
    } else {
      mag_ = jarray<uint32_t>(2);
      mag_[0] = high_word;
      mag_[1] = static_cast<uint32_t>(abs_val);
    }
  }

  // 将大端二进制补码 byte 子数组转换为 BigInteger.
  bigint(const jarray<uint8_t>& val, int32_t off, int32_t len) {
    if (val.length() == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }
    check_from_index_size(off, len, val.length());
    if (len == 0) {
      signum_ = 0;
      return;
    }

    const int32_t b = static_cast<int8_t>(val[off]);
    if (b < 0) {
      mag_ = make_positive_bytes(b, val, off, len);
      signum_ = -1;
    } else {
      mag_ = strip_leading_zero_bytes(b, val, off, len);
      signum_ = mag_.length() == 0 ? 0 : 1;
    }
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 将完整大端二进制补码 byte 数组转换为 BigInteger.
  bigint(const jarray<uint8_t>& val) : bigint(val, 0, val.length()) {
  }

  // 将 sign-magnitude byte 子数组转换为 BigInteger.
  bigint(int32_t signum, const jarray<uint8_t>& magnitude, int32_t off, int32_t len) {
    if (signum < -1 || signum > 1) {
      throw std::invalid_argument("Invalid signum value");
    }
    check_from_index_size(off, len, magnitude.length());
    mag_ = strip_leading_zero_bytes(magnitude, off, len);
    if (mag_.length() == 0) {
      signum_ = 0;
    } else {
      if (signum == 0) {
        throw std::invalid_argument("signum-magnitude mismatch");
      }
      signum_ = signum;
    }
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 将完整 sign-magnitude byte 数组转换为 BigInteger.
  bigint(int32_t signum, const jarray<uint8_t>& magnitude) : bigint(signum, magnitude, 0, magnitude.length()) {
  }

  // 内部 sign-magnitude byte 构造；对应 Java BigInteger(byte[] magnitude, int signum).
  bigint(const jarray<uint8_t>& magnitude, int32_t signum) {
    signum_ = magnitude.length() == 0 ? 0 : signum;
    mag_ = strip_leading_zero_bytes(magnitude, 0, magnitude.length());
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 生成 [0, 2^numBits) 的非负随机 BigInteger；对应 Java BigInteger(numBits, rnd).
  bigint(int32_t num_bits, std::mt19937_64& rnd) {
    jarray<uint8_t> magnitude = random_bits(num_bits, rnd);
    mag_ = strip_leading_zero_bytes(magnitude, 0, magnitude.length());
    signum_ = mag_.length() == 0 ? 0 : 1;
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
    magnitude.fill(0);
  }

  // 随机生成指定 bitLength 的 probable prime；对应 Java BigInteger(bitLength, certainty, rnd).
  bigint(int32_t bit_length, int32_t certainty, std::mt19937_64& rnd) {
    if (bit_length < 2) {
      throw std::runtime_error("bitLength < 2");
    }
    const bigint prime = bit_length < SMALL_PRIME_THRESHOLD ? small_prime(bit_length, certainty, rnd)
                                                            : large_prime(bit_length, certainty, rnd);
    signum_ = 1;
    mag_ = prime.mag_;
  }

  // 将指定 radix 的字符串转换为 BigInteger.
  bigint(const std::string& val, int32_t radix) {
    int32_t cursor = 0;
    const int32_t len = static_cast<int32_t>(val.size());

    if (radix < MIN_RADIX || radix > MAX_RADIX) {
      throw std::invalid_argument("Radix out of range");
    }
    if (len == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }

    int32_t sign = 1;
    const std::size_t index1 = val.rfind('-');
    const std::size_t index2 = val.rfind('+');
    if (index1 != std::string::npos) {
      if (index1 != 0 || index2 != std::string::npos) {
        throw std::invalid_argument("Illegal embedded sign character");
      }
      sign = -1;
      cursor = 1;
    } else if (index2 != std::string::npos) {
      if (index2 != 0) {
        throw std::invalid_argument("Illegal embedded sign character");
      }
      cursor = 1;
    }
    if (cursor == len) {
      throw std::invalid_argument("Zero length BigInteger");
    }

    while (cursor < len && digit(val[static_cast<std::size_t>(cursor)], radix) == 0) {
      ++cursor;
    }
    if (cursor == len) {
      signum_ = 0;
      return;
    }

    const int32_t num_digits = len - cursor;
    signum_ = sign;
    const uint64_t num_bits = ((uint64_t(num_digits) * bits_per_digit[radix]) >> 10) + 1;
    if (num_bits + 31 >= (1ULL << 32)) {
      report_overflow();
    }
    const int32_t num_words = static_cast<int32_t>((num_bits + 31) >> 5);
    jarray<uint32_t> magnitude(num_words);

    int32_t first_group_len = num_digits % digits_per_int[radix];
    if (first_group_len == 0) {
      first_group_len = digits_per_int[radix];
    }
    magnitude[num_words - 1] = parse_group(val, cursor, cursor + first_group_len, radix);
    cursor += first_group_len;

    const uint32_t super_radix = int_radix[radix];
    while (cursor < len) {
      const uint32_t group_val = parse_group(val, cursor, cursor + digits_per_int[radix], radix);
      cursor += digits_per_int[radix];
      destructive_mul_add(magnitude, super_radix, group_val);
    }

    mag_ = trusted_strip_leading_zero_limbs(magnitude);
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 将十进制字符串转换为 BigInteger.
  bigint(const std::string& val) : bigint(val, 10) {
  }

  // 使用预先解析出的符号构造十进制 BigInteger；对应 Java BigInteger(char[] val, int sign, int len).
  bigint(const std::string& val, int32_t sign, int32_t len) {
    int32_t cursor = 0;
    while (cursor < len && digit(val[static_cast<std::size_t>(cursor)], 10) == 0) {
      ++cursor;
    }
    if (cursor == len) {
      signum_ = 0;
      mag_ = ZERO.mag_;
      return;
    }

    const int32_t num_digits = len - cursor;
    signum_ = sign;
    int32_t num_words = 0;
    if (len < 10) {
      num_words = 1;
    } else {
      const uint64_t num_bits = ((uint64_t(num_digits) * bits_per_digit[10]) >> 10) + 1;
      if (num_bits + 31 >= (uint64_t{1} << 32)) {
        report_overflow();
      }
      num_words = static_cast<int32_t>(num_bits + 31) >> 5;
    }

    jarray<uint32_t> magnitude(num_words);
    int32_t first_group_len = num_digits % digits_per_int[10];
    if (first_group_len == 0) {
      first_group_len = digits_per_int[10];
    }
    magnitude[num_words - 1] = parse_int_decimal(val, cursor, cursor + first_group_len);
    cursor += first_group_len;

    while (cursor < len) {
      const uint32_t group_val = parse_int_decimal(val, cursor, cursor + digits_per_int[10]);
      cursor += digits_per_int[10];
      destructive_mul_add(magnitude, int_radix[10], group_val);
    }
    mag_ = trusted_strip_leading_zero_limbs(magnitude);
    if (mag_.length() >= MAX_MAG_LENGTH) {
      check_range();
    }
  }

  // 返回等于指定 int64_t 的 BigInteger；对应 Java valueOf(long).
  static bigint value_of(int64_t val) {
    if (val == 0) {
      return ZERO;
    }
    if (val > 0 && val <= MAX_CONSTANT) {
      return pos_const(static_cast<int32_t>(val));
    }
    if (val < 0 && val >= -MAX_CONSTANT) {
      return neg_const(static_cast<int32_t>(-val));
    }
    return bigint(val);
  }

  // 正小整数缓存入口；对应 Java posConst.
  static bigint pos_const(int32_t n);

  // 负小整数缓存入口；对应 Java negConst.
  static bigint neg_const(int32_t n);

  // 从大端二进制补码 limb 数组创建 BigInteger.
  static bigint value_of(const jarray<uint32_t>& val) {
    if (val.length() == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }
    return (val[0] & 0x80000000U) == 0 ? bigint(1, val.clone()) : from_twos_complement(val);
  }

  // 从大端二进制补码 limb 数组转换为 BigInteger.
  static bigint from_twos_complement(const jarray<uint32_t>& val) {
    if (val.length() == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }
    if ((val[0] & 0x80000000U) != 0) {
      return bigint(-1, make_positive_limbs(val));
    }
    return bigint(1, trusted_strip_leading_zero_limbs(val));
  }

  // 返回符号：-1、0 或 1.
  int32_t signum() const {
    return signum_;
  }

  bool is_zero() const {
    return signum_ == 0;
  }

  bool is_even() const {
    return signum_ == 0 || ((mag_[mag_.length() - 1] & 1U) == 0);
  }

  bool is_odd() const {
    return signum_ != 0 && ((mag_[mag_.length() - 1] & 1U) != 0);
  }

  // 比较两个 BigInteger 的数值大小.
  int32_t compare_to(const bigint& val) const {
    if (signum_ == val.signum_) {
      if (signum_ > 0) {
        return compare_magnitude(val);
      }
      if (signum_ < 0) {
        return val.compare_magnitude(*this);
      }
      return 0;
    }
    return signum_ > val.signum_ ? 1 : -1;
  }

  // 只比较绝对值大小.
  int32_t compare_magnitude(const bigint& val) const {
    const int32_t len1 = mag_.length();
    const int32_t len2 = val.mag_.length();
    if (len1 < len2) {
      return -1;
    }
    if (len1 > len2) {
      return 1;
    }
    for (int32_t i = 0; i < len1; ++i) {
      if (mag_[i] < val.mag_[i]) {
        return -1;
      }
      if (mag_[i] > val.mag_[i]) {
        return 1;
      }
    }
    return 0;
  }

  // 比较绝对值与 int64_t 的绝对值大小；调用方不得传入 INT64_MIN.
  int32_t compare_magnitude(int64_t val) const {
    assert(val != std::numeric_limits<int64_t>::min());
    const int32_t len = mag_.length();
    if (len > 2) {
      return 1;
    }
    uint64_t abs_val = val < 0 ? uint64_t(0) - static_cast<uint64_t>(val) : static_cast<uint64_t>(val);
    const uint32_t high_word = static_cast<uint32_t>(abs_val >> 32);
    if (high_word == 0) {
      if (len < 1) {
        return -1;
      }
      if (len > 1) {
        return 1;
      }
      const uint32_t low = static_cast<uint32_t>(abs_val);
      if (mag_[0] != low) {
        return mag_[0] < low ? -1 : 1;
      }
      return 0;
    }
    if (len < 2) {
      return -1;
    }
    if (mag_[0] != high_word) {
      return mag_[0] < high_word ? -1 : 1;
    }
    const uint32_t low = static_cast<uint32_t>(abs_val);
    if (mag_[1] != low) {
      return mag_[1] < low ? -1 : 1;
    }
    return 0;
  }

  bool operator==(const bigint& other) const {
    if (signum_ != other.signum_ || mag_.length() != other.mag_.length()) {
      return false;
    }
    for (int32_t i = 0; i < mag_.length(); ++i) {
      if (mag_[i] != other.mag_[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const bigint& other) const {
    return !(*this == other);
  }

  bool operator<(const bigint& other) const {
    return compare_to(other) < 0;
  }

  bool operator>(const bigint& other) const {
    return compare_to(other) > 0;
  }

  bool operator<=(const bigint& other) const {
    return compare_to(other) <= 0;
  }

  bool operator>=(const bigint& other) const {
    return compare_to(other) >= 0;
  }

  // 返回两者中的较小值；相等时允许返回任意一方.
  bigint min(const bigint& val) const {
    return compare_to(val) < 0 ? *this : val;
  }

  // 返回两者中的较大值；相等时允许返回任意一方.
  bigint max(const bigint& val) const {
    return compare_to(val) > 0 ? *this : val;
  }

  // 返回 Java BigInteger#hashCode 对应的哈希值.
  int32_t hash_code() const {
    int32_t hash = 0;
    for (int32_t i = 0; i < mag_.length(); ++i) {
      hash = static_cast<int32_t>(31 * hash + mag_[i]);
    }
    return hash * signum_;
  }

  // 返回绝对值.
  bigint abs() const {
    return signum_ >= 0 ? *this : negate();
  }

  // 返回相反数.
  bigint negate() const {
    return bigint(-signum_, mag_.clone());
  }

  bigint operator-() const {
    return negate();
  }

  // 返回 this + val.
  bigint add(const bigint& val) const {
    if (val.signum_ == 0) {
      return *this;
    }
    if (signum_ == 0) {
      return val;
    }
    if (val.signum_ == signum_) {
      return bigint(signum_, add_magnitude(mag_, val.mag_));
    }

    const int32_t cmp = compare_magnitude(val);
    if (cmp == 0) {
      return bigint();
    }
    jarray<uint32_t> result_mag = cmp > 0 ? subtract_magnitude(mag_, val.mag_) : subtract_magnitude(val.mag_, mag_);
    result_mag = trusted_strip_leading_zero_limbs(result_mag);
    return bigint(cmp == signum_ ? 1 : -1, std::move(result_mag));
  }

  // 返回 this + val，其中 val 是普通 int64_t.
  bigint add(int64_t val) const {
    if (val == 0) {
      return *this;
    }
    if (signum_ == 0) {
      return value_of(val);
    }
    const int32_t val_sign = (val > 0) - (val < 0);
    const uint64_t abs_val = val < 0 ? uint64_t(0) - static_cast<uint64_t>(val) : static_cast<uint64_t>(val);
    if (val_sign == signum_) {
      return bigint(signum_, add_magnitude(mag_, abs_val));
    }
    const int32_t cmp = compare_magnitude(val);
    if (cmp == 0) {
      return bigint();
    }
    jarray<uint32_t> result_mag = cmp > 0 ? subtract_magnitude(mag_, abs_val) : subtract_magnitude(abs_val, mag_);
    result_mag = trusted_strip_leading_zero_limbs(result_mag);
    return bigint(cmp == signum_ ? 1 : -1, std::move(result_mag));
  }

  bigint operator+(const bigint& val) const {
    return add(val);
  }

  // 返回 this - val.
  bigint subtract(const bigint& val) const {
    if (val.signum_ == 0) {
      return *this;
    }
    if (signum_ == 0) {
      return val.negate();
    }
    if (val.signum_ != signum_) {
      return bigint(signum_, add_magnitude(mag_, val.mag_));
    }

    const int32_t cmp = compare_magnitude(val);
    if (cmp == 0) {
      return bigint();
    }
    jarray<uint32_t> result_mag = cmp > 0 ? subtract_magnitude(mag_, val.mag_) : subtract_magnitude(val.mag_, mag_);
    result_mag = trusted_strip_leading_zero_limbs(result_mag);
    return bigint(cmp == signum_ ? 1 : -1, std::move(result_mag));
  }

  bigint operator-(const bigint& val) const {
    return subtract(val);
  }

  // 返回 this * val.
  bigint multiply(const bigint& val) const {
    return multiply(val, false);
  }

  // 返回 this * val；递归调用时跳过部分溢出检查.
  bigint multiply(const bigint& val, bool is_recursion) const {
    if (val.signum_ == 0 || signum_ == 0) {
      return ZERO;
    }

    const int32_t xlen = mag_.length();
    if (this == &val && xlen > MULTIPLY_SQUARE_THRESHOLD) {
      return square();
    }

    const int32_t ylen = val.mag_.length();
    if ((xlen < KARATSUBA_THRESHOLD) || (ylen < KARATSUBA_THRESHOLD)) {
      const int32_t result_sign = signum_ == val.signum_ ? 1 : -1;
      if (val.mag_.length() == 1) {
        return multiply_by_int(mag_, val.mag_[0], result_sign);
      }
      if (mag_.length() == 1) {
        return multiply_by_int(val.mag_, mag_[0], result_sign);
      }
      jarray<uint32_t> result = multiply_to_len(mag_, xlen, val.mag_, ylen, nullptr);
      result = trusted_strip_leading_zero_limbs(result);
      return bigint(result_sign, std::move(result));
    }

    if ((xlen < TOOM_COOK_THRESHOLD) && (ylen < TOOM_COOK_THRESHOLD)) {
      return multiply_karatsuba(*this, val);
    }

    if (!is_recursion) {
      if (static_cast<int64_t>(bit_length(mag_, mag_.length())) +
              static_cast<int64_t>(bit_length(val.mag_, val.mag_.length())) >
          32LL * MAX_MAG_LENGTH) {
        report_overflow();
      }
    }
    return multiply_toom_cook3(*this, val);
  }

  bigint operator*(const bigint& val) const {
    return multiply(val);
  }

  // 返回 this * v，其中 v 是普通 int64_t.
  bigint multiply(int64_t v) const {
    if (v == 0 || signum_ == 0) {
      return ZERO;
    }

    int32_t rsign = v > 0 ? signum_ : -signum_;
    uint64_t abs_v = v < 0 ? uint64_t(0) - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
    const uint64_t dh = abs_v >> 32;
    const uint64_t dl = abs_v & 0xffffffffULL;

    const int32_t xlen = mag_.length();
    const jarray<uint32_t>& value = mag_;
    jarray<uint32_t> rmag(dh == 0 ? xlen + 1 : xlen + 2);
    uint64_t carry = 0;
    int32_t rstart = rmag.length() - 1;
    for (int32_t i = xlen - 1; i >= 0; --i) {
      const uint64_t product = (value[i] & 0xffffffffULL) * dl + carry;
      rmag[rstart--] = static_cast<uint32_t>(product);
      carry = product >> 32;
    }
    rmag[rstart] = static_cast<uint32_t>(carry);
    if (dh != 0) {
      carry = 0;
      rstart = rmag.length() - 2;
      for (int32_t i = xlen - 1; i >= 0; --i) {
        const uint64_t product = (value[i] & 0xffffffffULL) * dh + (rmag[rstart] & 0xffffffffULL) + carry;
        rmag[rstart--] = static_cast<uint32_t>(product);
        carry = product >> 32;
      }
      rmag[0] = static_cast<uint32_t>(carry);
    }
    if (carry == 0) {
      rmag = rmag.copy_of_range(1, rmag.length());
    }
    return bigint(rsign, std::move(rmag));
  }

  // 返回低 n 个 limb 组成的新 BigInteger.
  bigint get_lower(int32_t n) const {
    const int32_t len = mag_.length();
    if (len <= n) {
      return abs();
    }

    jarray<uint32_t> lower_ints(n);
    jarray_copy(mag_, len - n, lower_ints, 0, n);
    return bigint(1, trusted_strip_leading_zero_limbs(lower_ints));
  }

  // 返回高 mag.length-n 个 limb 组成的新 BigInteger.
  bigint get_upper(int32_t n) const {
    const int32_t len = mag_.length();
    if (len <= n) {
      return ZERO;
    }

    const int32_t upper_len = len - n;
    jarray<uint32_t> upper_ints(upper_len);
    jarray_copy(mag_, 0, upper_ints, 0, upper_len);
    return bigint(1, trusted_strip_leading_zero_limbs(upper_ints));
  }

  // 返回 Toom-Cook 使用的切片.
  bigint get_toom_slice(int32_t lower_size, int32_t upper_size, int32_t slice, int32_t fullsize) const {
    int32_t start = 0;
    int32_t end = 0;
    int32_t slice_size = 0;
    const int32_t len = mag_.length();
    const int32_t offset = fullsize - len;

    if (slice == 0) {
      start = 0 - offset;
      end = upper_size - 1 - offset;
    } else {
      start = upper_size + (slice - 1) * lower_size - offset;
      end = start + lower_size - 1;
    }

    if (start < 0) {
      start = 0;
    }
    if (end < 0) {
      return ZERO;
    }

    slice_size = (end - start) + 1;
    if (slice_size <= 0) {
      return ZERO;
    }

    if (start == 0 && slice_size >= len) {
      return abs();
    }

    jarray<uint32_t> int_slice(slice_size);
    jarray_copy(mag_, start, int_slice, 0, slice_size);
    return bigint(1, trusted_strip_leading_zero_limbs(int_slice));
  }

  // 精确除以 3；用于 Toom-Cook.
  bigint exact_divide_by3() const {
    const int32_t len = mag_.length();
    jarray<uint32_t> result(len);
    uint64_t borrow = 0;
    for (int32_t i = len - 1; i >= 0; --i) {
      const uint64_t x = mag_[i] & 0xffffffffULL;
      const uint64_t w = x - borrow;
      if (borrow > x) {
        borrow = 1;
      } else {
        borrow = 0;
      }

      const uint64_t q = (w * 0xaaaaaaabULL) & 0xffffffffULL;
      result[i] = static_cast<uint32_t>(q);

      if (q >= 0x55555556ULL) {
        ++borrow;
        if (q >= 0xaaaaaaabULL) {
          ++borrow;
        }
      }
    }
    result = trusted_strip_leading_zero_limbs(result);
    return bigint(signum_, std::move(result));
  }

  // 返回 this / val；按 Java 阈值选择 Knuth 或 Burnikel-Ziegler.
  bigint divide(const bigint& val) const {
    if (val.signum_ == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }
    if (val.mag_.length() < mutable_bigint::BURNIKEL_ZIEGLER_THRESHOLD ||
        mag_.length() - val.mag_.length() < mutable_bigint::BURNIKEL_ZIEGLER_OFFSET) {
      return divide_knuth(val);
    }
    return divide_burnikel_ziegler(val);
  }

  // Knuth O(n^2) 除法；对应 Java divideKnuth.
  bigint divide_knuth(const bigint& val) const {
    mutable_bigint q;
    mutable_bigint a(mag_);
    mutable_bigint b(val.mag_);
    a.divide_knuth(b, q, false);
    return from_mutable(std::move(q), signum_ * val.signum_);
  }

  bigint operator/(const bigint& val) const {
    return divide(val);
  }

  // 返回 this / val 的商和余数；按 Java 阈值选择算法.
  std::pair<bigint, bigint> divide_and_remainder(const bigint& val) const {
    if (val.signum_ == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }
    if (val.mag_.length() < mutable_bigint::BURNIKEL_ZIEGLER_THRESHOLD ||
        mag_.length() - val.mag_.length() < mutable_bigint::BURNIKEL_ZIEGLER_OFFSET) {
      return divide_and_remainder_knuth(val);
    }
    return divide_and_remainder_burnikel_ziegler(val);
  }

  // Knuth 长除法商余；对应 Java divideAndRemainderKnuth.
  std::pair<bigint, bigint> divide_and_remainder_knuth(const bigint& val) const {
    mutable_bigint q;
    mutable_bigint a(mag_);
    mutable_bigint b(val.mag_);
    mutable_bigint r = a.divide_knuth(b, q);
    return {from_mutable(std::move(q), signum_ == val.signum_ ? 1 : -1), from_mutable(std::move(r), signum_)};
  }

  // 返回 this % val；按 Java 阈值选择算法.
  bigint remainder(const bigint& val) const {
    if (val.signum_ == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }
    if (val.mag_.length() < mutable_bigint::BURNIKEL_ZIEGLER_THRESHOLD ||
        mag_.length() - val.mag_.length() < mutable_bigint::BURNIKEL_ZIEGLER_OFFSET) {
      return remainder_knuth(val);
    }
    return remainder_burnikel_ziegler(val);
  }

  // Knuth 长除法余数；对应 Java remainderKnuth.
  bigint remainder_knuth(const bigint& val) const {
    mutable_bigint q;
    mutable_bigint a(mag_);
    mutable_bigint b(val.mag_);
    mutable_bigint r = a.divide_knuth(b, q);
    return from_mutable(std::move(r), signum_);
  }

  // Burnikel-Ziegler 除法商；对应 Java divideBurnikelZiegler.
  bigint divide_burnikel_ziegler(const bigint& val) const {
    return divide_and_remainder_burnikel_ziegler(val).first;
  }

  // Burnikel-Ziegler 除法余数；对应 Java remainderBurnikelZiegler.
  bigint remainder_burnikel_ziegler(const bigint& val) const {
    return divide_and_remainder_burnikel_ziegler(val).second;
  }

  // Burnikel-Ziegler 商余；对应 Java divideAndRemainderBurnikelZiegler.
  std::pair<bigint, bigint> divide_and_remainder_burnikel_ziegler(const bigint& val) const {
    mutable_bigint q;
    mutable_bigint a(mag_);
    mutable_bigint b(val.mag_);
    mutable_bigint r = a.divide_and_remainder_burnikel_ziegler(b, q);
    const bigint q_bigint = q.is_zero() ? ZERO : from_mutable(std::move(q), signum_ * val.signum_);
    const bigint r_bigint = r.is_zero() ? ZERO : from_mutable(std::move(r), signum_);
    return {q_bigint, r_bigint};
  }

  bigint operator%(const bigint& val) const {
    return remainder(val);
  }

  // 返回 |this| mod m；供 BitSieve 构造搜索筛时对应 MutableBigInteger.divideOneWord 使用.
  uint32_t mod_uint32(uint32_t m) const {
    if (m == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }
    uint64_t rem = 0;
    for (int32_t i = 0; i < mag_.length(); ++i) {
      rem = ((rem << 32) | (mag_[i] & 0xffffffffULL)) % m;
    }
    return static_cast<uint32_t>(rem);
  }

  // 返回 this 的 exponent 次方，exponent 必须非负.
  bigint pow(int32_t exponent) const {
    if (exponent < 0) {
      throw std::runtime_error("Negative exponent");
    }
    if (signum_ == 0) {
      return exponent == 0 ? ONE : *this;
    }

    bigint part_to_square = abs();

    const int32_t powers_of_two = part_to_square.get_lowest_set_bit();
    const int64_t bits_to_shift_long = static_cast<int64_t>(powers_of_two) * exponent;
    if (bits_to_shift_long > std::numeric_limits<int32_t>::max()) {
      report_overflow();
    }
    const int32_t bits_to_shift = static_cast<int32_t>(bits_to_shift_long);

    int32_t remaining_bits = 0;
    if (powers_of_two > 0) {
      part_to_square = part_to_square.shift_right(powers_of_two);
      remaining_bits = part_to_square.bit_length();
      if (remaining_bits == 1) {
        if (signum_ < 0 && (exponent & 1) == 1) {
          return NEGATIVE_ONE.shift_left(bits_to_shift);
        }
        return ONE.shift_left(bits_to_shift);
      }
    } else {
      remaining_bits = part_to_square.bit_length();
      if (remaining_bits == 1) {
        if (signum_ < 0 && (exponent & 1) == 1) {
          return NEGATIVE_ONE;
        }
        return ONE;
      }
    }

    const int64_t scale_factor = static_cast<int64_t>(remaining_bits) * exponent;

    if (part_to_square.mag_.length() == 1 && scale_factor <= 62) {
      const int32_t new_sign = (signum_ < 0 && (exponent & 1) == 1) ? -1 : 1;
      int64_t result = 1;
      int64_t base_to_pow2 = part_to_square.mag_[0] & 0xffffffffULL;

      int32_t working_exponent = exponent;
      while (working_exponent != 0) {
        if ((working_exponent & 1) == 1) {
          result = result * base_to_pow2;
        }
        working_exponent = static_cast<int32_t>(static_cast<uint32_t>(working_exponent) >> 1);
        if (working_exponent != 0) {
          base_to_pow2 = base_to_pow2 * base_to_pow2;
        }
      }

      if (powers_of_two > 0) {
        if (bits_to_shift + scale_factor <= 62) {
          return value_of((result << bits_to_shift) * new_sign);
        }
        return value_of(result * new_sign).shift_left(bits_to_shift);
      }
      return value_of(result * new_sign);
    }

    if (static_cast<int64_t>(bit_length()) * exponent / 32 > MAX_MAG_LENGTH) {
      report_overflow();
    }

    bigint answer = ONE;
    int32_t working_exponent = exponent;
    while (working_exponent != 0) {
      if ((working_exponent & 1) == 1) {
        answer = answer.multiply(part_to_square);
      }
      working_exponent = static_cast<int32_t>(static_cast<uint32_t>(working_exponent) >> 1);
      if (working_exponent != 0) {
        part_to_square = part_to_square.square();
      }
    }
    if (powers_of_two > 0) {
      answer = answer.shift_left(bits_to_shift);
    }
    if (signum_ < 0 && (exponent & 1) == 1) {
      return answer.negate();
    }
    return answer;
  }

  // 返回非负数的整数平方根.
  bigint sqrt() const {
    if (signum_ < 0) {
      throw std::runtime_error("Negative BigInteger");
    }
    mutable_bigint a = to_mutable();
    mutable_bigint s = a.sqrt();
    return from_mutable(std::move(s), 1);
  }

  // 返回整数平方根和对应余数.
  std::pair<bigint, bigint> sqrt_and_remainder() const {
    bigint s = sqrt();
    bigint r = subtract(s.multiply(s));
    return {s, r};
  }

  // 返回 abs(gcd(this, val)).
  bigint gcd(const bigint& val) const {
    if (val.signum_ == 0) {
      return abs();
    }
    if (signum_ == 0) {
      return val.abs();
    }
    mutable_bigint a = to_mutable();
    mutable_bigint b = val.to_mutable();
    mutable_bigint g = a.hybrid_gcd(b);
    return from_mutable(std::move(g), 1);
  }

  // 返回 this mod m，m 必须为正.
  bigint mod(const bigint& m) const {
    if (m.signum_ <= 0) {
      throw std::runtime_error("BigInteger: modulus not positive");
    }
    bigint result = remainder(m);
    return result.signum_ >= 0 ? result : result.add(m);
  }

  // 返回 this 在正模 m 下的乘法逆元.
  bigint mod_inverse(const bigint& m) const {
    if (m.signum_ != 1) {
      throw std::runtime_error("BigInteger: modulus not positive");
    }
    if (m == ONE) {
      return ZERO;
    }
    bigint mod_val = (signum_ < 0 || compare_magnitude(m) >= 0) ? mod(m) : *this;
    if (mod_val == ONE) {
      return ONE;
    }
    mutable_bigint a = mod_val.to_mutable();
    mutable_bigint p = m.to_mutable();
    mutable_bigint inv = a.mutable_mod_inverse(p);
    return from_mutable(std::move(inv), 1);
  }

  // 返回 this^exponent mod m；m 必须为正.
  bigint mod_pow(const bigint& exponent, const bigint& m) const {
    if (m.signum_ <= 0) {
      throw std::runtime_error("BigInteger: modulus not positive");
    }
    if (exponent.signum_ == 0) {
      return m == ONE ? ZERO : ONE;
    }
    if (*this == ONE) {
      return m == ONE ? ZERO : ONE;
    }
    if (signum_ == 0 && exponent.signum_ >= 0) {
      return ZERO;
    }
    if (*this == NEGATIVE_ONE && !exponent.test_bit(0)) {
      return m == ONE ? ZERO : ONE;
    }

    bool invert_result = exponent.signum_ < 0;
    bigint exp = exponent;
    if (invert_result) {
      exp = exponent.negate();
    }

    const bigint base = (signum_ < 0 || compare_to(m) >= 0 ? mod(m) : *this);
    bigint result;
    if (m.test_bit(0)) {
      result = base.odd_mod_pow(exp, m);
    } else {
      const int32_t p = m.get_lowest_set_bit();
      const bigint m1 = m.shift_right(p);
      const bigint m2 = ONE.shift_left(p);

      const bigint base2 = (signum_ < 0 || compare_to(m1) >= 0 ? mod(m1) : *this);
      const bigint a1 = (m1 == ONE ? ZERO : base2.odd_mod_pow(exp, m1));
      const bigint a2 = base.mod_pow2(exp, p);

      const bigint y1 = m2.mod_inverse(m1);
      const bigint y2 = m1.mod_inverse(m2);

      if (m.mag_.length() < MAX_MAG_LENGTH / 2) {
        result = a1.multiply(m2).multiply(y1).add(a2.multiply(m1).multiply(y2)).mod(m);
      } else {
        mutable_bigint t1;
        a1.multiply(m2).to_mutable().multiply(y1.to_mutable(), t1);
        mutable_bigint t2;
        a2.multiply(m1).to_mutable().multiply(y2.to_mutable(), t2);
        t1.add(t2);
        mutable_bigint q;
        result = from_mutable(t1.divide(m.to_mutable(), q), 1);
      }
    }
    return invert_result ? result.mod_inverse(m) : result;
  }

  // odd modulus 的模幂；完整 Montgomery 滑动窗口版本会继续按 Java oddModPow 补齐.
  bigint odd_mod_pow(const bigint& y, const bigint& z) const {
    if (y == ONE) {
      return *this;
    }
    if (signum_ == 0) {
      return ZERO;
    }

    jarray<uint32_t> base = mag_.clone();
    const jarray<uint32_t>& exp = y.mag_;
    jarray<uint32_t> mod = z.mag_.clone();
    int32_t mod_len = mod.length();

    if ((mod_len & 1) != 0) {
      jarray<uint32_t> x(mod_len + 1);
      jarray_copy(mod, 0, x, 1, mod_len);
      mod = std::move(x);
      ++mod_len;
    }

    int32_t wbits = 0;
    int32_t ebits = bit_length(exp, exp.length());
    if ((ebits != 17) || (exp[0] != 65537)) {
      while (ebits > bn_exp_mod_thresh_table[wbits]) {
        ++wbits;
      }
    }

    const int32_t tblmask = 1 << wbits;
    std::vector<jarray<uint32_t>> table(static_cast<std::size_t>(tblmask));
    for (int32_t i = 0; i < tblmask; ++i) {
      table[static_cast<std::size_t>(i)] = jarray<uint32_t>(mod_len);
    }

    const uint64_t n0 = (mod[mod_len - 1] & 0xffffffffULL) + ((mod[mod_len - 2] & 0xffffffffULL) << 32);
    const uint64_t inv = uint64_t(0) - mutable_bigint::inverse_mod64(n0);

    jarray<uint32_t> a = shift_left_mag(base, mod_len << 5);
    mutable_bigint q;
    mutable_bigint a2(a);
    mutable_bigint b2(mod);
    b2.normalize();
    mutable_bigint r = a2.divide(b2, q);
    table[0] = r.to_int_array();

    if (table[0].length() < mod_len) {
      const int32_t offset = mod_len - table[0].length();
      jarray<uint32_t> t2(mod_len);
      jarray_copy(table[0], 0, t2, offset, table[0].length());
      table[0] = std::move(t2);
    }

    jarray<uint32_t> b = montgomery_square(table[0], mod, mod_len, inv, nullptr);
    jarray<uint32_t> t = b.copy_of(mod_len);

    for (int32_t i = 1; i < tblmask; ++i) {
      table[static_cast<std::size_t>(i)] =
          montgomery_multiply(t, table[static_cast<std::size_t>(i - 1)], mod, mod_len, inv, nullptr);
    }

    uint32_t bitpos = uint32_t{1} << ((ebits - 1) & (32 - 1));
    int32_t buf = 0;
    int32_t elen = exp.length();
    int32_t e_index = 0;
    for (int32_t i = 0; i <= wbits; ++i) {
      buf = (buf << 1) | (((exp[e_index] & bitpos) != 0) ? 1 : 0);
      bitpos >>= 1;
      if (bitpos == 0) {
        ++e_index;
        bitpos = uint32_t{1} << (32 - 1);
        --elen;
      }
    }

    int32_t multpos = ebits;
    --ebits;
    bool isone = true;

    multpos = ebits - wbits;
    while ((buf & 1) == 0) {
      buf = static_cast<int32_t>(static_cast<uint32_t>(buf) >> 1);
      ++multpos;
    }

    jarray<uint32_t> mult = table[static_cast<std::size_t>(static_cast<uint32_t>(buf) >> 1)].clone();

    buf = 0;
    if (multpos == ebits) {
      isone = false;
    }

    while (true) {
      --ebits;
      buf <<= 1;

      if (elen != 0) {
        buf |= ((exp[e_index] & bitpos) != 0) ? 1 : 0;
        bitpos >>= 1;
        if (bitpos == 0) {
          ++e_index;
          bitpos = uint32_t{1} << (32 - 1);
          --elen;
        }
      }

      if ((buf & tblmask) != 0) {
        multpos = ebits - wbits;
        while ((buf & 1) == 0) {
          buf = static_cast<int32_t>(static_cast<uint32_t>(buf) >> 1);
          ++multpos;
        }
        mult = table[static_cast<std::size_t>(static_cast<uint32_t>(buf) >> 1)].clone();
        buf = 0;
      }

      if (ebits == multpos) {
        if (isone) {
          b = mult.clone();
          isone = false;
        } else {
          t = b;
          a = montgomery_multiply(t, mult, mod, mod_len, inv, &a);
          t = a;
          a = b;
          b = t;
        }
      }

      if (ebits == 0) {
        break;
      }

      if (!isone) {
        t = b;
        a = montgomery_square(t, mod, mod_len, inv, &a);
        t = a;
        a = b;
        b = t;
      }
    }

    jarray<uint32_t> t2(2 * mod_len);
    jarray_copy(b, 0, t2, mod_len, mod_len);
    b = mont_reduce(t2, mod, mod_len, static_cast<uint32_t>(inv));
    t2 = b.copy_of(mod_len);

    return bigint(1, std::move(t2));
  }

  // 返回 this**exponent mod 2**p.
  bigint mod_pow2(const bigint& exponent, int32_t p) const {
    bigint result = ONE;
    bigint base_to_pow2 = mod2(p);
    int32_t exp_offset = 0;

    int32_t limit = exponent.bit_length();
    if (test_bit(0)) {
      limit = (p - 1) < limit ? (p - 1) : limit;
    }

    while (exp_offset < limit) {
      if (exponent.test_bit(exp_offset)) {
        result = result.multiply(base_to_pow2).mod2(p);
      }
      ++exp_offset;
      if (exp_offset < limit) {
        base_to_pow2 = base_to_pow2.square().mod2(p);
      }
    }
    return result;
  }

  // 返回 this mod 2**p；假定 this >= 0 且 p > 0.
  bigint mod2(int32_t p) const {
    if (bit_length() <= p) {
      return *this;
    }

    const int32_t num_ints = static_cast<int32_t>(static_cast<uint32_t>(p + 31) >> 5);
    jarray<uint32_t> mag(num_ints);
    jarray_copy(mag_, mag_.length() - num_ints, mag, 0, num_ints);

    const int32_t excess_bits = (num_ints << 5) - p;
    mag[0] &= static_cast<uint32_t>((uint64_t{1} << (32 - excess_bits)) - 1);

    return mag[0] == 0 ? bigint(1, mag) : bigint(1, std::move(mag));
  }

  // 返回 this << n；n 为负时等价于右移 -n.
  bigint shift_left(int32_t n) const {
    if (signum_ == 0) {
      return ZERO;
    }
    if (n > 0) {
      return bigint(signum_, shift_left_mag(mag_, n));
    } else if (n == 0) {
      return *this;
    } else {
      return shift_right_impl(static_cast<int32_t>(-static_cast<uint32_t>(n)));
    }
  }

  bigint operator<<(int32_t n) const {
    return shift_left(n);
  }

  // 返回 this >> n；负数执行 Java BigInteger 的算术右移语义.
  bigint shift_right(int32_t n) const {
    if (signum_ == 0) {
      return ZERO;
    }
    if (n > 0) {
      return shift_right_impl(n);
    } else if (n == 0) {
      return *this;
    } else {
      return bigint(signum_, shift_left_mag(mag_, static_cast<int32_t>(-static_cast<uint32_t>(n))));
    }
  }

  // 无符号距离右移核心；对应 Java shiftRightImpl.
  bigint shift_right_impl(int32_t n) const {
    const int32_t n_ints = static_cast<int32_t>(static_cast<uint32_t>(n) >> 5);
    const int32_t n_bits = n & 0x1f;
    const int32_t mag_len = mag_.length();
    jarray<uint32_t> new_mag;

    if (n_ints >= mag_len) {
      return signum_ >= 0 ? ZERO : NEGATIVE_ONE;
    }

    if (n_bits == 0) {
      const int32_t new_mag_len = mag_len - n_ints;
      new_mag = mag_.copy_of(new_mag_len);
    } else {
      int32_t i = 0;
      const uint32_t high_bits = mag_[0] >> n_bits;
      if (high_bits != 0) {
        new_mag = jarray<uint32_t>(mag_len - n_ints);
        new_mag[i++] = high_bits;
      } else {
        new_mag = jarray<uint32_t>(mag_len - n_ints - 1);
      }
      const int32_t num_iter = mag_len - n_ints - 1;
      shift_right_impl_worker(new_mag, mag_, i, n_bits, num_iter);
    }

    if (signum_ < 0) {
      bool ones_lost = false;
      for (int32_t i = mag_len - 1, j = mag_len - n_ints; i >= j && !ones_lost; --i) {
        ones_lost = mag_[i] != 0;
      }
      if (!ones_lost && n_bits != 0) {
        ones_lost = (mag_[mag_len - n_ints - 1] << (32 - n_bits)) != 0;
      }

      if (ones_lost) {
        new_mag = java_increment(std::move(new_mag));
      }
    }

    return bigint(signum_, std::move(new_mag));
  }

  bigint operator>>(int32_t n) const {
    return shift_right(n);
  }

  // 返回两个 BigInteger 的按位与，按无限二进制补码语义执行.
  bigint and_op(const bigint& val) const {
    const int32_t len = std::max(int_length(), val.int_length());
    jarray<uint32_t> result(len);
    for (int32_t i = 0; i < len; ++i) {
      result[i] = get_int(len - i - 1) & val.get_int(len - i - 1);
    }
    return value_of(result);
  }

  bigint operator&(const bigint& val) const {
    return and_op(val);
  }

  // 返回两个 BigInteger 的按位或，按无限二进制补码语义执行.
  bigint or_op(const bigint& val) const {
    const int32_t len = std::max(int_length(), val.int_length());
    jarray<uint32_t> result(len);
    for (int32_t i = 0; i < len; ++i) {
      result[i] = get_int(len - i - 1) | val.get_int(len - i - 1);
    }
    return value_of(result);
  }

  bigint operator|(const bigint& val) const {
    return or_op(val);
  }

  // 返回两个 BigInteger 的按位异或，按无限二进制补码语义执行.
  bigint xor_op(const bigint& val) const {
    const int32_t len = std::max(int_length(), val.int_length());
    jarray<uint32_t> result(len);
    for (int32_t i = 0; i < len; ++i) {
      result[i] = get_int(len - i - 1) ^ val.get_int(len - i - 1);
    }
    return value_of(result);
  }

  bigint operator^(const bigint& val) const {
    return xor_op(val);
  }

  // 返回按位取反，等价于 -(this + 1).
  bigint not_op() const {
    const int32_t len = int_length();
    jarray<uint32_t> result(len);
    for (int32_t i = 0; i < len; ++i) {
      result[i] = ~get_int(len - i - 1);
    }
    return value_of(result);
  }

  bigint operator~() const {
    return not_op();
  }

  // 返回 this & ~val.
  bigint and_not(const bigint& val) const {
    const int32_t len = std::max(int_length(), val.int_length());
    jarray<uint32_t> result(len);
    for (int32_t i = 0; i < len; ++i) {
      result[i] = get_int(len - i - 1) & ~val.get_int(len - i - 1);
    }
    return value_of(result);
  }

  // 测试第 n 个 bit 是否为 1.
  bool test_bit(int32_t n) const {
    if (n < 0) {
      throw std::runtime_error("Negative bit address");
    }
    return (get_int(n >> 5) & (uint32_t(1) << (n & 31))) != 0;
  }

  // 设置第 n 个 bit.
  bigint set_bit(int32_t n) const {
    if (n < 0) {
      throw std::runtime_error("Negative bit address");
    }
    const int32_t int_num = n >> 5;
    jarray<uint32_t> result(std::max(int_length(), ((n + 1) >> 5) + 1));
    for (int32_t i = 0; i < result.length(); ++i) {
      result[result.length() - i - 1] = get_int(i);
    }
    result[result.length() - int_num - 1] |= uint32_t(1) << (n & 31);
    return value_of(result);
  }

  // 清除第 n 个 bit.
  bigint clear_bit(int32_t n) const {
    if (n < 0) {
      throw std::runtime_error("Negative bit address");
    }
    const int32_t int_num = n >> 5;
    jarray<uint32_t> result(std::max(int_length(), int_num + 2));
    for (int32_t i = 0; i < result.length(); ++i) {
      result[result.length() - i - 1] = get_int(i);
    }
    result[result.length() - int_num - 1] &= ~(uint32_t(1) << (n & 31));
    return value_of(result);
  }

  // 翻转第 n 个 bit.
  bigint flip_bit(int32_t n) const {
    if (n < 0) {
      throw std::runtime_error("Negative bit address");
    }
    const int32_t int_num = n >> 5;
    jarray<uint32_t> result(std::max(int_length(), int_num + 2));
    for (int32_t i = 0; i < result.length(); ++i) {
      result[result.length() - i - 1] = get_int(i);
    }
    result[result.length() - int_num - 1] ^= uint32_t(1) << (n & 31);
    return value_of(result);
  }

  // 返回最低 set bit 的下标；零返回 -1.
  int32_t get_lowest_set_bit() const {
    int32_t lsb = lowest_set_bit_plus_two_ - 2;
    if (lsb == -2) {
      lsb = 0;
      if (signum_ == 0) {
        lsb -= 1;
      } else {
        int32_t i = mag_.length();
        uint32_t b = 0;
        do {
          b = mag_[--i];
        } while (b == 0);
        lsb += ((mag_.length() - i - 1) << 5) + mutable_bigint::number_of_trailing_zeros(b);
      }
      lowest_set_bit_plus_two_ = lsb + 2;
    }
    return lsb;
  }

  // 返回最小二进制补码表示所需的 bit 数，不含符号位.
  int32_t bit_length() const {
    int32_t n = bit_length_plus_one_ - 1;
    if (n == -1) {
      if (signum_ == 0) {
        n = 0;
      } else {
        const int32_t mag_bit_length = ((mag_.length() - 1) << 5) + mutable_bigint::bit_length_for_limb(mag_[0]);
        if (signum_ < 0) {
          bool pow2 = (mag_[0] & (mag_[0] - 1)) == 0;
          for (int32_t i = 1; i < mag_.length() && pow2; ++i) {
            pow2 = mag_[i] == 0;
          }
          n = pow2 ? mag_bit_length - 1 : mag_bit_length;
        } else {
          n = mag_bit_length;
        }
      }
      bit_length_plus_one_ = n + 1;
    }
    return n;
  }

  // 返回二进制补码表示中与符号位不同的 bit 个数.
  int32_t bit_count() const {
    int32_t bc = bit_count_plus_one_ - 1;
    if (bc == -1) {
      bc = 0;
      for (int32_t i = 0; i < mag_.length(); ++i) {
        bc += __builtin_popcount(mag_[i]);
      }
      if (signum_ < 0) {
        int32_t mag_trailing_zero_count = 0;
        int32_t j = mag_.length() - 1;
        while (j >= 0 && mag_[j] == 0) {
          mag_trailing_zero_count += 32;
          --j;
        }
        if (j >= 0) {
          mag_trailing_zero_count += mutable_bigint::number_of_trailing_zeros(mag_[j]);
        }
        bc += mag_trailing_zero_count - 1;
      }
      bit_count_plus_one_ = bc + 1;
    }
    return bc;
  }

  // 补码表示所需的 32-bit word 数.
  int32_t int_length() const {
    return (bit_length() >> 5) + 1;
  }

  int32_t sign_bit() const {
    return signum_ < 0 ? 1 : 0;
  }

  uint32_t sign_int() const {
    return signum_ < 0 ? 0xffffffffU : 0U;
  }

  // 返回小端补码第 n 个 int，越界时执行无限符号扩展.
  uint32_t get_int(int32_t n) const {
    if (n < 0) {
      return 0;
    }
    if (n >= mag_.length()) {
      return sign_int();
    }
    const uint32_t mag_int = mag_[mag_.length() - n - 1];
    if (signum_ >= 0) {
      return mag_int;
    }
    return n <= first_nonzero_int_num() ? uint32_t(0U - mag_int) : ~mag_int;
  }

  // 返回绝对值小端视图中第一个非零 int 的下标.
  int32_t first_nonzero_int_num() const {
    int32_t fn = first_nonzero_int_num_plus_two_ - 2;
    if (fn == -2) {
      int32_t i = mag_.length() - 1;
      while (i >= 0 && mag_[i] == 0) {
        --i;
      }
      fn = mag_.length() - i - 1;
      first_nonzero_int_num_plus_two_ = fn + 2;
    }
    return fn;
  }

  // 按指定 certainty 判断是否为 probable prime.
  bool is_probable_prime(int32_t certainty) const {
    if (certainty <= 0) {
      return true;
    }
    bigint n = abs();
    if (n == TWO) {
      return true;
    }
    if (n.signum_ == 0 || n == ONE || n.is_even()) {
      return false;
    }
    return n.prime_to_certainty(certainty);
  }

  // 返回指定 bitLength 的 probable prime；对应 Java probablePrime.
  static bigint probable_prime(int32_t bit_length, std::mt19937_64& rnd);

  // 小 bitLength 的随机 probable prime 搜索.
  static bigint small_prime(int32_t bit_length, int32_t certainty, std::mt19937_64& rnd);

  // 大 bitLength 的随机 probable prime 搜索.
  static bigint large_prime(int32_t bit_length, int32_t certainty, std::mt19937_64& rnd);

  // Java nextProbablePrime 的搜索长度.
  static int32_t get_prime_search_len(int32_t bit_length);

  // SMALL_PRIME_PRODUCT 常量.
  static bigint small_prime_product();

  // 返回大于当前值的第一个 probable prime.
  bigint next_probable_prime() const;

  // Miller-Rabin + Lucas-Lehmer probable-prime 测试入口.
  bool prime_to_certainty(int32_t certainty) const {
    return prime_to_certainty(certainty, nullptr);
  }

  // Miller-Rabin + Lucas-Lehmer probable-prime 测试入口；对应 Java primeToCertainty(certainty, random).
  bool prime_to_certainty(int32_t certainty, std::mt19937_64* random) const {
    int32_t rounds = 0;
    const int32_t n = (std::min(certainty, std::numeric_limits<int32_t>::max() - 1) + 1) / 2;

    const int32_t size_in_bits = bit_length();
    if (size_in_bits < 100) {
      rounds = 50;
      rounds = n < rounds ? n : rounds;
      return passes_miller_rabin(rounds, random);
    }

    if (size_in_bits < 256) {
      rounds = 27;
    } else if (size_in_bits < 512) {
      rounds = 15;
    } else if (size_in_bits < 768) {
      rounds = 8;
    } else if (size_in_bits < 1024) {
      rounds = 4;
    } else {
      rounds = 2;
    }
    rounds = n < rounds ? n : rounds;

    return passes_miller_rabin(rounds, random) && passes_lucas_lehmer();
  }

  // Lucas-Lehmer probable-prime 测试.
  bool passes_lucas_lehmer() const {
    const bigint this_plus_one = add(ONE);

    int32_t d = 5;
    while (jacobi_symbol(d, *this) != -1) {
      d = (d < 0) ? std::abs(d) + 2 : -(d + 2);
    }

    const bigint u = lucas_lehmer_sequence(d, this_plus_one, *this);
    return u.mod(*this) == ZERO;
  }

  // 计算 Jacobi(p,n)，假定 n 为正奇数且 n>=3.
  static int32_t jacobi_symbol(int32_t p, const bigint& n) {
    if (p == 0) {
      return 0;
    }

    int32_t j = 1;
    int32_t u = static_cast<int32_t>(n.mag_[n.mag_.length() - 1]);

    if (p < 0) {
      p = -p;
      const int32_t n8 = u & 7;
      if ((n8 == 3) || (n8 == 7)) {
        j = -j;
      }
    }

    while ((p & 3) == 0) {
      p >>= 2;
    }
    if ((p & 1) == 0) {
      p >>= 1;
      if (((u ^ (u >> 1)) & 2) != 0) {
        j = -j;
      }
    }
    if (p == 1) {
      return j;
    }
    if ((p & u & 2) != 0) {
      j = -j;
    }
    u = n.mod(value_of(p)).int_value();

    while (u != 0) {
      while ((u & 3) == 0) {
        u >>= 2;
      }
      if ((u & 1) == 0) {
        u >>= 1;
        if (((p ^ (p >> 1)) & 2) != 0) {
          j = -j;
        }
      }
      if (u == 1) {
        return j;
      }
      const int32_t t = u;
      u = p;
      p = t;
      if ((u & p & 2) != 0) {
        j = -j;
      }
      u %= p;
    }
    return 0;
  }

  // Lucas-Lehmer 序列计算.
  static bigint lucas_lehmer_sequence(int32_t z, const bigint& k, const bigint& n) {
    const bigint d = value_of(z);
    bigint u = ONE;
    bigint v = ONE;

    for (int32_t i = k.bit_length() - 2; i >= 0; --i) {
      bigint u2 = u.multiply(v).mod(n);

      bigint v2 = v.square().add(d.multiply(u.square())).mod(n);
      if (v2.test_bit(0)) {
        v2 = v2.subtract(n);
      }
      v2 = v2.shift_right(1);

      u = u2;
      v = v2;
      if (k.test_bit(i)) {
        u2 = u.add(v).mod(n);
        if (u2.test_bit(0)) {
          u2 = u2.subtract(n);
        }
        u2 = u2.shift_right(1);
        v2 = v.add(d.multiply(u)).mod(n);
        if (v2.test_bit(0)) {
          v2 = v2.subtract(n);
        }
        v2 = v2.shift_right(1);

        u = u2;
        v = v2;
      }
    }
    return u;
  }

  // Miller-Rabin 测试.
  bool passes_miller_rabin(int32_t iterations) const {
    return passes_miller_rabin(iterations, nullptr);
  }

  // Miller-Rabin 测试；对应 Java passesMillerRabin(iterations, random).
  bool passes_miller_rabin(int32_t iterations, std::mt19937_64* random) const {
    const bigint this_minus_one = subtract(ONE);
    bigint m = this_minus_one;
    const int32_t a = m.get_lowest_set_bit();
    m = m.shift_right(a);

    static thread_local std::mt19937_64 rnd{std::random_device{}()};
    std::mt19937_64& source = random == nullptr ? rnd : *random;
    for (int32_t i = 0; i < iterations; ++i) {
      bigint b;
      do {
        b = random_bigint(bit_length(), source);
      } while (b.compare_to(ONE) <= 0 || b.compare_to(*this) >= 0);

      int32_t j = 0;
      bigint z = b.mod_pow(m, *this);
      while (!((j == 0 && z == ONE) || z == this_minus_one)) {
        if ((j > 0 && z == ONE) || ++j == a) {
          return false;
        }
        z = z.mod_pow(TWO, *this);
      }
    }
    return true;
  }

  // 生成随机 byte 数组并清除多余高位；对应 Java randomBits.
  static jarray<uint8_t> random_bits(int32_t num_bits, std::mt19937_64& rnd) {
    if (num_bits < 0) {
      throw std::invalid_argument("numBits must be non-negative");
    }
    const int32_t num_bytes = static_cast<int32_t>((static_cast<int64_t>(num_bits) + 7) / 8);
    jarray<uint8_t> bytes(num_bytes);
    for (int32_t i = 0; i < num_bytes; ++i) {
      bytes[i] = static_cast<uint8_t>(rnd() & 0xffU);
    }
    if (num_bytes > 0) {
      const int32_t excess_bits = 8 * num_bytes - num_bits;
      bytes[0] &= static_cast<uint8_t>((1U << (8 - excess_bits)) - 1U);
    }
    return bytes;
  }

  // 生成 [0, 2^num_bits) 的非负随机 BigInteger.
  static bigint random_bigint(int32_t num_bits, std::mt19937_64& rnd) {
    jarray<uint8_t> bytes = random_bits(num_bits, rnd);
    return bigint(1, bytes);
  }

  // 返回指定 radix 下的字符串表示；radix 越界时按 10 处理.
  std::string to_string(int32_t radix) const {
    if (signum_ == 0) {
      return "0";
    }
    if (radix < MIN_RADIX || radix > MAX_RADIX) {
      radix = 10;
    }

    const bigint abs_value = abs();
    const int32_t b = abs_value.bit_length();
    const int32_t num_chars =
        static_cast<int32_t>(std::floor(b * LOG_TWO / log_cache(radix)) + 1) + (signum_ < 0 ? 1 : 0);
    std::string sb;
    sb.reserve(static_cast<std::size_t>(num_chars));

    if (signum_ < 0) {
      sb.push_back('-');
    }

    to_string_recursive(abs_value, sb, radix, 0);
    return sb;
  }

  // 返回十进制字符串表示.
  std::string to_string() const {
    return to_string(10);
  }

  // 向字符串追加 num_zeros 个 0.
  static void pad_with_zeros(std::string& buf, int32_t num_zeros) {
    static const std::string zeros(NUM_ZEROS, '0');
    while (num_zeros >= NUM_ZEROS) {
      buf.append(zeros);
      num_zeros -= NUM_ZEROS;
    }
    if (num_zeros > 0) {
      buf.append(zeros, 0, static_cast<std::size_t>(num_zeros));
    }
  }

  // 小规模转字符串；对应 Java smallToString.
  void small_to_string(int32_t radix, std::string& buf, int32_t digits) const {
    assert(signum_ >= 0);

    if (signum_ == 0) {
      pad_with_zeros(buf, digits);
      return;
    }

    const int32_t max_num_digit_groups = (4 * mag_.length() + 6) / 7;
    std::vector<uint64_t> digit_groups(static_cast<std::size_t>(max_num_digit_groups));

    bigint tmp = *this;
    int32_t num_groups = 0;
    while (tmp.signum_ != 0) {
      const bigint d = long_radix_value(radix);
      auto results = tmp.divide_and_remainder(d);
      digit_groups[static_cast<std::size_t>(num_groups++)] = static_cast<uint64_t>(results.second.long_value());
      tmp = results.first;
    }

    std::string s = uint64_to_string(digit_groups[static_cast<std::size_t>(num_groups - 1)], radix);
    pad_with_zeros(buf, digits - (static_cast<int32_t>(s.size()) + (num_groups - 1) * digits_per_long[radix]));
    buf.append(s);

    for (int32_t i = num_groups - 2; i >= 0; --i) {
      s = uint64_to_string(digit_groups[static_cast<std::size_t>(i)], radix);
      const int32_t num_leading_zeros = digits_per_long[radix] - static_cast<int32_t>(s.size());
      if (num_leading_zeros != 0) {
        pad_with_zeros(buf, num_leading_zeros);
      }
      buf.append(s);
    }
  }

  // Schoenhage 递归转字符串.
  static void to_string_recursive(const bigint& u, std::string& sb, int32_t radix, int32_t digits) {
    assert(u.signum() >= 0);

    if (u.mag_.length() <= SCHOENHAGE_BASE_CONVERSION_THRESHOLD) {
      u.small_to_string(radix, sb, digits);
      return;
    }

    const int32_t b = u.bit_length();
    const int32_t n = static_cast<int32_t>(std::round(std::log(b * LOG_TWO / log_cache(radix)) / LOG_TWO - 1.0));

    const bigint v = get_radix_conversion_cache(radix, n);
    auto results = u.divide_and_remainder(v);

    const int32_t expected_digits = 1 << n;
    to_string_recursive(results.first, sb, radix, digits - expected_digits);
    to_string_recursive(results.second, sb, radix, expected_digits);
  }

  // 返回 radix^(2^exponent)，按 Java powerCache 思路缓存.
  static bigint get_radix_conversion_cache(int32_t radix, int32_t exponent) {
    static std::vector<std::vector<bigint>> power_cache;
    if (power_cache.empty()) {
      power_cache.resize(MAX_RADIX + 1);
      for (int32_t i = MIN_RADIX; i <= MAX_RADIX; ++i) {
        power_cache[static_cast<std::size_t>(i)].push_back(value_of(i));
      }
    }

    std::vector<bigint>& cache_line = power_cache[static_cast<std::size_t>(radix)];
    if (exponent < static_cast<int32_t>(cache_line.size())) {
      return cache_line[static_cast<std::size_t>(exponent)];
    }

    const int32_t old_length = static_cast<int32_t>(cache_line.size());
    cache_line.resize(static_cast<std::size_t>(exponent + 1));
    for (int32_t i = old_length; i <= exponent; ++i) {
      cache_line[static_cast<std::size_t>(i)] = cache_line[static_cast<std::size_t>(i - 1)].pow(2);
    }
    return cache_line[static_cast<std::size_t>(exponent)];
  }

  // radix 对应的 log cache.
  static double log_cache(int32_t radix) {
    static std::array<double, MAX_RADIX + 1> cache{};
    static bool initialized = false;
    if (!initialized) {
      for (int32_t i = MIN_RADIX; i <= MAX_RADIX; ++i) {
        cache[static_cast<std::size_t>(i)] = std::log(static_cast<double>(i));
      }
      initialized = true;
    }
    return cache[static_cast<std::size_t>(radix)];
  }

  // longRadix[radix].
  static bigint long_radix_value(int32_t radix) {
    static const std::array<uint64_t, 37> values = {0,
                                                    0,
                                                    0x4000000000000000ULL,
                                                    0x383d9170b85ff80bULL,
                                                    0x4000000000000000ULL,
                                                    0x6765c793fa10079dULL,
                                                    0x41c21cb8e1000000ULL,
                                                    0x3642798750226111ULL,
                                                    0x1000000000000000ULL,
                                                    0x12bf307ae81ffd59ULL,
                                                    0x0de0b6b3a7640000ULL,
                                                    0x4d28cb56c33fa539ULL,
                                                    0x1eca170c00000000ULL,
                                                    0x780c7372621bd74dULL,
                                                    0x1e39a5057d810000ULL,
                                                    0x5b27ac993df97701ULL,
                                                    0x1000000000000000ULL,
                                                    0x27b95e997e21d9f1ULL,
                                                    0x5da0e1e53c5c8000ULL,
                                                    0x0b16a458ef403f19ULL,
                                                    0x16bcc41e90000000ULL,
                                                    0x2d04b7fdd9c0ef49ULL,
                                                    0x05658597bcaa24000ULL,
                                                    0x06feb266931a75b7ULL,
                                                    0x0c29e98000000000ULL,
                                                    0x14adf4b7320334b9ULL,
                                                    0x226ed36478bfa000ULL,
                                                    0x383d9170b85ff80bULL,
                                                    0x5a3c23e39c000000ULL,
                                                    0x04e900abb53e6b71ULL,
                                                    0x07600ec618141000ULL,
                                                    0x0aee5720ee830681ULL,
                                                    0x1000000000000000ULL,
                                                    0x172588ad4f5f0981ULL,
                                                    0x211e44f7d02c1000ULL,
                                                    0x2ee56725f06e5c71ULL,
                                                    0x41c21cb8e1000000ULL};
    return value_of(static_cast<int64_t>(values[static_cast<std::size_t>(radix)]));
  }

  // unsigned long 按 radix 转字符串.
  static std::string uint64_to_string(uint64_t value, int32_t radix) {
    static constexpr char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (value == 0) {
      return "0";
    }
    std::string out;
    while (value != 0) {
      const uint64_t q = value / static_cast<uint64_t>(radix);
      const uint64_t r = value - q * static_cast<uint64_t>(radix);
      out.push_back(chars[static_cast<std::size_t>(r)]);
      value = q;
    }
    std::reverse(out.begin(), out.end());
    return out;
  }

  // 返回最小长度的大端二进制补码 byte 数组.
  jarray<uint8_t> to_byte_array() const {
    const int32_t byte_len = bit_length() / 8 + 1;
    jarray<uint8_t> byte_array(byte_len);
    int32_t byte_num = byte_len;
    int32_t bytes_copied = 4;
    uint32_t next_int = 0;
    int32_t int_index = 0;
    while (byte_num > 0) {
      if (bytes_copied == 4) {
        next_int = get_int(int_index++);
        bytes_copied = 1;
      } else {
        next_int >>= 8;
        ++bytes_copied;
      }
      byte_array[--byte_num] = static_cast<uint8_t>(next_int);
    }
    return byte_array;
  }

  // 返回 mag 的无符号 byte 序列；对应 Java magSerializedForm.
  jarray<uint8_t> mag_serialized_form() const {
    const int32_t len = mag_.length();
    const int32_t bit_len = len == 0 ? 0 : ((len - 1) << 5) + mutable_bigint::bit_length_for_limb(mag_[0]);
    const int32_t byte_len = static_cast<int32_t>(static_cast<uint32_t>(bit_len + 7) >> 3);
    jarray<uint8_t> result(byte_len);

    int32_t bytes_copied = 4;
    int32_t int_index = len - 1;
    uint32_t next_int = 0;
    for (int32_t i = byte_len - 1; i >= 0; --i) {
      if (bytes_copied == 4) {
        next_int = mag_[int_index--];
        bytes_copied = 1;
      } else {
        next_int >>= 8;
        ++bytes_copied;
      }
      result[i] = static_cast<uint8_t>(next_int);
    }
    return result;
  }

  // 返回低 32 bit，按 Java intValue 截断.
  int32_t to_int() const {
    return static_cast<int32_t>(get_int(0));
  }

  int32_t int_value() const {
    return to_int();
  }

  // 返回低 64 bit，按 Java longValue 截断.
  int64_t to_long() const {
    uint64_t result = 0;
    for (int32_t i = 1; i >= 0; --i) {
      result = (result << 32) | (get_int(i) & 0xffffffffULL);
    }
    return static_cast<int64_t>(result);
  }

  int64_t long_value() const {
    return to_long();
  }

  // 转为 float；对应 Java floatValue 的位级舍入流程.
  float to_float() const {
    if (signum_ == 0) {
      return 0.0f;
    }

    constexpr int32_t SIGNIFICAND_WIDTH = 24;
    constexpr int32_t EXP_BIAS = 127;
    constexpr int32_t MAX_EXPONENT = 127;
    constexpr uint32_t SIGNIF_BIT_MASK = 0x007fffffU;
    constexpr uint32_t SIGN_BIT_MASK = 0x80000000U;

    const int32_t exponent = ((mag_.length() - 1) << 5) + mutable_bigint::bit_length_for_limb(mag_[0]) - 1;
    if (exponent < 63) {
      return static_cast<float>(long_value());
    }
    if (exponent > MAX_EXPONENT) {
      return signum_ > 0 ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
    }

    const int32_t shift = exponent - SIGNIFICAND_WIDTH;
    const int32_t n_bits = shift & 0x1f;
    const int32_t n_bits2 = 32 - n_bits;

    uint32_t twice_signif_floor = 0;
    if (n_bits == 0) {
      twice_signif_floor = mag_[0];
    } else {
      twice_signif_floor = mag_[0] >> n_bits;
      if (twice_signif_floor == 0) {
        twice_signif_floor = (mag_[0] << n_bits2) | (mag_[1] >> n_bits);
      }
    }

    uint32_t signif_floor = twice_signif_floor >> 1;
    signif_floor &= SIGNIF_BIT_MASK;

    const bool increment =
        (twice_signif_floor & 1U) != 0 && ((signif_floor & 1U) != 0 || abs().get_lowest_set_bit() < shift);
    const uint32_t signif_rounded = increment ? signif_floor + 1U : signif_floor;
    uint32_t bits = static_cast<uint32_t>(exponent + EXP_BIAS) << (SIGNIFICAND_WIDTH - 1);
    bits += signif_rounded;
    bits |= static_cast<uint32_t>(signum_) & SIGN_BIT_MASK;

    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
  }

  float float_value() const {
    return to_float();
  }

  // 转为 double；对应 Java doubleValue 的位级舍入流程.
  double to_double() const {
    if (signum_ == 0) {
      return 0.0;
    }

    constexpr int32_t SIGNIFICAND_WIDTH = 53;
    constexpr int32_t EXP_BIAS = 1023;
    constexpr int32_t MAX_EXPONENT = 1023;
    constexpr uint64_t SIGNIF_BIT_MASK = 0x000fffffffffffffULL;
    constexpr uint64_t SIGN_BIT_MASK = 0x8000000000000000ULL;

    const int32_t exponent = ((mag_.length() - 1) << 5) + mutable_bigint::bit_length_for_limb(mag_[0]) - 1;
    if (exponent < 63) {
      return static_cast<double>(long_value());
    }
    if (exponent > MAX_EXPONENT) {
      return signum_ > 0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    }

    const int32_t shift = exponent - SIGNIFICAND_WIDTH;
    const int32_t n_bits = shift & 0x1f;
    const int32_t n_bits2 = 32 - n_bits;

    uint32_t high_bits = 0;
    uint32_t low_bits = 0;
    if (n_bits == 0) {
      high_bits = mag_[0];
      low_bits = mag_[1];
    } else {
      high_bits = mag_[0] >> n_bits;
      low_bits = (mag_[0] << n_bits2) | (mag_[1] >> n_bits);
      if (high_bits == 0) {
        high_bits = low_bits;
        low_bits = (mag_[1] << n_bits2) | (mag_[2] >> n_bits);
      }
    }

    const uint64_t twice_signif_floor = (static_cast<uint64_t>(high_bits) << 32) | low_bits;
    uint64_t signif_floor = twice_signif_floor >> 1;
    signif_floor &= SIGNIF_BIT_MASK;

    const bool increment =
        (twice_signif_floor & 1ULL) != 0 && ((signif_floor & 1ULL) != 0 || abs().get_lowest_set_bit() < shift);
    const uint64_t signif_rounded = increment ? signif_floor + 1ULL : signif_floor;
    uint64_t bits = static_cast<uint64_t>(exponent + EXP_BIAS) << (SIGNIFICAND_WIDTH - 1);
    bits += signif_rounded;
    bits |= static_cast<uint64_t>(static_cast<int64_t>(signum_)) & SIGN_BIT_MASK;

    double result = 0.0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
  }

  double double_value() const {
    return to_double();
  }

  // 精确转换为 int64_t；超出范围时抛异常.
  int64_t to_long_exact() const {
    if (mag_.length() <= 2 && bit_length() <= 63) {
      return to_long();
    }
    throw std::runtime_error("BigInteger out of long range");
  }

  int64_t long_value_exact() const {
    return to_long_exact();
  }

  // 精确转换为 int32_t；超出范围时抛异常.
  int32_t to_int_exact() const {
    if (mag_.length() <= 1 && bit_length() <= 31) {
      return to_int();
    }
    throw std::runtime_error("BigInteger out of int range");
  }

  int32_t int_value_exact() const {
    return to_int_exact();
  }

  // 精确转换为 int16_t；超出范围时抛异常.
  int16_t to_short_exact() const {
    if (mag_.length() <= 1 && bit_length() <= 31) {
      const int32_t value = to_int();
      if (value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max()) {
        return static_cast<int16_t>(value);
      }
    }
    throw std::runtime_error("BigInteger out of short range");
  }

  int16_t short_value_exact() const {
    return to_short_exact();
  }

  // 精确转换为 int8_t；超出范围时抛异常.
  int8_t to_byte_exact() const {
    if (mag_.length() <= 1 && bit_length() <= 31) {
      const int32_t value = to_int();
      if (value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max()) {
        return static_cast<int8_t>(value);
      }
    }
    throw std::runtime_error("BigInteger out of byte range");
  }

  int8_t byte_value_exact() const {
    return to_byte_exact();
  }

  // 返回当前 magnitude 拷贝为 mutable_bigint.
  mutable_bigint to_mutable() const {
    return mutable_bigint(mag_);
  }

  // 从 mutable_bigint 和符号构造规范 bigint.
  static bigint from_mutable(mutable_bigint&& value, int32_t sign) {
    value.normalize();
    if (value.is_zero()) {
      return ZERO;
    }
    return bigint(sign, value.get_magnitude_array());
  }

  // 检查子区间是否满足 Java Objects.checkFromIndexSize 的约束.
  static void check_from_index_size(int32_t off, int32_t len, int32_t array_len) {
    if (off < 0 || len < 0 || off > array_len || len > array_len - off) {
      throw std::out_of_range("Index out of bounds");
    }
  }

  // 检查 BigInteger 支持范围.
  void check_range() const {
    if (mag_.length() > MAX_MAG_LENGTH || (mag_.length() == MAX_MAG_LENGTH && (mag_[0] & 0x80000000U) != 0)) {
      report_overflow();
    }
  }

  static void report_overflow() {
    throw std::runtime_error("BigInteger would overflow supported range");
  }

  // Character.digit 的 ASCII 子集实现，支持 radix 2..36.
  static int32_t digit(char ch, int32_t radix) {
    int32_t value = -1;
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c >= '0' && c <= '9') {
      value = c - '0';
    } else if (c >= 'a' && c <= 'z') {
      value = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
      value = c - 'A' + 10;
    }
    return value >= 0 && value < radix ? value : -1;
  }

  // 解析 [start, end) 内的一组 radix 数字，非法字符时报错.
  static uint32_t parse_group(const std::string& source, int32_t start, int32_t end, int32_t radix) {
    uint32_t result = 0;
    for (int32_t i = start; i < end; ++i) {
      const int32_t next = digit(source[static_cast<std::size_t>(i)], radix);
      if (next < 0) {
        throw std::invalid_argument("Illegal digit");
      }
      result = result * static_cast<uint32_t>(radix) + static_cast<uint32_t>(next);
    }
    return result;
  }

  // 解析十进制字符片段；对应 Java parseInt(char[] source, int start, int end).
  static uint32_t parse_int_decimal(const std::string& source, int32_t start, int32_t end) {
    int32_t result = digit(source[static_cast<std::size_t>(start++)], 10);
    if (result == -1) {
      throw std::invalid_argument("Illegal digit");
    }
    for (int32_t index = start; index < end; ++index) {
      const int32_t next_val = digit(source[static_cast<std::size_t>(index)], 10);
      if (next_val == -1) {
        throw std::invalid_argument("Illegal digit");
      }
      result = 10 * result + next_val;
    }
    return static_cast<uint32_t>(result);
  }

  // 去掉大端 limb 数组中的前导零，返回新数组.
  static jarray<uint32_t> strip_leading_zero_limbs(const jarray<uint32_t>& val) {
    int32_t keep = 0;
    while (keep < val.length() && val[keep] == 0) {
      ++keep;
    }
    return val.copy_of_range(keep, val.length());
  }

  // 去掉前导零；源数组可信时仍按值返回，便于保持不可变语义.
  static jarray<uint32_t> trusted_strip_leading_zero_limbs(const jarray<uint32_t>& val) {
    int32_t keep = 0;
    while (keep < val.length() && val[keep] == 0) {
      ++keep;
    }
    return keep == 0 ? val.clone() : val.copy_of_range(keep, val.length());
  }

  // 将大端 byte 子数组转换为规范 magnitude limb 数组.
  static jarray<uint32_t> strip_leading_zero_bytes(const jarray<uint8_t>& a, int32_t from, int32_t len) {
    return strip_leading_zero_bytes(-129, a, from, len);
  }

  // 返回去掉前导 0 byte 后的 magnitude；b < -128 表示尚未读取 a[from].
  static jarray<uint32_t> strip_leading_zero_bytes(int32_t b, const jarray<uint8_t>& a, int32_t from, int32_t len) {
    if (len == 0) {
      return jarray<uint32_t>();
    }
    const int32_t to = from + len;
    if (b < -128) {
      b = static_cast<int8_t>(a[from]);
    }
    ++from;

    for (; b == 0 && from < to; b = static_cast<int8_t>(a[from++])) {
    }
    if (b == 0) {
      return jarray<uint32_t>();
    }

    jarray<uint32_t> result(((to - from) >> 2) + 1);

    uint32_t d0 = static_cast<uint32_t>(b) & 0xffU;
    while (((to - from) & 0x3) != 0) {
      d0 = (d0 << 8) | (a[from++] & 0xffU);
    }
    result[0] = d0;

    int32_t i = 1;
    while (from < to) {
      const uint32_t b0 = a[from++];
      const uint32_t b1 = a[from++];
      const uint32_t b2 = a[from++];
      const uint32_t b3 = a[from++];
      result[i++] = (b0 << 24) | ((b1 & 0xffU) << 16) | ((b2 & 0xffU) << 8) | (b3 & 0xffU);
    }
    return result;
  }

  // 将负数二进制补码 byte 子数组转换为其绝对值 magnitude.
  static jarray<uint32_t> make_positive_bytes(const jarray<uint8_t>& a, int32_t from, int32_t len) {
    return make_positive_bytes(static_cast<int8_t>(a[from]), a, from, len);
  }

  // 将负数二进制补码 byte 子数组转换为其绝对值 magnitude；b 是已读取的 a[from].
  static jarray<uint32_t> make_positive_bytes(int32_t b, const jarray<uint8_t>& a, int32_t from, int32_t len) {
    const int32_t to = from + len;
    ++from;

    for (; b == -1 && from < to; b = static_cast<int8_t>(a[from++])) {
    }

    uint32_t d0 = (0xffffffffU << 8) | (static_cast<uint32_t>(b) & 0xffU);
    while (((to - from) & 0x3) != 0) {
      b = static_cast<int8_t>(a[from++]);
      d0 = (d0 << 8) | (static_cast<uint32_t>(b) & 0xffU);
    }
    const int32_t f = from;

    for (; b == 0 && from < to; b = static_cast<int8_t>(a[from++])) {
    }

    uint32_t d = static_cast<uint32_t>(b) & 0xffU;
    while (((to - from) & 0x3) != 0) {
      d = (d << 8) | (a[from++] & 0xffU);
    }

    const int32_t c = ((to - from) | static_cast<int32_t>(d0) | static_cast<int32_t>(d)) == 0 ? 1 : 0;
    jarray<uint32_t> result(c + 1 + ((to - f) >> 2));
    result[0] = c == 0 ? d0 : 0xffffffffU;

    int32_t i = result.length() - ((to - from) >> 2);
    if (i > 1) {
      result[i - 1] = d;
    }

    while (from < to) {
      const uint32_t b0 = a[from++];
      const uint32_t b1 = a[from++];
      const uint32_t b2 = a[from++];
      const uint32_t b3 = a[from++];
      result[i++] = (b0 << 24) | ((b1 & 0xffU) << 16) | ((b2 & 0xffU) << 8) | (b3 & 0xffU);
    }

    while (--i >= 0 && result[i] == 0) {
    }
    result[i] = uint32_t(0U - result[i]);
    while (--i >= 0) {
      result[i] = ~result[i];
    }
    return result;
  }

  // 将负数二进制补码 limb 数组转换为其绝对值 magnitude.
  static jarray<uint32_t> make_positive_limbs(const jarray<uint32_t>& a) {
    int32_t keep = 0;
    int32_t j = 0;

    for (keep = 0; keep < a.length() && a[keep] == 0xffffffffU; ++keep) {
    }

    for (j = keep; j < a.length() && a[j] == 0; ++j) {
    }
    const int32_t extra_int = j == a.length() ? 1 : 0;
    jarray<uint32_t> result(a.length() - keep + extra_int);

    for (int32_t i = keep; i < a.length(); ++i) {
      result[i - keep + extra_int] = ~a[i];
    }

    for (int32_t i = result.length() - 1; ++result[i] == 0; --i) {
    }

    return result;
  }

  // x = x*y + z，原地更新大端 limb 数组.
  static void destructive_mul_add(jarray<uint32_t>& x, uint32_t y, uint32_t z) {
    const uint64_t ylong = y & 0xffffffffULL;
    const uint64_t zlong = z & 0xffffffffULL;
    const int32_t len = x.length();
    uint64_t carry = 0;
    for (int32_t i = len - 1; i >= 0; --i) {
      const uint64_t product = ylong * (x[i] & 0xffffffffULL) + carry;
      x[i] = static_cast<uint32_t>(product);
      carry = product >> 32;
    }

    uint64_t sum = (x[len - 1] & 0xffffffffULL) + zlong;
    x[len - 1] = static_cast<uint32_t>(sum);
    carry = sum >> 32;
    for (int32_t i = len - 2; i >= 0; --i) {
      sum = (x[i] & 0xffffffffULL) + carry;
      x[i] = static_cast<uint32_t>(sum);
      carry = sum >> 32;
    }
  }

  // 单 limb 乘法；对应 Java multiplyByInt.
  static bigint multiply_by_int(const jarray<uint32_t>& x, uint32_t y, int32_t sign) {
    if (__builtin_popcount(y) == 1) {
      return bigint(sign, shift_left_mag(x, mutable_bigint::number_of_trailing_zeros(y)));
    }

    const int32_t xlen = x.length();
    jarray<uint32_t> rmag(xlen + 1);
    uint64_t carry = 0;
    const uint64_t yl = y & 0xffffffffULL;
    int32_t rstart = rmag.length() - 1;
    for (int32_t i = xlen - 1; i >= 0; --i) {
      const uint64_t product = (x[i] & 0xffffffffULL) * yl + carry;
      rmag[rstart--] = static_cast<uint32_t>(product);
      carry = product >> 32;
    }
    if (carry == 0) {
      rmag = rmag.copy_of_range(1, rmag.length());
    } else {
      rmag[rstart] = static_cast<uint32_t>(carry);
    }
    return bigint(sign, std::move(rmag));
  }

  // 多 limb 标准乘法入口；对应 Java multiplyToLen.
  static jarray<uint32_t> multiply_to_len(const jarray<uint32_t>& x, int32_t xlen, const jarray<uint32_t>& y,
                                          int32_t ylen, jarray<uint32_t>* z) {
    multiply_to_len_check(x, xlen);
    multiply_to_len_check(y, ylen);
    return impl_multiply_to_len(x, xlen, y, ylen, z);
  }

  // 多 limb 标准乘法实现；对应 Java implMultiplyToLen.
  static jarray<uint32_t> impl_multiply_to_len(const jarray<uint32_t>& x, int32_t xlen, const jarray<uint32_t>& y,
                                               int32_t ylen, jarray<uint32_t>* z) {
    const int32_t xstart = xlen - 1;
    const int32_t ystart = ylen - 1;

    jarray<uint32_t> local;
    if (z == nullptr || z->length() < xlen + ylen) {
      local = jarray<uint32_t>(xlen + ylen);
      z = &local;
    }

    uint64_t carry = 0;
    for (int32_t j = ystart, k = ystart + 1 + xstart; j >= 0; --j, --k) {
      const uint64_t product = (y[j] & 0xffffffffULL) * (x[xstart] & 0xffffffffULL) + carry;
      (*z)[k] = static_cast<uint32_t>(product);
      carry = product >> 32;
    }
    (*z)[xstart] = static_cast<uint32_t>(carry);

    for (int32_t i = xstart - 1; i >= 0; --i) {
      carry = 0;
      for (int32_t j = ystart, k = ystart + 1 + i; j >= 0; --j, --k) {
        const uint64_t product = (y[j] & 0xffffffffULL) * (x[i] & 0xffffffffULL) + ((*z)[k] & 0xffffffffULL) + carry;
        (*z)[k] = static_cast<uint32_t>(product);
        carry = product >> 32;
      }
      (*z)[i] = static_cast<uint32_t>(carry);
    }
    return z->clone();
  }

  // multiplyToLen 的参数检查.
  static void multiply_to_len_check(const jarray<uint32_t>& array, int32_t length) {
    if (length <= 0) {
      return;
    }
    if (length > array.length()) {
      throw std::out_of_range("multiplyToLen length out of range");
    }
  }

  // 返回 magnitude << n；对应 Java shiftLeft(int[] mag, int n).
  static jarray<uint32_t> shift_left_mag(const jarray<uint32_t>& mag, int32_t n) {
    const int32_t n_ints = static_cast<int32_t>(static_cast<uint32_t>(n) >> 5);
    const int32_t n_bits = n & 0x1f;
    const int32_t mag_len = mag.length();
    jarray<uint32_t> new_mag;

    if (n_bits == 0) {
      new_mag = jarray<uint32_t>(mag_len + n_ints);
      jarray_copy(mag, 0, new_mag, 0, mag_len);
    } else {
      int32_t i = 0;
      const int32_t n_bits2 = 32 - n_bits;
      const uint32_t high_bits = mag[0] >> n_bits2;
      if (high_bits != 0) {
        new_mag = jarray<uint32_t>(mag_len + n_ints + 1);
        new_mag[i++] = high_bits;
      } else {
        new_mag = jarray<uint32_t>(mag_len + n_ints);
      }
      const int32_t num_iter = mag_len - 1;
      shift_left_impl_worker(new_mag, mag, i, n_bits, num_iter);
      new_mag[num_iter + i] = mag[num_iter] << n_bits;
    }
    return new_mag;
  }

  // shiftLeft 的 limb 合并循环.
  static void shift_left_impl_worker(jarray<uint32_t>& new_arr, const jarray<uint32_t>& old_arr, int32_t new_idx,
                                     int32_t shift_count, int32_t num_iter) {
    const int32_t shift_count_right = 32 - shift_count;
    int32_t old_idx = 0;
    while (old_idx < num_iter) {
      const uint32_t high = old_arr[old_idx++];
      const uint32_t low = old_arr[old_idx];
      new_arr[new_idx++] = (high << shift_count) | (low >> shift_count_right);
    }
  }

  // shiftRightImpl 的 limb 合并循环.
  static void shift_right_impl_worker(jarray<uint32_t>& new_arr, const jarray<uint32_t>& old_arr, int32_t new_idx,
                                      int32_t shift_count, int32_t num_iter) {
    const int32_t shift_count_left = 32 - shift_count;
    int32_t idx = num_iter;
    int32_t nidx = new_idx == 0 ? num_iter - 1 : num_iter;
    while (nidx >= new_idx) {
      const uint32_t low = old_arr[idx--] >> shift_count;
      const uint32_t high = old_arr[idx] << shift_count_left;
      new_arr[nidx--] = low | high;
    }
  }

  // 对 magnitude 执行 Java BigInteger.javaIncrement.
  static jarray<uint32_t> java_increment(jarray<uint32_t> val) {
    uint32_t last_sum = 0;
    for (int32_t i = val.length() - 1; i >= 0 && last_sum == 0; --i) {
      val[i] += 1;
      last_sum = val[i];
    }
    if (last_sum == 0) {
      val = jarray<uint32_t>(val.length() + 1);
      val[0] = 1;
    }
    return val;
  }

  // 计算前 len 个 limb 的 bitLength，假定无前导零.
  static int32_t bit_length(const jarray<uint32_t>& val, int32_t len) {
    if (len == 0) {
      return 0;
    }
    return ((len - 1) << 5) + mutable_bigint::bit_length_for_limb(val[0]);
  }

  // 平方入口；对应 Java square().
  bigint square() const {
    return square(false);
  }

  // 平方；递归调用时跳过部分溢出检查.
  bigint square(bool is_recursion) const {
    if (signum_ == 0) {
      return ZERO;
    }
    const int32_t len = mag_.length();

    if (len < KARATSUBA_SQUARE_THRESHOLD) {
      jarray<uint32_t> z = square_to_len(mag_, len, nullptr);
      return bigint(1, trusted_strip_leading_zero_limbs(z));
    }
    if (len < TOOM_COOK_SQUARE_THRESHOLD) {
      return square_karatsuba();
    }

    if (!is_recursion) {
      if (bit_length(mag_, mag_.length()) > 16LL * MAX_MAG_LENGTH) {
        report_overflow();
      }
    }
    return square_toom_cook3();
  }

  // 平方标准算法入口；对应 Java squareToLen.
  static jarray<uint32_t> square_to_len(const jarray<uint32_t>& x, int32_t len, jarray<uint32_t>* z) {
    const int32_t zlen = len << 1;
    jarray<uint32_t> local;
    if (z == nullptr || z->length() < zlen) {
      local = jarray<uint32_t>(zlen);
      z = &local;
    }
    impl_square_to_len_checks(x, len, *z, zlen);
    return impl_square_to_len(x, len, *z, zlen);
  }

  // squareToLen 参数检查.
  static void impl_square_to_len_checks(const jarray<uint32_t>& x, int32_t len, const jarray<uint32_t>& z,
                                        int32_t zlen) {
    if (len < 1) {
      throw std::invalid_argument("invalid input length");
    }
    if (len > x.length()) {
      throw std::invalid_argument("input length out of bound");
    }
    if (len * 2 > z.length()) {
      throw std::invalid_argument("input length out of bound");
    }
    if (zlen < 1) {
      throw std::invalid_argument("invalid input length");
    }
    if (zlen > z.length()) {
      throw std::invalid_argument("input length out of bound");
    }
  }

  // Colin Plumb 平方算法；对应 Java implSquareToLen.
  static jarray<uint32_t> impl_square_to_len(const jarray<uint32_t>& x, int32_t len, jarray<uint32_t>& z,
                                             int32_t zlen) {
    uint32_t last_product_low_word = 0;
    for (int32_t j = 0, i = 0; j < len; ++j) {
      const uint64_t piece = x[j] & 0xffffffffULL;
      const uint64_t product = piece * piece;
      z[i++] = (last_product_low_word << 31) | static_cast<uint32_t>(product >> 33);
      z[i++] = static_cast<uint32_t>(product >> 1);
      last_product_low_word = static_cast<uint32_t>(product);
    }

    for (int32_t i = len, offset = 1; i > 0; --i, offset += 2) {
      const uint32_t t0 = x[i - 1];
      const uint32_t t = mul_add(z, x, offset, i - 1, t0);
      add_one(z, offset - 1, i, t);
    }

    primitive_left_shift(z, zlen, 1);
    z[zlen - 1] |= x[len - 1] & 1U;
    return z.clone();
  }

  // 左移数组 a 的前 len 个 limb；对应 Java leftShift(int[] a, int len, int n).
  static jarray<uint32_t> left_shift(jarray<uint32_t> a, int32_t len, int32_t n) {
    const int32_t n_ints = static_cast<int32_t>(static_cast<uint32_t>(n) >> 5);
    const int32_t n_bits = n & 0x1f;
    const int32_t bits_in_high_word = mutable_bigint::bit_length_for_limb(a[0]);

    if (n <= 32 - bits_in_high_word) {
      primitive_left_shift(a, len, n_bits);
      return a;
    }
    if (n_bits <= 32 - bits_in_high_word) {
      jarray<uint32_t> result(n_ints + len);
      jarray_copy(a, 0, result, 0, len);
      primitive_left_shift(result, result.length(), n_bits);
      return result;
    }

    jarray<uint32_t> result(n_ints + len + 1);
    jarray_copy(a, 0, result, 0, len);
    primitive_right_shift(result, result.length(), 32 - n_bits);
    return result;
  }

  // 原位右移 n bit，0<n<32；对应 Java primitiveRightShift.
  static void primitive_right_shift(jarray<uint32_t>& a, int32_t len, int32_t n) {
    shift_right_impl_worker(a, a, 1, n, len - 1);
    a[0] >>= n;
  }

  // 原位左移 n bit，0<=n<32；对应 Java primitiveLeftShift.
  static void primitive_left_shift(jarray<uint32_t>& a, int32_t len, int32_t n) {
    if (len == 0 || n == 0) {
      return;
    }
    shift_left_impl_worker(a, a, 0, n, len - 1);
    a[len - 1] <<= n;
  }

  // out += in * k，返回 carry；对应 Java mulAdd.
  static uint32_t mul_add(jarray<uint32_t>& out, const jarray<uint32_t>& in, int32_t offset, int32_t len, uint32_t k) {
    impl_mul_add_check(out, in, offset, len);
    return impl_mul_add(out, in, offset, len, k);
  }

  // mulAdd 参数检查.
  static void impl_mul_add_check(const jarray<uint32_t>& out, const jarray<uint32_t>& in, int32_t offset, int32_t len) {
    if (len > in.length()) {
      throw std::invalid_argument("input length is out of bound");
    }
    if (offset < 0) {
      throw std::invalid_argument("input offset is invalid");
    }
    if (offset > out.length() - 1) {
      throw std::invalid_argument("input offset is out of bound");
    }
    if (len > out.length() - offset) {
      throw std::invalid_argument("input len is out of bound");
    }
  }

  // mulAdd 实现.
  static uint32_t impl_mul_add(jarray<uint32_t>& out, const jarray<uint32_t>& in, int32_t offset, int32_t len,
                               uint32_t k) {
    const uint64_t k_long = k & 0xffffffffULL;
    uint64_t carry = 0;

    offset = out.length() - offset - 1;
    for (int32_t j = len - 1; j >= 0; --j) {
      const uint64_t product = (in[j] & 0xffffffffULL) * k_long + (out[offset] & 0xffffffffULL) + carry;
      out[offset--] = static_cast<uint32_t>(product);
      carry = product >> 32;
    }
    return static_cast<uint32_t>(carry);
  }

  // 向 a 的内部片段加一个 word，返回最终 carry；对应 Java addOne.
  static uint32_t add_one(jarray<uint32_t>& a, int32_t offset, int32_t mlen, uint32_t carry) {
    offset = a.length() - 1 - mlen - offset;
    uint64_t t = (a[offset] & 0xffffffffULL) + (carry & 0xffffffffULL);

    a[offset] = static_cast<uint32_t>(t);
    if ((t >> 32) == 0) {
      return 0;
    }
    while (--mlen >= 0) {
      if (--offset < 0) {
        return 1;
      }
      ++a[offset];
      if (a[offset] != 0) {
        return 0;
      }
    }
    return 1;
  }

  // Montgomery 乘法包装；对应 Java montgomeryMultiply.
  static jarray<uint32_t> montgomery_multiply(const jarray<uint32_t>& a, const jarray<uint32_t>& b,
                                              const jarray<uint32_t>& n, int32_t len, uint64_t inv,
                                              jarray<uint32_t>* product) {
    impl_montgomery_multiply_checks(a, b, n, len, product);
    if (len > MONTGOMERY_INTRINSIC_THRESHOLD) {
      jarray<uint32_t> prod = multiply_to_len(a, len, b, len, product);
      return mont_reduce(prod, n, len, static_cast<uint32_t>(inv));
    }
    jarray<uint32_t> materialized = materialize(product, len);
    return impl_montgomery_multiply(a, b, n, len, inv, materialized);
  }

  // Montgomery 平方包装；对应 Java montgomerySquare.
  static jarray<uint32_t> montgomery_square(const jarray<uint32_t>& a, const jarray<uint32_t>& n, int32_t len,
                                            uint64_t inv, jarray<uint32_t>* product) {
    impl_montgomery_multiply_checks(a, a, n, len, product);
    if (len > MONTGOMERY_INTRINSIC_THRESHOLD) {
      jarray<uint32_t> prod = square_to_len(a, len, product);
      return mont_reduce(prod, n, len, static_cast<uint32_t>(inv));
    }
    jarray<uint32_t> materialized = materialize(product, len);
    return impl_montgomery_square(a, n, len, inv, materialized);
  }

  // Montgomery 参数检查.
  static void impl_montgomery_multiply_checks(const jarray<uint32_t>& a, const jarray<uint32_t>& b,
                                              const jarray<uint32_t>& n, int32_t len, const jarray<uint32_t>* product) {
    if (len % 2 != 0) {
      throw std::invalid_argument("input array length must be even");
    }
    if (len < 1) {
      throw std::invalid_argument("invalid input length");
    }
    if (len > a.length() || len > b.length() || len > n.length() || (product != nullptr && len > product->length())) {
      throw std::invalid_argument("input array length out of bound");
    }
  }

  // 确保 Montgomery product 数组存在且至少有 len 个元素.
  static jarray<uint32_t> materialize(const jarray<uint32_t>* z, int32_t len) {
    if (z == nullptr || z->length() < len) {
      return jarray<uint32_t>(len);
    }
    return z->clone();
  }

  // Montgomery 乘法实现.
  static jarray<uint32_t> impl_montgomery_multiply(const jarray<uint32_t>& a, const jarray<uint32_t>& b,
                                                   const jarray<uint32_t>& n, int32_t len, uint64_t inv,
                                                   jarray<uint32_t>& product) {
    product = multiply_to_len(a, len, b, len, &product);
    return mont_reduce(product, n, len, static_cast<uint32_t>(inv));
  }

  // Montgomery 平方实现.
  static jarray<uint32_t> impl_montgomery_square(const jarray<uint32_t>& a, const jarray<uint32_t>& n, int32_t len,
                                                 uint64_t inv, jarray<uint32_t>& product) {
    product = square_to_len(a, len, &product);
    return mont_reduce(product, n, len, static_cast<uint32_t>(inv));
  }

  // Montgomery reduce；对应 Java montReduce.
  static jarray<uint32_t> mont_reduce(jarray<uint32_t> n, const jarray<uint32_t>& mod, int32_t mlen, uint32_t inv) {
    int32_t c = 0;
    int32_t len = mlen;
    int32_t offset = 0;

    do {
      const uint32_t n_end = n[n.length() - 1 - offset];
      const uint32_t carry = mul_add(n, mod, offset, mlen, inv * n_end);
      c += static_cast<int32_t>(add_one(n, offset, mlen, carry));
      ++offset;
    } while (--len > 0);

    while (c > 0) {
      c += sub_n(n, mod, mlen);
    }

    while (int_array_cmp_to_len(n, mod, mlen) >= 0) {
      sub_n(n, mod, mlen);
    }
    return n;
  }

  // 比较两个大端无符号数组的前 len 个元素.
  static int32_t int_array_cmp_to_len(const jarray<uint32_t>& arg1, const jarray<uint32_t>& arg2, int32_t len) {
    for (int32_t i = 0; i < len; ++i) {
      const uint64_t b1 = arg1[i] & 0xffffffffULL;
      const uint64_t b2 = arg2[i] & 0xffffffffULL;
      if (b1 < b2) {
        return -1;
      }
      if (b1 > b2) {
        return 1;
      }
    }
    return 0;
  }

  // 同长数组相减，返回 borrow.
  static int32_t sub_n(jarray<uint32_t>& a, const jarray<uint32_t>& b, int32_t len) {
    int64_t sum = 0;
    while (--len >= 0) {
      sum = static_cast<int64_t>(a[len] & 0xffffffffULL) - static_cast<int64_t>(b[len] & 0xffffffffULL) + (sum >> 32);
      a[len] = static_cast<uint32_t>(sum);
    }
    return static_cast<int32_t>(sum >> 32);
  }

  // Karatsuba 平方；对应 Java squareKaratsuba.
  bigint square_karatsuba() const {
    const int32_t half = (mag_.length() + 1) / 2;
    const bigint xl = get_lower(half);
    const bigint xh = get_upper(half);

    const bigint xhs = xh.square();
    const bigint xls = xl.square();

    return xhs.shift_left(half * 32).add(xl.add(xh).square().subtract(xhs.add(xls))).shift_left(half * 32).add(xls);
  }

  // Toom-Cook 3 路平方；对应 Java squareToomCook3.
  bigint square_toom_cook3() const {
    const int32_t len = mag_.length();
    const int32_t k = (len + 2) / 3;
    const int32_t r = len - 2 * k;

    const bigint a2 = get_toom_slice(k, r, 0, len);
    const bigint a1 = get_toom_slice(k, r, 1, len);
    const bigint a0 = get_toom_slice(k, r, 2, len);

    const bigint v0 = a0.square(true);
    bigint da1 = a2.add(a0);
    const bigint vm1 = da1.subtract(a1).square(true);
    da1 = da1.add(a1);
    const bigint v1 = da1.square(true);
    const bigint vinf = a2.square(true);
    const bigint v2 = da1.add(a2).shift_left(1).subtract(a0).square(true);

    bigint t2 = v2.subtract(vm1).exact_divide_by3();
    bigint tm1 = v1.subtract(vm1).shift_right(1);
    bigint t1 = v1.subtract(v0);
    t2 = t2.subtract(t1).shift_right(1);
    t1 = t1.subtract(tm1).subtract(vinf);
    t2 = t2.subtract(vinf.shift_left(1));
    tm1 = tm1.subtract(t2);

    const int32_t ss = k * 32;
    return vinf.shift_left(ss).add(t2).shift_left(ss).add(t1).shift_left(ss).add(tm1).shift_left(ss).add(v0);
  }

  // Karatsuba 乘法；对应 Java multiplyKaratsuba.
  static bigint multiply_karatsuba(const bigint& x, const bigint& y) {
    const int32_t xlen = x.mag_.length();
    const int32_t ylen = y.mag_.length();

    const int32_t half = (std::max(xlen, ylen) + 1) / 2;

    const bigint xl = x.get_lower(half);
    const bigint xh = x.get_upper(half);
    const bigint yl = y.get_lower(half);
    const bigint yh = y.get_upper(half);

    const bigint p1 = xh.multiply(yh);
    const bigint p2 = xl.multiply(yl);
    const bigint p3 = xh.add(xl).multiply(yh.add(yl));

    bigint result = p1.shift_left(32 * half).add(p3.subtract(p1).subtract(p2)).shift_left(32 * half).add(p2);

    if (x.signum_ != y.signum_) {
      return result.negate();
    }
    return result;
  }

  // Toom-Cook 3 路乘法；对应 Java multiplyToomCook3.
  static bigint multiply_toom_cook3(const bigint& a, const bigint& b) {
    const int32_t alen = a.mag_.length();
    const int32_t blen = b.mag_.length();
    const int32_t largest = std::max(alen, blen);

    const int32_t k = (largest + 2) / 3;
    const int32_t r = largest - 2 * k;

    const bigint a2 = a.get_toom_slice(k, r, 0, largest);
    const bigint a1 = a.get_toom_slice(k, r, 1, largest);
    const bigint a0 = a.get_toom_slice(k, r, 2, largest);
    const bigint b2 = b.get_toom_slice(k, r, 0, largest);
    const bigint b1 = b.get_toom_slice(k, r, 1, largest);
    const bigint b0 = b.get_toom_slice(k, r, 2, largest);

    const bigint v0 = a0.multiply(b0, true);
    bigint da1 = a2.add(a0);
    bigint db1 = b2.add(b0);
    const bigint vm1 = da1.subtract(a1).multiply(db1.subtract(b1), true);
    da1 = da1.add(a1);
    db1 = db1.add(b1);
    const bigint v1 = da1.multiply(db1, true);
    const bigint v2 = da1.add(a2).shift_left(1).subtract(a0).multiply(db1.add(b2).shift_left(1).subtract(b0), true);
    const bigint vinf = a2.multiply(b2, true);

    bigint t2 = v2.subtract(vm1).exact_divide_by3();
    bigint tm1 = v1.subtract(vm1).shift_right(1);
    bigint t1 = v1.subtract(v0);
    t2 = t2.subtract(t1).shift_right(1);
    t1 = t1.subtract(tm1).subtract(vinf);
    t2 = t2.subtract(vinf.shift_left(1));
    tm1 = tm1.subtract(t2);

    const int32_t ss = k * 32;
    bigint result = vinf.shift_left(ss).add(t2).shift_left(ss).add(t1).shift_left(ss).add(tm1).shift_left(ss).add(v0);

    if (a.signum_ != b.signum_) {
      return result.negate();
    }
    return result;
  }

  // magnitude + uint64_t，返回新 magnitude.
  static jarray<uint32_t> add_magnitude(const jarray<uint32_t>& x, uint64_t val) {
    int32_t x_index = x.length();
    jarray<uint32_t> result;
    uint64_t sum = 0;
    const uint32_t high_word = static_cast<uint32_t>(val >> 32);
    if (high_word == 0) {
      result = jarray<uint32_t>(x_index);
      sum = (x[--x_index] & 0xffffffffULL) + val;
      result[x_index] = static_cast<uint32_t>(sum);
    } else {
      if (x_index == 1) {
        result = jarray<uint32_t>(2);
        sum = val + (x[0] & 0xffffffffULL);
        result[1] = static_cast<uint32_t>(sum);
        result[0] = static_cast<uint32_t>(sum >> 32);
        return result;
      }
      result = jarray<uint32_t>(x_index);
      sum = (x[--x_index] & 0xffffffffULL) + (val & 0xffffffffULL);
      result[x_index] = static_cast<uint32_t>(sum);
      sum = (x[--x_index] & 0xffffffffULL) + (high_word & 0xffffffffULL) + (sum >> 32);
      result[x_index] = static_cast<uint32_t>(sum);
    }

    bool carry = (sum >> 32) != 0;
    while (x_index > 0 && carry) {
      --x_index;
      result[x_index] = x[x_index] + 1;
      carry = result[x_index] == 0;
    }
    while (x_index > 0) {
      --x_index;
      result[x_index] = x[x_index];
    }
    if (carry) {
      jarray<uint32_t> bigger(result.length() + 1);
      jarray_copy(result, 0, bigger, 1, result.length());
      bigger[0] = 1;
      return bigger;
    }
    return result;
  }

  // 两个 magnitude 相加，返回新 magnitude.
  static jarray<uint32_t> add_magnitude(const jarray<uint32_t>& xin, const jarray<uint32_t>& yin) {
    const jarray<uint32_t>* x = &xin;
    const jarray<uint32_t>* y = &yin;
    if (x->length() < y->length()) {
      std::swap(x, y);
    }

    int32_t x_index = x->length();
    int32_t y_index = y->length();
    jarray<uint32_t> result(x_index);
    uint64_t sum = 0;
    if (y_index == 1) {
      sum = ((*x)[--x_index] & 0xffffffffULL) + ((*y)[0] & 0xffffffffULL);
      result[x_index] = static_cast<uint32_t>(sum);
    } else {
      while (y_index > 0) {
        sum = ((*x)[--x_index] & 0xffffffffULL) + ((*y)[--y_index] & 0xffffffffULL) + (sum >> 32);
        result[x_index] = static_cast<uint32_t>(sum);
      }
    }

    bool carry = (sum >> 32) != 0;
    while (x_index > 0 && carry) {
      --x_index;
      result[x_index] = (*x)[x_index] + 1;
      carry = result[x_index] == 0;
    }
    while (x_index > 0) {
      --x_index;
      result[x_index] = (*x)[x_index];
    }
    if (carry) {
      jarray<uint32_t> bigger(result.length() + 1);
      jarray_copy(result, 0, bigger, 1, result.length());
      bigger[0] = 1;
      return bigger;
    }
    return result;
  }

  // uint64_t - magnitude，调用方保证 val >= little.
  static jarray<uint32_t> subtract_magnitude(uint64_t val, const jarray<uint32_t>& little) {
    const uint32_t high_word = static_cast<uint32_t>(val >> 32);
    if (high_word == 0) {
      jarray<uint32_t> result(1);
      result[0] = static_cast<uint32_t>(val - (little[0] & 0xffffffffULL));
      return result;
    }

    jarray<uint32_t> result(2);
    if (little.length() == 1) {
      int64_t difference = int64_t(val & 0xffffffffULL) - int64_t(little[0] & 0xffffffffULL);
      result[1] = static_cast<uint32_t>(difference);
      result[0] = difference < 0 ? high_word - 1 : high_word;
      return result;
    }

    int64_t difference = int64_t(val & 0xffffffffULL) - int64_t(little[1] & 0xffffffffULL);
    result[1] = static_cast<uint32_t>(difference);
    difference = int64_t(high_word & 0xffffffffULL) - int64_t(little[0] & 0xffffffffULL) + (difference >> 32);
    result[0] = static_cast<uint32_t>(difference);
    return result;
  }

  // magnitude - uint64_t，调用方保证 big >= val.
  static jarray<uint32_t> subtract_magnitude(const jarray<uint32_t>& big, uint64_t val) {
    const uint32_t high_word = static_cast<uint32_t>(val >> 32);
    int32_t big_index = big.length();
    jarray<uint32_t> result(big_index);
    int64_t difference = 0;

    if (high_word == 0) {
      difference = int64_t(big[--big_index] & 0xffffffffULL) - int64_t(val);
      result[big_index] = static_cast<uint32_t>(difference);
    } else {
      difference = int64_t(big[--big_index] & 0xffffffffULL) - int64_t(val & 0xffffffffULL);
      result[big_index] = static_cast<uint32_t>(difference);
      difference = int64_t(big[--big_index] & 0xffffffffULL) - int64_t(high_word & 0xffffffffULL) + (difference >> 32);
      result[big_index] = static_cast<uint32_t>(difference);
    }

    bool borrow = difference < 0;
    while (big_index > 0 && borrow) {
      --big_index;
      result[big_index] = big[big_index] - 1;
      borrow = result[big_index] == 0xffffffffU;
    }
    while (big_index > 0) {
      --big_index;
      result[big_index] = big[big_index];
    }
    return result;
  }

  // big - little，调用方保证 big >= little.
  static jarray<uint32_t> subtract_magnitude(const jarray<uint32_t>& big, const jarray<uint32_t>& little) {
    int32_t big_index = big.length();
    jarray<uint32_t> result(big_index);
    int32_t little_index = little.length();
    int64_t difference = 0;

    while (little_index > 0) {
      difference = int64_t(big[--big_index] & 0xffffffffULL) - int64_t(little[--little_index] & 0xffffffffULL) +
                   (difference >> 32);
      result[big_index] = static_cast<uint32_t>(difference);
    }

    bool borrow = difference < 0;
    while (big_index > 0 && borrow) {
      --big_index;
      result[big_index] = big[big_index] - 1;
      borrow = result[big_index] == 0xffffffffU;
    }
    while (big_index > 0) {
      --big_index;
      result[big_index] = big[big_index];
    }
    return result;
  }
};

inline const bigint bigint::ZERO{};
inline const bigint bigint::ONE{1};
inline const bigint bigint::TWO{2};
inline const bigint bigint::TEN{10};
inline const bigint bigint::NEGATIVE_ONE{-1};

// Java posConst 缓存.
inline bigint bigint::pos_const(int32_t n) {
  static const std::array<bigint, MAX_CONSTANT + 1> cache = [] {
    std::array<bigint, MAX_CONSTANT + 1> values{};
    for (int32_t i = 1; i <= MAX_CONSTANT; ++i) {
      jarray<uint32_t> magnitude(1);
      magnitude[0] = static_cast<uint32_t>(i);
      values[static_cast<std::size_t>(i)] = bigint(1, std::move(magnitude));
    }
    return values;
  }();
  return cache[static_cast<std::size_t>(n)];
}

// Java negConst 缓存.
inline bigint bigint::neg_const(int32_t n) {
  static const std::array<bigint, MAX_CONSTANT + 1> cache = [] {
    std::array<bigint, MAX_CONSTANT + 1> values{};
    for (int32_t i = 1; i <= MAX_CONSTANT; ++i) {
      jarray<uint32_t> magnitude(1);
      magnitude[0] = static_cast<uint32_t>(i);
      values[static_cast<std::size_t>(i)] = bigint(-1, std::move(magnitude));
    }
    return values;
  }();
  return cache[static_cast<std::size_t>(n)];
}

// bit_sieve
// 用于寻找素数候选值的简单 bit 筛，对应 Java BitSieve.
//
// 新筛的所有 bit 初始为 0；当一个候选数被判定为合数时，将对应 bit 置 1.
// 筛中不表示偶数，每个 bit 表示一个奇数：
//   N = offset + (2 * index + 1)
struct bit_sieve {
  // 存储筛 bit.
  jarray<uint64_t> bits_;

  // 筛持有的 bit 数量.
  int32_t length_{0};

  // 构造 Java smallSieve：base 为 0，用于生成小素数集合.
  bit_sieve() {
    length_ = 150 * 64;
    bits_ = jarray<uint64_t>(unit_index(length_ - 1) + 1);

    // 标记 1 为合数.
    set(0);
    int32_t next_index = 1;
    int32_t next_prime = 3;

    // 寻找素数并从筛中删除它们的倍数.
    do {
      sieve_single(length_, next_index + next_prime, next_prime);
      next_index = sieve_search(length_, next_index + 1);
      next_prime = 2 * next_index + 1;
    } while ((next_index > 0) && (next_prime < length_));
  }

  // 构造 searchLen 个 bit 的搜索筛；base 必须为偶数.
  bit_sieve(const bigint& base, int32_t search_len) {
    bits_ = jarray<uint64_t>(unit_index(search_len - 1) + 1);
    length_ = search_len;
    int32_t start = 0;

    int32_t step = small_sieve().sieve_search(small_sieve().length_, start);
    int32_t converted_step = (step * 2) + 1;

    // 在偶数 base 指定的偏移处构造大筛.
    do {
      // 计算 base mod convertedStep.
      start = static_cast<int32_t>(base.mod_uint32(static_cast<uint32_t>(converted_step)));

      // 从筛中删除 step 的每个倍数.
      start = converted_step - start;
      if (start % 2 == 0) {
        start += converted_step;
      }
      sieve_single(search_len, (start - 1) / 2, converted_step);

      // 从小筛中寻找下一个素数.
      step = small_sieve().sieve_search(small_sieve().length_, step + 1);
      converted_step = (step * 2) + 1;
    } while (step > 0);
  }

  // 给定 bit index，返回包含它的 64-bit 单元下标.
  static int32_t unit_index(int32_t bit_index) {
    return static_cast<int32_t>(static_cast<uint32_t>(bit_index) >> 6);
  }

  // 返回能屏蔽指定 bit 的 64-bit 单元.
  static uint64_t bit(int32_t bit_index) {
    return uint64_t{1} << (bit_index & ((1 << 6) - 1));
  }

  // 读取指定下标的 bit.
  bool get(int32_t bit_index) const {
    const int32_t index = unit_index(bit_index);
    return (bits_[index] & bit(bit_index)) != 0;
  }

  // 设置指定下标的 bit.
  void set(int32_t bit_index) {
    const int32_t index = unit_index(bit_index);
    bits_[index] |= bit(bit_index);
  }

  // 返回 start 处或之后第一个清零 bit 的下标；不搜索超过 limit.
  int32_t sieve_search(int32_t limit, int32_t start) const {
    if (start >= limit) {
      return -1;
    }

    int32_t index = start;
    do {
      if (!get(index)) {
        return index;
      }
      ++index;
    } while (index < limit - 1);
    return -1;
  }

  // 从 start 开始，将 step 的每个倍数从筛中删除.
  void sieve_single(int32_t limit, int32_t start, int32_t step) {
    while (start < limit) {
      set(start);
      start += step;
    }
  }

  // 在筛中测试 probable prime，并返回第一个通过测试的候选值.
  std::optional<bigint> retrieve(const bigint& init_value, int32_t certainty, std::mt19937_64* random = nullptr) const {
    int32_t offset = 1;
    for (int32_t i = 0; i < bits_.length(); ++i) {
      uint64_t next_long = ~bits_[i];
      for (int32_t j = 0; j < 64; ++j) {
        if ((next_long & 1U) == 1U) {
          bigint candidate = init_value.add(bigint::value_of(offset));
          if (candidate.prime_to_certainty(certainty, random)) {
            return candidate;
          }
        }
        next_long >>= 1;
        offset += 2;
      }
    }
    return std::nullopt;
  }

  // Java 的 static smallSieve.
  static const bit_sieve& small_sieve() {
    static const bit_sieve sieve;
    return sieve;
  }
};

// 返回指定 bitLength 的 probable prime；对应 Java probablePrime.
inline bigint bigint::probable_prime(int32_t bit_length, std::mt19937_64& rnd) {
  if (bit_length < 2) {
    throw std::runtime_error("bitLength < 2");
  }
  return bit_length < SMALL_PRIME_THRESHOLD ? small_prime(bit_length, DEFAULT_PRIME_CERTAINTY, rnd)
                                            : large_prime(bit_length, DEFAULT_PRIME_CERTAINTY, rnd);
}

// 小 bitLength 的随机 probable prime 搜索；对应 Java smallPrime.
inline bigint bigint::small_prime(int32_t bit_length, int32_t certainty, std::mt19937_64& rnd) {
  const int32_t mag_len = static_cast<int32_t>(static_cast<uint32_t>(bit_length + 31) >> 5);
  jarray<uint32_t> temp(mag_len);
  const uint32_t high_bit = uint32_t{1} << ((bit_length + 31) & 0x1f);
  const uint32_t high_mask = (high_bit << 1) - 1U;

  while (true) {
    for (int32_t i = 0; i < mag_len; ++i) {
      temp[i] = static_cast<uint32_t>(rnd());
    }
    temp[0] = (temp[0] & high_mask) | high_bit;
    if (bit_length > 2) {
      temp[mag_len - 1] |= 1U;
    }

    bigint p(1, temp);
    if (bit_length > 6) {
      const int64_t r = p.remainder(small_prime_product()).long_value();
      if ((r % 3 == 0) || (r % 5 == 0) || (r % 7 == 0) || (r % 11 == 0) || (r % 13 == 0) || (r % 17 == 0) ||
          (r % 19 == 0) || (r % 23 == 0) || (r % 29 == 0) || (r % 31 == 0) || (r % 37 == 0) || (r % 41 == 0)) {
        continue;
      }
    }

    if (bit_length < 4) {
      return p;
    }
    if (p.prime_to_certainty(certainty, &rnd)) {
      return p;
    }
  }
}

// 大 bitLength 的随机 probable prime 搜索；对应 Java largePrime.
inline bigint bigint::large_prime(int32_t bit_length, int32_t certainty, std::mt19937_64& rnd) {
  bigint p = random_bigint(bit_length, rnd).set_bit(bit_length - 1);
  p.mag_[p.mag_.length() - 1] &= 0xfffffffeU;

  const int32_t search_len = get_prime_search_len(bit_length);
  bit_sieve search_sieve(p, search_len);
  std::optional<bigint> candidate = search_sieve.retrieve(p, certainty, &rnd);

  while (!candidate.has_value() || candidate->bit_length() != bit_length) {
    p = p.add(bigint::value_of(2LL * search_len));
    if (p.bit_length() != bit_length) {
      p = random_bigint(bit_length, rnd).set_bit(bit_length - 1);
    }
    p.mag_[p.mag_.length() - 1] &= 0xfffffffeU;
    search_sieve = bit_sieve(p, search_len);
    candidate = search_sieve.retrieve(p, certainty, &rnd);
  }
  return *candidate;
}

// Java getPrimeSearchLen.
inline int32_t bigint::get_prime_search_len(int32_t bit_length) {
  if (bit_length > PRIME_SEARCH_BIT_LENGTH_LIMIT + 1) {
    throw std::runtime_error("Prime search implementation restriction on bitLength");
  }
  return bit_length / 20 * 64;
}

// SMALL_PRIME_PRODUCT = 3*5*...*41.
inline bigint bigint::small_prime_product() {
  return value_of(3LL * 5 * 7 * 11 * 13 * 17 * 19 * 23 * 29 * 31 * 37 * 41);
}

// 返回大于当前值的第一个 probable prime；对应 Java nextProbablePrime.
inline bigint bigint::next_probable_prime() const {
  if (signum_ < 0) {
    throw std::runtime_error("start < 0: " + to_string());
  }

  if (signum_ == 0 || *this == ONE) {
    return TWO;
  }

  bigint result = add(ONE);
  if (result.bit_length() < SMALL_PRIME_THRESHOLD) {
    if (!result.test_bit(0)) {
      result = result.add(ONE);
    }

    while (true) {
      if (result.bit_length() > 6) {
        const int64_t r = result.remainder(small_prime_product()).long_value();
        if ((r % 3 == 0) || (r % 5 == 0) || (r % 7 == 0) || (r % 11 == 0) || (r % 13 == 0) || (r % 17 == 0) ||
            (r % 19 == 0) || (r % 23 == 0) || (r % 29 == 0) || (r % 31 == 0) || (r % 37 == 0) || (r % 41 == 0)) {
          result = result.add(TWO);
          continue;
        }
      }

      if (result.bit_length() < 4) {
        return result;
      }
      if (result.prime_to_certainty(DEFAULT_PRIME_CERTAINTY)) {
        return result;
      }
      result = result.add(TWO);
    }
  }

  if (result.test_bit(0)) {
    result = result.subtract(ONE);
  }

  const int32_t search_len = get_prime_search_len(result.bit_length());
  while (true) {
    bit_sieve search_sieve(result, search_len);
    std::optional<bigint> candidate = search_sieve.retrieve(result, DEFAULT_PRIME_CERTAINTY);
    if (candidate.has_value()) {
      return *candidate;
    }
    result = result.add(bigint::value_of(2LL * search_len));
  }
}
