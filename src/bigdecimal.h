#pragma once

// Consolidated header: round_mode, math_context, jarray, mutable_bigint,
// signed_mutable_bigint, bigint, bit_sieve

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// ============================================================================
// round_mode.h
// ============================================================================

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

// ============================================================================
// math_context.h
// ============================================================================

// 不可变的数值运算上下文.
//
// math_context 封装会影响数值运算的上下文设置，例如 BigDecimal 运算使用的
// 精度和舍入模式.当前实现对应 Java MathContext 的基础行为：
// precision 表示运算使用的十进制位数；rounding_mode 表示舍入算法.
struct math_context {
  // 精度为 0 表示不限精度，舍入模式为 HALF_UP.
  static const math_context UNLIMITED;

  // IEEE 754 decimal32 对应的 7 位精度，舍入模式为 HALF_EVEN.
  static const math_context DECIMAL32;

  // IEEE 754 decimal64 对应的 16 位精度，舍入模式为 HALF_EVEN.
  static const math_context DECIMAL64;

  // IEEE 754 decimal128 对应的 34 位精度，舍入模式为 HALF_EVEN.
  static const math_context DECIMAL128;

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
  math_context(const math_context& other) : precision_(other.precision_), rounding_mode_(other.rounding_mode_) {
  }

  // 显式声明拷贝赋值，保持值对象语义.
  math_context& operator=(const math_context& other) {
    if (this == &other) {
      return *this;
    }
    precision_ = other.precision_;
    rounding_mode_ = other.rounding_mode_;
    return *this;
  }

  // 显式声明移动构造，移动后源对象恢复为默认上下文.
  math_context(math_context&& other) noexcept : precision_(other.precision_), rounding_mode_(other.rounding_mode_) {
    other.precision_ = default_digits;
    other.rounding_mode_ = default_rounding_mode;
  }

  // 显式声明移动赋值，移动后源对象恢复为默认上下文.
  math_context& operator=(math_context&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    precision_ = other.precision_;
    rounding_mode_ = other.rounding_mode_;
    other.precision_ = default_digits;
    other.rounding_mode_ = default_rounding_mode;
    return *this;
  }

  // 从字符串构造上下文.字符串格式必须与 to_string() 输出一致：
  // "precision=<digits> roundingMode=<mode>".
  //
  // 格式错误时抛出 std::invalid_argument("bad string format")；精度为负时
  // 抛出 std::invalid_argument("Digits < 0").
  math_context(const std::string& val) {
    constexpr const char* precision_prefix = "precision=";
    constexpr const char* rounding_prefix = "roundingMode=";

    try {
      if (val.rfind(precision_prefix, 0) != 0) {
        throw std::runtime_error("bad");
      }

      const std::size_t fence = val.find(' ');
      if (fence == std::string::npos) {
        throw std::runtime_error("bad");
      }

      const std::string precision_text = val.substr(10, fence - 10);
      std::size_t parsed = 0;
      const int parsed_precision = std::stoi(precision_text, &parsed);
      if (parsed != precision_text.size()) {
        throw std::runtime_error("bad");
      }

      const std::size_t rounding_pos = fence + 1;
      if (val.rfind(rounding_prefix, rounding_pos) != rounding_pos) {
        throw std::runtime_error("bad");
      }

      const std::string mode_text = val.substr(rounding_pos + 13);
      const round_mode parsed_rounding_mode = rounding_mode_from_string(mode_text);

      if (parsed_precision < min_digits) {
        throw std::invalid_argument("Digits < 0");
      }

      precision_ = parsed_precision;
      rounding_mode_ = parsed_rounding_mode;
    } catch (const std::invalid_argument& e) {
      if (std::string(e.what()) == "Digits < 0") {
        throw;
      }
      throw std::invalid_argument("bad string format");
    } catch (const std::exception&) {
      throw std::invalid_argument("bad string format");
    }
  }

  // 返回 precision 设置.该值始终非负.
  int precision() const {
    return precision_;
  }

  // 返回 rounding_mode 设置.
  round_mode rounding_mode() const {
    return rounding_mode_;
  }

  // 判断两个上下文是否具有完全相同的设置.
  friend bool operator==(const math_context& lhs, const math_context& rhs) {
    return lhs.precision_ == rhs.precision_ && lhs.rounding_mode_ == rhs.rounding_mode_;
  }

  friend bool operator!=(const math_context& lhs, const math_context& rhs) {
    return !(lhs == rhs);
  }

  // 返回 hash code.公式对应 Java MathContext#hashCode.
  int hash_code() const {
    return precision_ + static_cast<int>(rounding_mode_) * 59;
  }

  // 返回上下文设置的字符串表示.
  //
  // 格式为两个以单个空格分隔的字段：
  // "precision=<digits> roundingMode=<mode>".
  std::string to_string() const {
    return "precision=" + std::to_string(precision_) + " roundingMode=" + rounding_mode_to_string(rounding_mode_);
  }

  static constexpr int default_digits = 9;
  static constexpr round_mode default_rounding_mode = round_mode::HALF_UP;
  static constexpr int min_digits = 0;

  static const char* rounding_mode_to_string(round_mode mode) {
    switch (mode) {
      case round_mode::UP:
        return "UP";
      case round_mode::DOWN:
        return "DOWN";
      case round_mode::CEILING:
        return "CEILING";
      case round_mode::FLOOR:
        return "FLOOR";
      case round_mode::HALF_UP:
        return "HALF_UP";
      case round_mode::HALF_DOWN:
        return "HALF_DOWN";
      case round_mode::HALF_EVEN:
        return "HALF_EVEN";
      case round_mode::UNNECESSARY:
        return "UNNECESSARY";
    }
    throw std::invalid_argument("bad string format");
  }

  static round_mode rounding_mode_from_string(const std::string& value) {
    if (value == "UP") {
      return round_mode::UP;
    }
    if (value == "DOWN") {
      return round_mode::DOWN;
    }
    if (value == "CEILING") {
      return round_mode::CEILING;
    }
    if (value == "FLOOR") {
      return round_mode::FLOOR;
    }
    if (value == "HALF_UP") {
      return round_mode::HALF_UP;
    }
    if (value == "HALF_DOWN") {
      return round_mode::HALF_DOWN;
    }
    if (value == "HALF_EVEN") {
      return round_mode::HALF_EVEN;
    }
    if (value == "UNNECESSARY") {
      return round_mode::UNNECESSARY;
    }
    throw std::invalid_argument("bad string format");
  }

  int precision_{default_digits};
  round_mode rounding_mode_{default_rounding_mode};
};

inline const math_context math_context::UNLIMITED{0, round_mode::HALF_UP};
inline const math_context math_context::DECIMAL32{7, round_mode::HALF_EVEN};
inline const math_context math_context::DECIMAL64{16, round_mode::HALF_EVEN};
inline const math_context math_context::DECIMAL128{34, round_mode::HALF_EVEN};

// ============================================================================
// jarray.h
// ============================================================================

template <typename T>
struct jarray {
  static_assert(std::is_trivially_copyable_v<T>, "jarray<T> only support trivially copyable types");

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

// ============================================================================
// mutable_bigint.h
// ============================================================================

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
    return (uint64_t)(int_len_) * 32 - number_of_leading_zeros(value_[offset_]);
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

// ============================================================================
// signed_mutable_bigint.h
// ============================================================================

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

// ============================================================================
// mutable_bigint.h (continued)
// ============================================================================

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

// static 常量定义
inline const mutable_bigint mutable_bigint::ONE{1};

// ============================================================================
// bigint.h
// ============================================================================

// 不可变任意精度整数.
//
// 这个类按 Java BigInteger 的核心表示移植：signum_ 保存符号，mag_ 保存
// 大端无符号 magnitude，且规范形式中没有前导 0 limb.底层多精度运算优先
// 复用 mutable_bigint，避免在 bigint 中重复维护同一套 limb 算法.
struct bigint {
  int32_t signum_{0};
  jarray<uint32_t> mag_;

  static constexpr uint64_t LONG_MASK = 0xffffffffULL;
  static constexpr int32_t KARATSUBA_THRESHOLD = 80;
  static constexpr int32_t TOOM_COOK_THRESHOLD = 240;
  static constexpr int32_t KARATSUBA_SQUARE_THRESHOLD = 128;
  static constexpr int32_t TOOM_COOK_SQUARE_THRESHOLD = 216;
  static constexpr int32_t BURNIKEL_ZIEGLER_THRESHOLD = 80;
  static constexpr int32_t BURNIKEL_ZIEGLER_OFFSET = 40;

  static const bigint ZERO;
  static const bigint ONE;
  static const bigint TWO;
  static const bigint TEN;
  static const bigint NEGATIVE_ONE;

  bigint() = default;

  // 从大端二进制补码 byte 子数组构造 bigint.
  bigint(const jarray<uint8_t>& val, int32_t off, int32_t len) {
    if (val.length() == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }
    if (off < 0 || len < 0 || off > val.length() || len > val.length() - off) {
      throw std::out_of_range("BigInteger byte array range");
    }
    if (len == 0) {
      return;
    }

    const uint8_t b = val[off];
    if ((b & 0x80U) != 0) {
      mag_ = make_positive_bytes(val, off, len);
      signum_ = -1;
    } else {
      mag_ = strip_leading_zero_bytes(val, off, len);
      signum_ = mag_.length() == 0 ? 0 : 1;
    }
  }

  // 从完整大端二进制补码 byte 数组构造 bigint.
  explicit bigint(const jarray<uint8_t>& val) : bigint(val, 0, val.length()) {
  }

  // 从大端 sign-magnitude byte 子数组构造 bigint.
  bigint(int32_t signum, const jarray<uint8_t>& magnitude, int32_t off, int32_t len) {
    if (signum < -1 || signum > 1) {
      throw std::invalid_argument("Invalid signum value");
    }
    if (off < 0 || len < 0 || off > magnitude.length() || len > magnitude.length() - off) {
      throw std::out_of_range("BigInteger magnitude byte array range");
    }

    mag_ = strip_leading_zero_bytes(magnitude, off, len);
    if (mag_.length() == 0) {
      signum_ = 0;
    } else {
      if (signum == 0) {
        throw std::invalid_argument("signum-magnitude mismatch");
      }
      signum_ = signum;
    }
  }

  // 从完整大端 sign-magnitude byte 数组构造 bigint.
  bigint(int32_t signum, const jarray<uint8_t>& magnitude) : bigint(signum, magnitude, 0, magnitude.length()) {
  }

  // 从指定 radix 的字符串构造 bigint.
  bigint(const std::string& val, int32_t radix) {
    int32_t cursor = 0;
    const int32_t len = (int32_t)val.size();

    if (radix < 2 || radix > 36) {
      throw std::invalid_argument("Radix out of range");
    }
    if (len == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }

    int32_t sign = 1;
    const std::size_t index1 = val.find_last_of('-');
    const std::size_t index2 = val.find_last_of('+');
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

    while (cursor < len && digit(val[cursor], radix) == 0) {
      ++cursor;
    }

    if (cursor == len) {
      signum_ = 0;
      return;
    }

    const int32_t num_digits = len - cursor;
    signum_ = sign;

    const int64_t num_bits = (((int64_t)num_digits * bits_per_digit()[radix]) >> 10) + 1;
    if (num_bits + 31 >= (1LL << 32)) {
      throw std::overflow_error("BigInteger overflow");
    }
    const int32_t num_words = (int32_t)(num_bits + 31) >> 5;
    jarray<uint32_t> magnitude(num_words);

    int32_t first_group_len = num_digits % digits_per_int()[radix];
    if (first_group_len == 0) {
      first_group_len = digits_per_int()[radix];
    }

    magnitude[num_words - 1] = parse_group(val, cursor, cursor + first_group_len, radix);
    cursor += first_group_len;

    const uint32_t super_radix = int_radix()[radix];
    while (cursor < len) {
      const int32_t next = cursor + digits_per_int()[radix];
      const uint32_t group_val = parse_group(val, cursor, next, radix);
      destructive_mul_add(magnitude, super_radix, group_val);
      cursor = next;
    }

    mag_ = strip_leading_zero_limbs(magnitude);
    if (mag_.length() == 0) {
      signum_ = 0;
    }
  }

  // 从十进制字符串构造 bigint.
  explicit bigint(const std::string& val) : bigint(val, 10) {
  }

  explicit bigint(int64_t val) {
    if (val == 0) {
      return;
    }
    signum_ = val < 0 ? -1 : 1;
    uint64_t abs_val = val < 0 ? (uint64_t)(-(val + 1)) + 1 : (uint64_t)val;
    if ((abs_val >> 32) == 0) {
      mag_ = jarray<uint32_t>(1);
      mag_[0] = (uint32_t)abs_val;
    } else {
      mag_ = jarray<uint32_t>(2);
      mag_[0] = (uint32_t)(abs_val >> 32);
      mag_[1] = (uint32_t)abs_val;
    }
  }

  bigint(int32_t signum, const jarray<uint32_t>& magnitude) {
    if (signum < -1 || signum > 1) {
      throw std::invalid_argument("Invalid signum value");
    }
    mag_ = strip_leading_zero_limbs(magnitude);
    signum_ = mag_.length() == 0 ? 0 : signum;
    if (signum_ == 0 && mag_.length() != 0) {
      throw std::invalid_argument("signum-magnitude mismatch");
    }
  }

  explicit bigint(const jarray<uint32_t>& magnitude) : bigint(1, magnitude) {
  }

  static bigint value_of(int64_t val) {
    return bigint(val);
  }

  // 对应 Java 私有构造 BigInteger(int[] val)：从大端二进制补码 int 数组构造.
  static bigint from_twos_complement(const jarray<uint32_t>& val) {
    if (val.length() == 0) {
      throw std::invalid_argument("Zero length BigInteger");
    }

    if ((val[0] & 0x80000000U) != 0) {
      return bigint(-1, make_positive_limbs(val));
    }
    return bigint(1, strip_leading_zero_limbs(val));
  }

  int32_t signum() const {
    return signum_;
  }

  const jarray<uint32_t>& magnitude() const {
    return mag_;
  }

  bool is_zero() const {
    return signum_ == 0;
  }

  bool is_one() const {
    return signum_ == 1 && mag_.length() == 1 && mag_[0] == 1;
  }

  bool is_odd() const {
    return mag_.length() > 0 && (mag_[mag_.length() - 1] & 1U) != 0;
  }

  bool is_even() const {
    return !is_odd();
  }

  uint64_t bit_length() const {
    if (signum_ == 0) {
      return 0;
    }
    return (uint64_t)mag_.length() * 32 - mutable_bigint::number_of_leading_zeros(mag_[0]);
  }

  int32_t compare_to(const bigint& val) const {
    if (signum_ < val.signum_) {
      return -1;
    }
    if (signum_ > val.signum_) {
      return 1;
    }
    if (signum_ == 0) {
      return 0;
    }
    const int32_t cmp = compare_magnitude(val);
    return signum_ > 0 ? cmp : -cmp;
  }

  int32_t compare_magnitude(const bigint& val) const {
    return compare_magnitude(mag_, val.mag_);
  }

  bigint abs() const {
    return signum_ >= 0 ? *this : negate();
  }

  bigint negate() const {
    bigint result(*this);
    result.signum_ = -result.signum_;
    return result;
  }

  bigint add(const bigint& val) const {
    if (val.signum_ == 0) {
      return *this;
    }
    if (signum_ == 0) {
      return val;
    }
    if (val.signum_ == signum_) {
      return from_mutable(add_magnitude(to_mutable(), val.to_mutable()), signum_);
    }

    const int32_t cmp = compare_magnitude(val);
    if (cmp == 0) {
      return ZERO;
    }
    if (cmp > 0) {
      mutable_bigint diff = to_mutable();
      diff.subtract(val.to_mutable());
      return from_mutable(diff, signum_);
    }
    mutable_bigint diff = val.to_mutable();
    diff.subtract(to_mutable());
    return from_mutable(diff, val.signum_);
  }

  bigint add(int64_t val) const {
    return add(bigint(val));
  }

  bigint subtract(const bigint& val) const {
    return add(val.negate());
  }

  bigint subtract(int64_t val) const {
    return subtract(bigint(val));
  }

  bigint multiply(const bigint& val) const {
    if (signum_ == 0 || val.signum_ == 0) {
      return ZERO;
    }
    mutable_bigint product;
    to_mutable().multiply(val.to_mutable(), product);
    return from_mutable(product, signum_ == val.signum_ ? 1 : -1);
  }

  bigint divide(const bigint& val) const {
    return divide_and_remainder(val).first;
  }

  bigint remainder(const bigint& val) const {
    return divide_and_remainder(val).second;
  }

  std::pair<bigint, bigint> divide_and_remainder(const bigint& val) const {
    if (val.signum_ == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }
    if (signum_ == 0) {
      return {ZERO, ZERO};
    }

    mutable_bigint quotient;
    mutable_bigint remainder = to_mutable().divide(val.to_mutable(), quotient);
    const int32_t q_sign = signum_ == val.signum_ ? 1 : -1;
    return {from_mutable(quotient, q_sign), from_mutable(remainder, signum_)};
  }

  bigint shift_left(int32_t n) const {
    if (signum_ == 0) {
      return ZERO;
    }
    if (n == 0) {
      return *this;
    }
    if (n < 0) {
      return shift_right(-n);
    }
    mutable_bigint m = to_mutable();
    m.safe_left_shift(n);
    return from_mutable(m, signum_);
  }

  bigint shift_right(int32_t n) const {
    if (signum_ == 0) {
      return ZERO;
    }
    if (n == 0) {
      return *this;
    }
    if (n < 0) {
      return shift_left(-n);
    }
    if (signum_ < 0) {
      // Java BigInteger 的负数右移是向负无穷取整.这里用 -( ((abs(x)-1)>>n) + 1 )
      // 表达同一语义.
      bigint adjusted = abs().subtract(ONE).shift_right(n).add(ONE);
      return adjusted.negate();
    }
    mutable_bigint m = to_mutable();
    m.safe_right_shift(n);
    return from_mutable(m, 1);
  }

  bigint sqrt() const {
    if (signum_ < 0) {
      throw std::runtime_error("Negative BigInteger");
    }
    mutable_bigint root = to_mutable().sqrt();
    return from_mutable(root, 1);
  }

  bigint gcd(const bigint& val) const {
    if (signum_ == 0) {
      return val.abs();
    }
    if (val.signum_ == 0) {
      return abs();
    }
    mutable_bigint a = abs().to_mutable();
    mutable_bigint b = val.abs().to_mutable();
    return from_mutable(a.hybrid_gcd(b), 1);
  }

  bigint mod(const bigint& m) const {
    if (m.signum_ <= 0) {
      throw std::runtime_error("BigInteger: modulus not positive");
    }
    bigint result = remainder(m);
    return result.signum_ >= 0 ? result : result.add(m);
  }

  bigint mod_inverse(const bigint& m) const {
    if (m.signum_ != 1) {
      throw std::runtime_error("BigInteger: modulus not positive");
    }
    mutable_bigint a = mod(m).to_mutable();
    mutable_bigint p = m.to_mutable();
    return from_mutable(a.mutable_mod_inverse(p), 1);
  }

  uint32_t mod_uint32(uint32_t m) const {
    if (m == 0) {
      throw std::runtime_error("BigInteger divide by zero");
    }
    uint64_t rem = 0;
    for (int32_t i = 0; i < mag_.length(); ++i) {
      rem = ((rem << 32) + (mag_[i] & LONG_MASK)) % m;
    }
    if (signum_ < 0 && rem != 0) {
      rem = m - rem;
    }
    return (uint32_t)rem;
  }

  bool prime_to_certainty(int32_t certainty) const {
    (void)certainty;
    if (signum_ <= 0) {
      return false;
    }
    if (mag_.length() == 1) {
      return is_small_prime(mag_[0]);
    }
    // 这里保留为确定性小因子过滤后的占位入口；完整 Miller-Rabin/Lucas
    // 会随 BigInteger 素性测试模块继续移植.
    static const uint32_t small_primes[] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41};
    if (!is_odd()) {
      return false;
    }
    for (uint32_t p : small_primes) {
      if (mod_uint32(p) == 0) {
        return false;
      }
    }
    return true;
  }

  static jarray<uint32_t> strip_leading_zero_limbs(const jarray<uint32_t>& val) {
    int32_t keep = 0;
    while (keep < val.length() && val[keep] == 0) {
      ++keep;
    }
    return val.copy_of_range(keep, val.length());
  }

  static jarray<uint32_t> make_positive_limbs(const jarray<uint32_t>& val) {
    int32_t keep = 0;
    while (keep < val.length() && val[keep] == 0xffffffffU) {
      ++keep;
    }

    int32_t j = keep;
    while (j < val.length() && val[j] == 0) {
      ++j;
    }
    const int32_t extra_int = j == val.length() ? 1 : 0;
    jarray<uint32_t> result(val.length() - keep + extra_int);

    for (int32_t i = keep; i < val.length(); ++i) {
      result[i - keep + extra_int] = ~val[i];
    }

    for (int32_t i = result.length() - 1; i >= 0; --i) {
      ++result[i];
      if (result[i] != 0) {
        break;
      }
    }
    return result;
  }

  static jarray<uint32_t> strip_leading_zero_bytes(const jarray<uint8_t>& val, int32_t off, int32_t len) {
    int32_t keep = 0;
    while (keep < len && val[off + keep] == 0) {
      ++keep;
    }
    return bytes_to_magnitude(val, off + keep, len - keep);
  }

  static jarray<uint32_t> make_positive_bytes(const jarray<uint8_t>& val, int32_t off, int32_t len) {
    jarray<uint8_t> bytes(len);
    for (int32_t i = 0; i < len; ++i) {
      bytes[i] = (uint8_t)~val[off + i];
    }

    for (int32_t i = len - 1; i >= 0; --i) {
      bytes[i] = (uint8_t)(bytes[i] + 1);
      if (bytes[i] != 0) {
        break;
      }
    }

    return strip_leading_zero_bytes(bytes, 0, len);
  }

  static jarray<uint32_t> bytes_to_magnitude(const jarray<uint8_t>& val, int32_t off, int32_t len) {
    if (len == 0) {
      return jarray<uint32_t>();
    }

    const int32_t int_len = (len + 3) >> 2;
    jarray<uint32_t> result(int_len);
    int32_t byte_index = off + len;
    for (int32_t int_index = int_len - 1; int_index >= 0; --int_index) {
      uint32_t word = 0;
      const int32_t bytes_this_word = std::min(4, byte_index - off);
      for (int32_t j = 0; j < bytes_this_word; ++j) {
        word |= (uint32_t)val[--byte_index] << (8 * j);
      }
      result[int_index] = word;
    }
    return result;
  }

  static int32_t digit(char ch, int32_t radix) {
    int32_t value = -1;
    if (ch >= '0' && ch <= '9') {
      value = ch - '0';
    } else if (ch >= 'a' && ch <= 'z') {
      value = ch - 'a' + 10;
    } else if (ch >= 'A' && ch <= 'Z') {
      value = ch - 'A' + 10;
    }
    return value >= 0 && value < radix ? value : -1;
  }

  static uint32_t parse_group(const std::string& source, int32_t start, int32_t end, int32_t radix) {
    int32_t result = digit(source[start++], radix);
    if (result < 0) {
      throw std::invalid_argument("Illegal digit");
    }

    for (int32_t index = start; index < end; ++index) {
      const int32_t next_val = digit(source[index], radix);
      if (next_val < 0) {
        throw std::invalid_argument("Illegal digit");
      }
      result = radix * result + next_val;
    }

    return (uint32_t)result;
  }

  static void destructive_mul_add(jarray<uint32_t>& x, uint32_t y, uint32_t z) {
    const uint64_t ylong = y & LONG_MASK;
    const uint64_t zlong = z & LONG_MASK;
    const int32_t len = x.length();

    uint64_t carry = 0;
    for (int32_t i = len - 1; i >= 0; --i) {
      const uint64_t product = ylong * (x[i] & LONG_MASK) + carry;
      x[i] = (uint32_t)product;
      carry = product >> 32;
    }

    uint64_t sum = (x[len - 1] & LONG_MASK) + zlong;
    x[len - 1] = (uint32_t)sum;
    carry = sum >> 32;
    for (int32_t i = len - 2; i >= 0; --i) {
      sum = (x[i] & LONG_MASK) + carry;
      x[i] = (uint32_t)sum;
      carry = sum >> 32;
    }
  }

  static const int64_t* bits_per_digit() {
    static const int64_t table[] = {0,    0,    1024, 1624, 2048, 2378, 2648, 2875, 3072, 3247, 3402, 3543, 3672,
                                    3790, 3899, 4001, 4096, 4186, 4271, 4350, 4426, 4498, 4567, 4633, 4696, 4756,
                                    4814, 4870, 4923, 4975, 5025, 5074, 5120, 5166, 5210, 5253, 5295};
    return table;
  }

  static const int32_t* digits_per_int() {
    static const int32_t table[] = {0, 0, 30, 19, 15, 13, 11, 11, 10, 9, 9, 8, 8, 8, 8, 7, 7, 7, 7,
                                    7, 7, 7,  6,  6,  6,  6,  6,  6,  6, 6, 6, 6, 6, 6, 6, 6, 5};
    return table;
  }

  static const uint32_t* int_radix() {
    static const uint32_t table[] = {
        0,           0,           0x40000000U, 0x4546b3dbU, 0x40000000U, 0x48c27395U, 0x159fd800U, 0x75db9c97U,
        0x40000000U, 0x17179149U, 0x3b9aca00U, 0x0cc6db61U, 0x19a10000U, 0x309f1021U, 0x57f6c100U, 0x0a2f1b6fU,
        0x10000000U, 0x18754571U, 0x247dbc80U, 0x3547667bU, 0x4c4b4000U, 0x6b5a6e1dU, 0x06c20a40U, 0x08d2d931U,
        0x0b640000U, 0x0e8d4a51U, 0x1269ae40U, 0x17179149U, 0x1cb91000U, 0x23744899U, 0x2b73a840U, 0x34e63b41U,
        0x40000000U, 0x4cfa3cc1U, 0x5c13d840U, 0x6d91b519U, 0x039aa400U};
    return table;
  }

  static int32_t compare_magnitude(const jarray<uint32_t>& x, const jarray<uint32_t>& y) {
    if (x.length() < y.length()) {
      return -1;
    }
    if (x.length() > y.length()) {
      return 1;
    }
    for (int32_t i = 0; i < x.length(); ++i) {
      if (x[i] < y[i]) {
        return -1;
      }
      if (x[i] > y[i]) {
        return 1;
      }
    }
    return 0;
  }

  mutable_bigint to_mutable() const {
    return mutable_bigint(mag_);
  }

  static bigint from_mutable(mutable_bigint value, int32_t sign) {
    value.normalize();
    if (value.is_zero()) {
      return ZERO;
    }
    return bigint(sign, value.to_int_array());
  }

  static mutable_bigint add_magnitude(mutable_bigint x, mutable_bigint y) {
    x.add(y);
    return x;
  }

  static bool is_small_prime(uint32_t value) {
    if (value < 2) {
      return false;
    }
    if ((value & 1U) == 0) {
      return value == 2;
    }
    for (uint32_t d = 3; (uint64_t)d * d <= value; d += 2) {
      if (value % d == 0) {
        return false;
      }
    }
    return true;
  }
};

// ============================================================================
// bit_sieve.h
// ============================================================================

// 用于寻找素数候选值的简单 bit 筛.
//
// 这个类翻译自 Java BigInteger 的 BitSieve.筛中的 bit 被设置时表示对应
// 候选数已经被排除；清零的 bit 表示仍可能是素数.为了降低存储空间并提高
// 效率，筛中不表示偶数，每个 bit 只对应一个奇数：
//
//   N = offset + (2 * index + 1)
//
// 其中 N 是该 bit 表示的整数，offset 是筛开始处的偶数基准值，index 是筛
// 数组中的 bit 下标.
struct bit_sieve {
  // 构造“小筛”，基准值为 0.
  //
  // 这个构造函数用于生成小素数集合.主构造函数会利用这些小素数，把搜索
  // 筛中可确定为合数的候选值排除.长度沿用 Java 的经验值；它是在构造
  // 其他筛的耗时和后续素性测试浪费之间做出的折中.
  bit_sieve() : bits_(unit_index(k_small_sieve_length - 1) + 1), length_(k_small_sieve_length) {
    build_small_sieve();
  }

  // 构造指定长度的空筛.所有 bit 初始为 0，主要用于测试和局部筛操作.
  explicit bit_sieve(int32_t length) : bits_(unit_count(length)), length_(length) {
    if (length < 0) {
      throw std::invalid_argument("Negative sieve length");
    }
  }

  // 构造用于寻找素数候选值的搜索筛.base 必须是偶数.
  //
  // 候选值由清零 bit 表示.随着候选值被证明不是素数，对应 bit 会被设置并
  // 从搜索范围里排除.筛只表示奇数候选值.
  bit_sieve(const bigint& base, int32_t search_len) : bits_(unit_count(search_len)), length_(search_len) {
    if (search_len < 0) {
      throw std::invalid_argument("Negative sieve length");
    }
    if (base.is_odd()) {
      throw std::invalid_argument("BitSieve base must be even");
    }

    int32_t start = 0;
    const bit_sieve& small = small_sieve();
    int32_t step = small.sieve_search(small.length_, start);

    while (step > 0) {
      const int32_t converted_step = (step * 2) + 1;

      // 计算 base mod convertedStep.
      start = static_cast<int32_t>(base.mod_uint32(static_cast<uint32_t>(converted_step)));

      // 从搜索筛中删除每个 step 的倍数.
      start = converted_step - start;
      if (start % 2 == 0) {
        start += converted_step;
      }
      sieve_single(search_len, (start - 1) / 2, converted_step);

      // 从小筛中寻找下一个素数.
      step = small.sieve_search(small.length_, step + 1);
    }
  }

  // 显式声明拷贝构造，保持值对象语义.
  bit_sieve(const bit_sieve& other) : bits_(other.bits_), length_(other.length_) {
  }

  // 显式声明拷贝赋值，保持值对象语义.
  bit_sieve& operator=(const bit_sieve& other) {
    if (this == &other) {
      return *this;
    }
    bits_ = other.bits_;
    length_ = other.length_;
    return *this;
  }

  // 显式声明移动构造，移动后源筛长度归零.
  bit_sieve(bit_sieve&& other) noexcept : bits_(std::move(other.bits_)), length_(other.length_) {
    other.length_ = 0;
  }

  // 显式声明移动赋值，移动后源筛长度归零.
  bit_sieve& operator=(bit_sieve&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    bits_ = std::move(other.bits_);
    length_ = other.length_;
    other.length_ = 0;
    return *this;
  }

  // 返回保存指定 bit 的 64-bit 单元下标.
  static int32_t unit_index(int32_t bit_index) {
    return bit_index >> 6;
  }

  // 返回用于屏蔽指定 bit 的单元掩码.
  static uint64_t bit(int32_t bit_index) {
    return uint64_t{1} << (bit_index & ((1 << 6) - 1));
  }

  // 返回筛中 bit 的数量.
  int32_t length() const {
    return length_;
  }

  // 取得指定下标 bit 的值.
  bool get(int32_t bit_index) const {
    check_bit_index(bit_index);
    const int32_t word_index = unit_index(bit_index);
    return (bits_[word_index] & bit(bit_index)) != 0;
  }

  // 设置指定下标的 bit.
  void set(int32_t bit_index) {
    check_bit_index(bit_index);
    const int32_t word_index = unit_index(bit_index);
    bits_[word_index] |= bit(bit_index);
  }

  // 返回 start 处或其后第一个清零 bit 的下标，不搜索超过 limit.
  //
  // 如果不存在这样的 bit，返回 -1.
  int32_t sieve_search(int32_t limit, int32_t start) const {
    check_limit(limit);
    if (start < 0) {
      throw std::out_of_range("Negative sieve start");
    }
    if (start >= limit) {
      return -1;
    }

    for (int32_t index = start; index < limit; ++index) {
      if (!get(index)) {
        return index;
      }
    }
    return -1;
  }

  // 从指定 start 下标开始，把 step 的每个倍数都筛掉，直到达到 limit.
  void sieve_single(int32_t limit, int32_t start, int32_t step) {
    check_limit(limit);
    if (start < 0) {
      throw std::out_of_range("Negative sieve start");
    }
    if (step <= 0) {
      throw std::invalid_argument("Non-positive sieve step");
    }

    while (start < limit) {
      set(start);
      start += step;
    }
  }

  // 在筛中测试可能素数，并返回第一个通过测试的候选值.
  std::optional<bigint> retrieve(const bigint& init_value, int32_t certainty) const {
    int64_t offset = 1;
    for (int32_t i = 0; i < bits_.length(); ++i) {
      uint64_t next_word = ~bits_[i];
      for (int32_t j = 0; j < 64 && (i * 64 + j) < length_; ++j) {
        if ((next_word & 1U) == 1U) {
          bigint candidate = init_value.add(offset);
          if (candidate.prime_to_certainty(certainty)) {
            return candidate;
          }
        }
        next_word >>= 1;
        offset += 2;
      }
    }
    return std::nullopt;
  }

  // 返回全局小筛.搜索筛构造时使用它过滤小素数倍数.
  static const bit_sieve& small_sieve() {
    static const bit_sieve sieve;
    return sieve;
  }

  static constexpr int32_t k_small_sieve_length = 150 * 64;

  static int32_t unit_count(int32_t length) {
    if (length < 0) {
      return 0;
    }
    return length == 0 ? 0 : unit_index(length - 1) + 1;
  }

  void build_small_sieve() {
    // 标记 1 为合数.
    set(0);

    int32_t next_index = 1;
    int32_t next_prime = 3;

    // 寻找素数，并把它们的倍数从筛中删除.
    do {
      sieve_single(length_, next_index + next_prime, next_prime);
      next_index = sieve_search(length_, next_index + 1);
      next_prime = 2 * next_index + 1;
    } while ((next_index > 0) && (next_prime < length_));
  }

  void check_bit_index(int32_t bit_index) const {
    if (bit_index < 0 || bit_index >= length_) {
      throw std::out_of_range("BitSieve bit index out of range");
    }
  }

  void check_limit(int32_t limit) const {
    if (limit < 0 || limit > length_) {
      throw std::out_of_range("BitSieve limit out of range");
    }
  }

  // 存储当前筛的 bit.
  jarray<uint64_t> bits_;

  // 当前筛持有的 bit 数量.
  int32_t length_{0};
};

// ============================================================================
// bigint.h (continued)
// ============================================================================

inline const bigint bigint::ZERO{0};
inline const bigint bigint::ONE{1};
inline const bigint bigint::TWO{2};
inline const bigint bigint::TEN{10};
inline const bigint bigint::NEGATIVE_ONE{-1};
