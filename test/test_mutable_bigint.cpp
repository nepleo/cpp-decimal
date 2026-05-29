// C++ mutable_bigint 与 JDK MutableBigInteger 对拍（MUTABLE_BIGINT_JAVA=1 启用 harness）。
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bigdecimal.h"

static std::string mutable_bigint_state(const mutable_bigint& m) {
  std::ostringstream os;
  os << "int_len=" << m.int_len_;
  os << " offset=" << m.offset_;
  os << " buf_len=" << m.value_.length();
  os << " limbs=";
  for (int32_t i = 0; i < m.int_len_; ++i) {
    if (i > 0) {
      os << ',';
    }
    os << m.value_[m.offset_ + i];
  }
  return os.str();
}

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

static bool java_harness_enabled() {
  const char* flag = std::getenv("MUTABLE_BIGINT_JAVA");
  return flag != nullptr && std::string(flag) != "0";
}

static std::filesystem::path java_harness_script() {
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "out" / "run_java_harness.sh";
}

static std::string shell_quote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += '\'';
  return out;
}

static std::string shell_escape_command(const std::string& command) {
  std::istringstream iss(command);
  std::string token;
  std::ostringstream os;
  bool first = true;
  while (iss >> token) {
    if (!first) {
      os << ' ';
    }
    first = false;
    if (token.find(',') != std::string::npos) {
      os << shell_quote(token);
    } else {
      os << token;
    }
  }
  return os.str();
}

static std::string run_java_harness(const std::string& command) {
  const auto script = java_harness_script();
  if (!std::filesystem::exists(script)) {
    return {};
  }

  const std::string shell_cmd = "bash \"" + script.string() + "\" " +
                                shell_escape_command(command) + " 2>/dev/null";
  std::FILE* pipe = popen(shell_cmd.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }

  std::string output;
  char buffer[256];
  while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }
  pclose(pipe);

  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }
  return output;
}

static jarray<uint32_t> mag_from_csv(const std::string& csv) {
  if (csv.empty() || csv == "empty") {
    return jarray<uint32_t>();
  }
  std::vector<uint32_t> parts;
  std::stringstream ss(csv);
  std::string item;
  while (std::getline(ss, item, ',')) {
    parts.push_back(static_cast<uint32_t>(std::stoul(item)));
  }
  jarray<uint32_t> mag(static_cast<int>(parts.size()));
  for (int i = 0; i < mag.length(); ++i) {
    mag[i] = parts[static_cast<size_t>(i)];
  }
  return mag;
}

static mutable_bigint bigint_from_mag(const std::string& csv) {
  if (csv.empty() || csv == "empty") {
    return mutable_bigint();
  }
  return mutable_bigint(mag_from_csv(csv));
}

static std::string format_limbs(const jarray<uint32_t>& mag) {
  std::ostringstream os;
  os << "limbs=";
  for (int i = 0; i < mag.length(); ++i) {
    if (i > 0) {
      os << ',';
    }
    os << mag[i];
  }
  return os.str();
}

static std::string format_limbs_normalized(const mutable_bigint& m) {
  mutable_bigint t = m;
  t.normalize();
  return format_limbs(t.get_magnitude_array());
}

static std::string format_result(int32_t v) {
  return "result=" + std::to_string(v);
}

static std::string format_flag(bool v) {
  return std::string("flag=") + (v ? "true" : "false");
}

static std::string format_u64(uint64_t v) {
  return "u64=" + std::to_string(v);
}

static std::string format_u32(uint32_t v) {
  return "u32=" + std::to_string(v);
}

static std::string bench_mag_csv(int limbs) {
  std::ostringstream os;
  for (int i = 0; i < limbs; ++i) {
    if (i > 0) {
      os << ',';
    }
    os << (0x9e3779b9u + static_cast<uint32_t>(i) * 0x85ebca6bu);
  }
  return os.str();
}

class MutableBigIntTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!java_harness_enabled()) {
      return;
    }
    if (!std::filesystem::exists(java_harness_script())) {
      java_available_ = false;
      return;
    }
    if (run_java_harness("ping") != "ok") {
      java_available_ = false;
    }
  }

  void expect_java_line(const std::string& cmd, const std::string& expected) {
    if (!java_harness_enabled()) {
      return;
    }
    if (!java_available_) {
      GTEST_SKIP() << "Java harness unavailable; run: bash out/run_java_harness.sh ping";
    }
    const std::string actual = run_java_harness(cmd);
    if (actual.empty()) {
      GTEST_SKIP() << "Java harness returned no output for: " << cmd;
    }
    EXPECT_EQ(actual, expected) << "cmd: " << cmd;
  }

  // C++ 与 JDK 在同一场景下必须得到相同的一行输出。
  void assert_parity(const std::string& cmd, const std::string& cpp_line) {
    if (!java_harness_enabled()) {
      return;
    }
    if (!java_available_) {
      GTEST_SKIP() << "Java harness unavailable; run: bash out/run_java_harness.sh ping";
    }
    const std::string java_line = run_java_harness(cmd);
    if (java_line.empty()) {
      GTEST_SKIP() << "Java harness returned no output for: " << cmd;
    }
    EXPECT_EQ(java_line, cpp_line) << "cmd: " << cmd;
  }

  void assert_state_parity(const std::string& cmd, const mutable_bigint& m) {
    assert_parity(cmd, mutable_bigint_state(m));
  }

  void assert_subtract_parity(const std::string& mag_a, const std::string& mag_b) {
    mutable_bigint a = bigint_from_mag(mag_a);
    const mutable_bigint b = bigint_from_mag(mag_b);
    const int32_t sign = a.subtract(b);
    const std::string cmd_base = "result_subtract " + mag_a + " " + mag_b;
    const std::string state_cmd = "state_subtract " + mag_a + " " + mag_b;
    assert_parity(cmd_base, format_result(sign));
    assert_state_parity(state_cmd, a);
  }

  void assert_difference_parity(const std::string& mag_a, const std::string& mag_b) {
    mutable_bigint a = bigint_from_mag(mag_a);
    mutable_bigint b = bigint_from_mag(mag_b);
    const int32_t sign = a.difference(b);
    assert_parity("result_difference " + mag_a + " " + mag_b, format_result(sign));
    mutable_bigint& winner = sign < 0 ? b : a;
    assert_state_parity("state_difference_winner " + mag_a + " " + mag_b, winner);
  }

  void assert_divide_parity(const std::string& mag_a, const std::string& mag_b) {
    mutable_bigint a = bigint_from_mag(mag_a);
    mutable_bigint b = bigint_from_mag(mag_b);
    mutable_bigint q;
    mutable_bigint r = a.divide(b, q);
    q.normalize();
    r.normalize();
    const std::string cmd_q = "state_divide_quotient " + mag_a + " " + mag_b;
    const std::string cmd_r = "state_divide_remainder " + mag_a + " " + mag_b;
    assert_state_parity(cmd_q, q);
    assert_state_parity(cmd_r, r);
  }

  bool java_available_{true};
};

// --- 构造 / 生命周期 smoke ---

TEST_F(MutableBigIntTest, DefaultConstructorSmoke) {
  EXPECT_NO_THROW({ mutable_bigint n; });
}

TEST_F(MutableBigIntTest, UintConstructorSmoke) {
  EXPECT_NO_THROW({
    mutable_bigint z(0u);
    mutable_bigint o(1u);
    mutable_bigint h(0x80000000u);
    (void)z;
    (void)o;
    (void)h;
  });
}

TEST_F(MutableBigIntTest, JarrayConstructorSmoke) {
  jarray<uint32_t> mag = {0x12345678u, 0x9abcdef0u};
  EXPECT_NO_THROW({ mutable_bigint n(mag); });
}

TEST_F(MutableBigIntTest, CopyAndMoveSmoke) {
  EXPECT_NO_THROW({
    mutable_bigint a(42u);
    mutable_bigint b(a);
    mutable_bigint c;
    c = a;
    mutable_bigint d(std::move(b));
    mutable_bigint e;
    e = std::move(c);
    a.clear();
    d.reset();
    (void)e;
  });
}

// --- 谓词 ---

TEST_F(MutableBigIntTest, ParityIsZero) {
  EXPECT_TRUE(mutable_bigint().is_zero());
  assert_parity("flag is_zero empty", format_flag(true));

  mutable_bigint z(0u);
  EXPECT_FALSE(z.is_zero());
  assert_parity("flag is_zero 0", format_flag(false));
}

TEST_F(MutableBigIntTest, ParityIsOne) {
  EXPECT_TRUE(mutable_bigint(1u).is_one());
  assert_parity("flag is_one 1", format_flag(true));

  EXPECT_FALSE(mutable_bigint(2u).is_one());
  assert_parity("flag is_one 2", format_flag(false));
}

TEST_F(MutableBigIntTest, ParityIsEvenOdd) {
  EXPECT_TRUE(mutable_bigint().is_even());
  assert_parity("flag is_even empty", format_flag(true));

  EXPECT_TRUE(mutable_bigint(2u).is_even());
  assert_parity("flag is_even 2", format_flag(true));

  EXPECT_TRUE(mutable_bigint(3u).is_odd());
  assert_parity("flag is_odd 3", format_flag(true));

  EXPECT_FALSE(mutable_bigint(4u).is_odd());
  assert_parity("flag is_odd 4", format_flag(false));
}

TEST_F(MutableBigIntTest, ParityIsNormal) {
  mutable_bigint n(5u);
  EXPECT_TRUE(n.is_normal());
  assert_parity("flag is_normal 5", format_flag(true));

  mutable_bigint leading(mag_from_csv("0,0,5"));
  EXPECT_FALSE(leading.is_normal());
  assert_parity("flag is_normal 0,0,5", format_flag(false));
}

// --- normalize / limbs ---

TEST_F(MutableBigIntTest, ParityNormalize) {
  mutable_bigint m(mag_from_csv("0,0,5"));
  m.normalize();
  EXPECT_FALSE(m.is_zero());
  assert_parity("limbs_normalize 0,0,5", format_limbs(m.to_int_array()));

  mutable_bigint z(mag_from_csv("0,0,0"));
  z.normalize();
  EXPECT_TRUE(z.is_zero());
  assert_parity("limbs_normalize 0,0,0", format_limbs(z.to_int_array()));
}

// --- compare ---

TEST_F(MutableBigIntTest, ParityCompareInt) {
  mutable_bigint a(42u);
  mutable_bigint b(43u);
  EXPECT_EQ(a.compare(b), -1);
  assert_parity("result_compare_int 42 43", format_result(-1));

  EXPECT_EQ(a.compare(a), 0);
  assert_parity("result_compare_int 42 42", format_result(0));

  mutable_bigint big(mag_from_csv("1,0"));
  mutable_bigint small(1u);
  EXPECT_EQ(small.compare(big), -1);
  assert_parity("result_compare_mag 1 1,0", format_result(-1));
}

TEST_F(MutableBigIntTest, ParityCompareShifted) {
  mutable_bigint a(mag_from_csv("2,3,4"));
  mutable_bigint b(mag_from_csv("2,3"));
  const int32_t r = a.compare_shifted(b, 1);
  EXPECT_EQ(r, 0);
  assert_parity("result_compare_shifted 2,3,4 2,3 1", format_result(r));
}

TEST_F(MutableBigIntTest, ParityCompareHalf) {
  mutable_bigint zero;
  EXPECT_EQ(zero.compare_half(zero), 0);
  assert_parity("result_compare_half empty empty", format_result(0));

  mutable_bigint one(1u);
  EXPECT_EQ(zero.compare_half(one), -1);
  assert_parity("result_compare_half empty 1", format_result(-1));

  mutable_bigint two(2u);
  mutable_bigint three(3u);
  const int32_t r = two.compare_half(three);
  EXPECT_EQ(r, 1);
  assert_parity("result_compare_half 2 3", format_result(r));
}

// --- copy_value / set_* ---

TEST_F(MutableBigIntTest, ParityCopyValueInt) {
  mutable_bigint dst;
  dst.copy_value(mutable_bigint(55u));
  assert_state_parity("state_copy_value_int 55", dst);
  assert_parity("limbs_to_int_array 55", format_limbs(dst.to_int_array()));
}

TEST_F(MutableBigIntTest, ParityCopyValueMag) {
  mutable_bigint dst;
  const jarray<uint32_t> mag = mag_from_csv("305419896,2596069104");
  dst.copy_value(mag);
  assert_state_parity("state_copy_value_mag 305419896,2596069104", dst);
}

TEST_F(MutableBigIntTest, ParityCopyValueMbi) {
  mutable_bigint src(mag_from_csv("0,0,9"));
  mutable_bigint dst;
  dst.copy_value(src);
  assert_state_parity("state_copy_value_mbi 0,0,9", dst);
}

TEST_F(MutableBigIntTest, ParitySetInt) {
  mutable_bigint m(10u);
  m.set_int(0, 99u);
  assert_state_parity("state_set_int 10 0 99", m);
}

TEST_F(MutableBigIntTest, ParitySetValue) {
  mutable_bigint m;
  const jarray<uint32_t> mag = mag_from_csv("7,8,9,10");
  m.set_value(mag, 2);
  assert_state_parity("state_set_value 7,8,9,10 2", m);
}

// --- to_int_array / get_magnitude_array ---

TEST_F(MutableBigIntTest, ParityToIntArray) {
  mutable_bigint m(mag_from_csv("305419896,2596069104"));
  assert_parity("limbs_to_int_array 305419896,2596069104",
                format_limbs(m.to_int_array()));
}

TEST_F(MutableBigIntTest, ParityGetMagnitudeArray) {
  mutable_bigint m(mag_from_csv("0,0,5"));
  m.normalize();
  (void)m.get_magnitude_array();
  EXPECT_TRUE(m.is_normal());
  assert_state_parity("state_get_magnitude 0,0,5", m);
}

// --- bit_length / lowest_set_bit / to_long / get_int ---

TEST_F(MutableBigIntTest, ParityBitLength) {
  assert_parity("u64 bit_length empty", format_u64(mutable_bigint().bit_length()));

  mutable_bigint one(1u);
  assert_parity("u64 bit_length 1", format_u64(one.bit_length()));

  mutable_bigint five(5u);
  assert_parity("u64 bit_length 5", format_u64(five.bit_length()));

  mutable_bigint hi(0x80000000u);
  assert_parity("u64 bit_length 2147483648", format_u64(hi.bit_length()));

  mutable_bigint two_limbs(mag_from_csv("1,0"));
  assert_parity("u64 bit_length 1,0", format_u64(two_limbs.bit_length()));
}

TEST_F(MutableBigIntTest, ParityLowestSetBit) {
  assert_parity("i32 lowest_set_bit empty",
                format_result(mutable_bigint().get_lowest_set_bit()));

  mutable_bigint eight(8u);
  assert_parity("i32 lowest_set_bit 8",
                format_result(eight.get_lowest_set_bit()));

  mutable_bigint leading(mag_from_csv("0,0,8"));
  assert_parity("i32 lowest_set_bit 0,0,8",
                format_result(leading.get_lowest_set_bit()));
}

TEST_F(MutableBigIntTest, ParityToLong) {
  mutable_bigint small(42u);
  assert_parity("u64 to_long 42", format_u64(small.to_long()));

  mutable_bigint wide(mag_from_csv("1,4294967295"));
  assert_parity("u64 to_long 1,4294967295", format_u64(wide.to_long()));
}

TEST_F(MutableBigIntTest, ParityGetInt) {
  mutable_bigint m(mag_from_csv("305419896,2596069104"));
  std::ostringstream os;
  os << "u32=" << m.get_int(1);
  assert_parity("u32_at 305419896,2596069104 1", os.str());
}

TEST_F(MutableBigIntTest, ParityClearAndReset) {
  mutable_bigint cleared(55u);
  cleared.clear();
  assert_state_parity("clear_after_ctor 55", cleared);

  mutable_bigint resetted(55u);
  resetted.reset();
  assert_state_parity("reset_after_ctor 55", resetted);
}

// --- primitive / right / left / safe_* shift ---

TEST_F(MutableBigIntTest, ParityPrimitiveRightShift) {
  mutable_bigint m(16u);
  m.primitive_right_shift(2);
  assert_state_parity("state_shift primitive_right_shift 16 2", m);

  mutable_bigint hi(0x80000000u);
  hi.primitive_right_shift(1);
  assert_state_parity("state_shift primitive_right_shift 2147483648 1", hi);
}

TEST_F(MutableBigIntTest, ParityPrimitiveLeftShift) {
  mutable_bigint m(1u);
  m.primitive_left_shift(3);
  assert_state_parity("state_shift primitive_left_shift 1 3", m);

  mutable_bigint wide(mag_from_csv("1,1"));
  wide.primitive_left_shift(4);
  assert_state_parity("state_shift primitive_left_shift 1,1 4", wide);
}

TEST_F(MutableBigIntTest, ParityRightShift) {
  mutable_bigint eight(8u);
  eight.right_shift(1);
  assert_state_parity("state_shift right_shift 8 1", eight);

  mutable_bigint hi(0x80000000u);
  hi.right_shift(1);
  assert_state_parity("state_shift right_shift 2147483648 1", hi);

  mutable_bigint two_limbs(mag_from_csv("1,0"));
  two_limbs.right_shift(32);
  assert_state_parity("state_shift right_shift 1,0 32", two_limbs);

  mutable_bigint odd(12u);
  odd.right_shift(2);
  assert_state_parity("state_shift right_shift 12 2", odd);
}

TEST_F(MutableBigIntTest, ParityLeftShift) {
  mutable_bigint five(5u);
  five.left_shift(2);
  assert_state_parity("state_shift left_shift 5 2", five);

  mutable_bigint one(1u);
  one.left_shift(32);
  assert_state_parity("state_shift left_shift 1 32", one);

  mutable_bigint multi(mag_from_csv("1,1"));
  multi.left_shift(5);
  assert_state_parity("state_shift left_shift 1,1 5", multi);
}

TEST_F(MutableBigIntTest, ParitySafeRightShift) {
  mutable_bigint small(8u);
  small.safe_right_shift(3);
  assert_state_parity("state_shift safe_right_shift 8 3", small);

  mutable_bigint wiped(mag_from_csv("1,0"));
  wiped.safe_right_shift(64);
  assert_state_parity("state_shift safe_right_shift 1,0 64", wiped);
}

TEST_F(MutableBigIntTest, ParitySafeLeftShift) {
  mutable_bigint unchanged(7u);
  unchanged.safe_left_shift(0);
  assert_state_parity("state_shift safe_left_shift 7 0", unchanged);

  mutable_bigint doubled(3u);
  doubled.safe_left_shift(1);
  assert_state_parity("state_shift safe_left_shift 3 1", doubled);
}

TEST_F(MutableBigIntTest, ParityShiftOnEmpty) {
  mutable_bigint z;
  z.right_shift(5);
  assert_state_parity("state_shift right_shift empty 5", z);

  mutable_bigint z2;
  z2.left_shift(10);
  assert_state_parity("state_shift left_shift empty 10", z2);

  mutable_bigint z3;
  z3.safe_right_shift(100);
  assert_state_parity("state_shift safe_right_shift empty 100", z3);
}

TEST_F(MutableBigIntTest, ParityRightShiftWordAndBitBoundaries) {
  mutable_bigint all_bits(0xffffffffu);
  all_bits.right_shift(31);
  assert_state_parity("state_shift right_shift 4294967295 31", all_bits);

  mutable_bigint all_bits2(0xffffffffu);
  all_bits2.right_shift(32);
  assert_state_parity("state_shift right_shift 4294967295 32", all_bits2);

  mutable_bigint three(mag_from_csv("2,0,0"));
  three.right_shift(33);
  assert_state_parity("state_shift right_shift 2,0,0 33", three);

  mutable_bigint pow2(0x80000000u);
  pow2.right_shift(31);
  assert_state_parity("state_shift right_shift 2147483648 31", pow2);
}

TEST_F(MutableBigIntTest, ParityLeftShiftSmallAndGrow) {
  mutable_bigint small(3u);
  small.left_shift(4);
  assert_state_parity("state_shift left_shift 3 4", small);

  mutable_bigint near_top(0x40000000u);
  near_top.left_shift(1);
  assert_state_parity("state_shift left_shift 1073741824 1", near_top);

  mutable_bigint max32(0xffffffffu);
  max32.left_shift(1);
  assert_state_parity("state_shift left_shift 4294967295 1", max32);

  mutable_bigint two(mag_from_csv("1,1"));
  two.left_shift(33);
  assert_state_parity("state_shift left_shift 1,1 33", two);
}

TEST_F(MutableBigIntTest, ParityLeftShiftWithSpareBuffer) {
  mutable_bigint m;
  m.set_value(mag_from_csv("1,0,0,0"), 1);
  m.left_shift(32);
  assert_state_parity("state_shift_set left_shift 1,0,0,0 1 32", m);

  mutable_bigint m2;
  m2.set_value(mag_from_csv("5,0,0"), 1);
  m2.left_shift(16);
  assert_state_parity("state_shift_set left_shift 5,0,0 1 16", m2);
}

TEST_F(MutableBigIntTest, ParitySafeShiftBoundaries) {
  mutable_bigint one(1u);
  one.safe_right_shift(32);
  assert_state_parity("state_shift safe_right_shift 1 32", one);

  mutable_bigint pair(mag_from_csv("1,0"));
  pair.safe_right_shift(32);
  assert_state_parity("state_shift safe_right_shift 1,0 32", pair);

  pair = mag_from_csv("1,0");
  pair.safe_right_shift(31);
  assert_state_parity("state_shift safe_right_shift 1,0 31", pair);

  mutable_bigint big(mag_from_csv("2,0,0"));
  big.safe_right_shift(96);
  assert_state_parity("state_shift safe_right_shift 2,0,0 96", big);
}

TEST_F(MutableBigIntTest, ParityPrimitiveShiftExtremes) {
  mutable_bigint m(mag_from_csv("0,1"));
  m.primitive_right_shift(1);
  assert_state_parity("state_shift primitive_right_shift 0,1 1", m);

  mutable_bigint wide(mag_from_csv("3,0"));
  wide.primitive_left_shift(31);
  assert_state_parity("state_shift primitive_left_shift 3,0 31", wide);

  mutable_bigint tri(mag_from_csv("1,2,3"));
  tri.primitive_right_shift(4);
  assert_state_parity("state_shift primitive_right_shift 1,2,3 4", tri);
}

TEST_F(MutableBigIntTest, ParityCompareShiftedMore) {
  mutable_bigint a(mag_from_csv("9,8,7"));
  mutable_bigint b(mag_from_csv("9,8"));
  const int32_t r = a.compare_shifted(b, 1);
  assert_parity("result_compare_shifted 9,8,7 9,8 1", format_result(r));

  mutable_bigint c(mag_from_csv("1,0"));
  mutable_bigint d(1u);
  EXPECT_EQ(c.compare_shifted(d, 1), 0);
  assert_parity("result_compare_shifted 1,0 1 1", format_result(0));
}

TEST_F(MutableBigIntTest, ParityGetLongAndBitLengthForLimb) {
  mutable_bigint m(mag_from_csv("305419896,2596069104"));
  std::ostringstream os;
  os << "u64=" << m.get_long(0);
  assert_parity("u64 get_long 305419896,2596069104", os.str());

  EXPECT_EQ(mutable_bigint::bit_length_for_limb(0), 0);
  EXPECT_EQ(mutable_bigint::bit_length_for_limb(1), 1);
  EXPECT_EQ(mutable_bigint::bit_length_for_limb(0x80000000u), 32);
}

// --- add ---

TEST_F(MutableBigIntTest, ParityAddBasic) {
  mutable_bigint a(1u);
  a.add(mutable_bigint(1u));
  assert_state_parity("state_add 1 1", a);

  mutable_bigint five(5u);
  five.add(mutable_bigint(7u));
  assert_state_parity("state_add 5 7", five);

  mutable_bigint zero;
  zero.add(mutable_bigint(42u));
  assert_state_parity("state_add empty 42", zero);
}

TEST_F(MutableBigIntTest, ParityAddCarryAndMultiLimb) {
  mutable_bigint a(0xffffffffu);
  a.add(mutable_bigint(1u));
  assert_state_parity("state_add 4294967295 1", a);

  mutable_bigint b(mag_from_csv("1,0"));
  b.add(mutable_bigint(1u));
  assert_state_parity("state_add 1,0 1", b);

  mutable_bigint c(mag_from_csv("305419896,2596069104"));
  c.add(mutable_bigint(mag_from_csv("1,1")));
  assert_state_parity("state_add 305419896,2596069104 1,1", c);

  mutable_bigint d(mag_from_csv("0,1"));
  d.add(mutable_bigint(mag_from_csv("0,1")));
  assert_state_parity("state_add 0,1 0,1", d);
}

TEST_F(MutableBigIntTest, ParityAddInPlaceBuffer) {
  mutable_bigint a;
  a.set_value(mag_from_csv("9,0,0,0"), 1);
  a.add(mutable_bigint(1u));
  assert_state_parity("state_add_set 9,0,0,0 1 1", a);

  mutable_bigint b;
  b.set_value(mag_from_csv("1,0,0"), 1);
  b.add(mutable_bigint(mag_from_csv("0,1")));
  assert_state_parity("state_add_set 1,0,0 1 0,1", b);
}

TEST_F(MutableBigIntTest, ParityAddNoOpAddend) {
  mutable_bigint a(99u);
  a.add(mutable_bigint());
  assert_state_parity("state_add 99 empty", a);
}

TEST_F(MutableBigIntTest, ParityAddMoreCases) {
  mutable_bigint a(3u);
  a.add(mutable_bigint(5u));
  assert_state_parity("state_add 3 5", a);

  mutable_bigint b(5u);
  b.add(mutable_bigint(3u));
  assert_state_parity("state_add 5 3", b);

  mutable_bigint z1(0u);
  z1.add(mutable_bigint(0u));
  assert_state_parity("state_add 0 0", z1);

  mutable_bigint hi(0x80000000u);
  hi.add(mutable_bigint(0x80000000u));
  assert_state_parity("state_add 2147483648 2147483648", hi);

  mutable_bigint wide(mag_from_csv("2,0"));
  wide.add(mutable_bigint(mag_from_csv("1,0")));
  assert_state_parity("state_add 2,0 1,0", wide);
}

// --- subtract ---

TEST_F(MutableBigIntTest, ParitySubtractBasic) {
  assert_subtract_parity("10", "3");
  assert_subtract_parity("3", "10");
  assert_subtract_parity("10", "10");
}

TEST_F(MutableBigIntTest, ParitySubtractBorrow) {
  assert_subtract_parity("1,0", "1");
  assert_subtract_parity("4294967295", "1");
  assert_subtract_parity("2", "1");
}

TEST_F(MutableBigIntTest, ParitySubtractMultiLimb) {
  assert_subtract_parity("305419896,2596069104", "1,1");
  assert_subtract_parity("1,1", "305419896,2596069104");
  assert_subtract_parity("0,2", "0,1");
  assert_subtract_parity("2,0", "1,0");
}

TEST_F(MutableBigIntTest, ParitySubtractWithEmpty) {
  assert_subtract_parity("42", "empty");
  assert_subtract_parity("empty", "42");
  assert_subtract_parity("empty", "empty");
}

TEST_F(MutableBigIntTest, ParitySubtractInPlaceBuffer) {
  mutable_bigint a;
  a.set_value(mag_from_csv("10,0,0,0"), 1);
  const int32_t sign = a.subtract(mutable_bigint(3u));
  ASSERT_EQ(sign, 1);
  assert_state_parity("state_subtract_set 10,0,0,0 1 3", a);

  mutable_bigint b;
  b.set_value(mag_from_csv("5,0,0"), 1);
  const int32_t sign2 = b.subtract(mutable_bigint(8u));
  ASSERT_EQ(sign2, -1);
  assert_state_parity("state_subtract_set 5,0,0 1 8", b);
}

// --- difference（#40）：结果写在较大的操作数上 ---

TEST_F(MutableBigIntTest, ParityDifferenceBasic) {
  assert_difference_parity("10", "3");
  assert_difference_parity("3", "10");
}

TEST_F(MutableBigIntTest, ParityDifferenceEqual) {
  assert_difference_parity("7", "7");
}

TEST_F(MutableBigIntTest, ParityDifferenceBorrow) {
  assert_difference_parity("1,0", "1");
  assert_difference_parity("4294967295", "1");
}

TEST_F(MutableBigIntTest, ParityDifferenceMultiLimb) {
  assert_difference_parity("305419896,2596069104", "1,1");
  assert_difference_parity("1,1", "305419896,2596069104");
}

// --- add_disjoint（#41）---

TEST_F(MutableBigIntTest, ParityAddDisjointConcat) {
  mutable_bigint a(5u);
  a.add_disjoint(mutable_bigint(3u), 2);
  assert_state_parity("state_add_disjoint 5 2 3", a);

  mutable_bigint low(9u);
  low.add_disjoint(mutable_bigint(1u), 1);
  assert_state_parity("state_add_disjoint 9 1 1", low);
}

TEST_F(MutableBigIntTest, ParityAddDisjointTwoLimb) {
  mutable_bigint a(mag_from_csv("1,2"));
  a.add_disjoint(mutable_bigint(mag_from_csv("4,3")), 2);
  assert_state_parity("state_add_disjoint 1,2 2 4,3", a);
}

TEST_F(MutableBigIntTest, ParityAddDisjointFromEmpty) {
  mutable_bigint z;
  z.add_disjoint(mutable_bigint(7u), 2);
  assert_state_parity("state_add_disjoint empty 2 7", z);
}

TEST_F(MutableBigIntTest, ParityAddDisjointNoOp) {
  mutable_bigint a(11u);
  a.add_disjoint(mutable_bigint(), 3);
  assert_state_parity("state_add_disjoint 11 3 empty", a);
}

// --- add_shifted（#42）/ add_lower（#43）---

TEST_F(MutableBigIntTest, ParityAddShifted) {
  mutable_bigint a(5u);
  a.add_shifted(mutable_bigint(3u), 1);
  assert_state_parity("state_add_shifted 5 1 3", a);

  mutable_bigint b(mag_from_csv("1,0"));
  b.add_shifted(mutable_bigint(1u), 1);
  assert_state_parity("state_add_shifted 1,0 1 1", b);

  mutable_bigint c;
  c.add_shifted(mutable_bigint(7u), 2);
  assert_state_parity("state_add_shifted empty 2 7", c);
}

TEST_F(MutableBigIntTest, ParityAddLower) {
  mutable_bigint a(10u);
  a.add_lower(mutable_bigint(8u), 1);
  assert_state_parity("state_add_lower 10 1 8", a);

  mutable_bigint b;
  b.add_lower(mutable_bigint(mag_from_csv("7,8,9")), 2);
  assert_state_parity("state_add_lower 0 2 7,8,9", b);

  mutable_bigint c(mag_from_csv("1,0"));
  c.add_lower(mutable_bigint(mag_from_csv("0,1")), 1);
  assert_state_parity("state_add_lower 1,0 1 0,1", c);
}

// --- mul / multiply（#44–#45）---

TEST_F(MutableBigIntTest, ParityMulBasic) {
  mutable_bigint z;
  mutable_bigint(5u).mul(2u, z);
  assert_state_parity("state_mul 5 2", z);

  mutable_bigint z1;
  mutable_bigint(0xffffffffu).mul(1u, z1);
  assert_state_parity("state_mul 4294967295 1", z1);

  mutable_bigint z2;
  mutable_bigint(0xffffffffu).mul(2u, z2);
  assert_state_parity("state_mul 4294967295 2", z2);
}

TEST_F(MutableBigIntTest, ParityMulMultiLimbAndEdge) {
  mutable_bigint z;
  bigint_from_mag("1,0").mul(3u, z);
  assert_state_parity("state_mul 1,0 3", z);

  mutable_bigint z0;
  mutable_bigint(99u).mul(0u, z0);
  assert_state_parity("state_mul 99 0", z0);

  mutable_bigint zcopy;
  mutable_bigint(42u).mul(1u, zcopy);
  assert_state_parity("state_mul 42 1", zcopy);
}

TEST_F(MutableBigIntTest, ParityMultiplyBasic) {
  mutable_bigint z;
  mutable_bigint(5u).multiply(mutable_bigint(7u), z);
  assert_state_parity("state_multiply 5 7", z);

  mutable_bigint z2;
  mutable_bigint(0xffffu).multiply(mutable_bigint(0xffffu), z2);
  assert_state_parity("state_multiply 65535 65535", z2);
}

TEST_F(MutableBigIntTest, ParityMultiplyMultiLimb) {
  mutable_bigint z;
  bigint_from_mag("1,0").multiply(mutable_bigint(1u), z);
  assert_state_parity("state_multiply 1,0 1", z);

  mutable_bigint z2;
  bigint_from_mag("2,1").multiply(mutable_bigint(mag_from_csv("4,3")), z2);
  assert_state_parity("state_multiply 2,1 4,3", z2);

  mutable_bigint z3;
  bigint_from_mag("305419896,2596069104")
      .multiply(mutable_bigint(mag_from_csv("1,1")), z3);
  assert_state_parity("state_multiply 305419896,2596069104 1,1", z3);
}

// JDK multiply 在任一操作数为 empty（intLen==0）时会越界，与 C++ 相同；暂不测 empty 组合。

// --- ensure_capacity（#7）---

TEST_F(MutableBigIntTest, ParityEnsureCapacityGrow) {
  mutable_bigint m;
  m.ensure_capacity(4);
  assert_state_parity("state_ensure_capacity empty 4", m);
}

TEST_F(MutableBigIntTest, ParityEnsureCapacityNoGrow) {
  mutable_bigint m(42u);
  m.ensure_capacity(1);
  assert_state_parity("state_ensure_capacity 42 1", m);
}

// --- 静态位运算 / ONE / get_int·get_long 补充 ---

TEST_F(MutableBigIntTest, ParityNumberOfTrailingZeros) {
  assert_parity("i32_ntz 0", format_result(mutable_bigint::number_of_trailing_zeros(0)));
  assert_parity("i32_ntz 8", format_result(mutable_bigint::number_of_trailing_zeros(8)));
  assert_parity("i32_ntz 4294967295",
                format_result(mutable_bigint::number_of_trailing_zeros(0xffffffffu)));
}

TEST_F(MutableBigIntTest, ParityNumberOfLeadingZeros) {
  assert_parity("i32_nlz 0", format_result(mutable_bigint::number_of_leading_zeros(0)));
  assert_parity("i32_nlz 2147483648",
                format_result(mutable_bigint::number_of_leading_zeros(0x80000000u)));
  assert_parity("i32_nlz 1", format_result(mutable_bigint::number_of_leading_zeros(1)));
}

TEST_F(MutableBigIntTest, ParityOneConstant) {
  assert_state_parity("state_one", mutable_bigint(1u));
}

TEST_F(MutableBigIntTest, ParityGetIntAndGetLongMore) {
  mutable_bigint m(mag_from_csv("7,8,9"));
  std::ostringstream os_int;
  os_int << "u32=" << m.get_int(2);
  assert_parity("u32_at 7,8,9 2", os_int.str());

  std::ostringstream os_long;
  os_long << "u64=" << m.get_long(1);
  assert_parity("u64_at 7,8,9 1", os_long.str());
}

TEST_F(MutableBigIntTest, ParityBitLengthAndLowestSetBitMore) {
  mutable_bigint zero_limb(0u);
  assert_parity("u64 bit_length 0", format_u64(zero_limb.bit_length()));

  mutable_bigint all_ones(0xffffffffu);
  assert_parity("u64 bit_length 4294967295", format_u64(all_ones.bit_length()));
  assert_parity("i32 lowest_set_bit 4294967295",
                format_result(all_ones.get_lowest_set_bit()));

  mutable_bigint tri(mag_from_csv("0,0,4"));
  assert_parity("i32 lowest_set_bit 0,0,4",
                format_result(tri.get_lowest_set_bit()));
}

// --- 构造：C++ 与 JDK state 对拍 ---

TEST_F(MutableBigIntTest, ParityDefaultCtor) {
  assert_state_parity("default_ctor", mutable_bigint());
}

TEST_F(MutableBigIntTest, ParityCtorInt) {
  assert_state_parity("ctor_int 0", mutable_bigint(0u));
  assert_state_parity("ctor_int 1", mutable_bigint(1u));
  assert_state_parity("ctor_int -2147483648", mutable_bigint(0x80000000u));
  assert_state_parity("ctor_int 42", mutable_bigint(42u));
}

TEST_F(MutableBigIntTest, ParityCtorArrayAndCopy) {
  assert_state_parity("ctor_array 305419896,2596069104",
                      bigint_from_mag("305419896,2596069104"));
  const mutable_bigint a(42u);
  const mutable_bigint b(a);
  assert_state_parity("copy_ctor 42", b);
}

// --- 原有 JDK 黄金用例（仅 Java 侧，无对应 C++ API 时保留）---

TEST_F(MutableBigIntTest, CppSmokeWithJdkDefaultCtor) {
  EXPECT_NO_THROW({ mutable_bigint n; });
  assert_state_parity("default_ctor", mutable_bigint());
}

TEST_F(MutableBigIntTest, CppSmokeWithJdkCopyCtor) {
  EXPECT_NO_THROW({
    mutable_bigint a(42u);
    mutable_bigint b(a);
    (void)b;
  });
  assert_state_parity("ctor_int 42", mutable_bigint(42u));
}

// --- 除法 / sqrt / GCD / 模逆（阶段 6–8）---

TEST_F(MutableBigIntTest, ParityDivideBasic) {
  assert_divide_parity("10", "3");
  assert_divide_parity("1000", "37");
  assert_divide_parity("5", "10");
  assert_divide_parity("7", "7");
  assert_divide_parity("empty", "5");
}

TEST_F(MutableBigIntTest, ParityDivideMultiLimb) {
  assert_divide_parity("305419896,2596069104", "65537");
  assert_divide_parity("2,1", "4,3");
  assert_divide_parity("4294967295,1", "65535");
}

TEST_F(MutableBigIntTest, ParityDivideBurnikelZiegler) {
  const std::string a = bench_mag_csv(120);
  const std::string b = bench_mag_csv(85);
  mutable_bigint dividend = bigint_from_mag(a);
  mutable_bigint divisor = bigint_from_mag(b);
  mutable_bigint q;
  mutable_bigint r = dividend.divide(divisor, q);
  q.normalize();
  r.normalize();
  const std::string cmd_base = a + " " + b;
  assert_parity("limbs_divide_quotient " + cmd_base, format_limbs_normalized(q));
  assert_parity("limbs_divide_remainder " + cmd_base, format_limbs_normalized(r));
}

TEST_F(MutableBigIntTest, ParityDivideOneWord) {
  mutable_bigint a = bigint_from_mag("4294967295,1");
  mutable_bigint q;
  const uint32_t rem = a.divide_one_word(65537u, q);
  assert_parity("u32_divide_one_word 4294967295,1 65537", format_u32(rem));
  assert_state_parity("state_divide_one_word_quotient 4294967295,1 65537", q);
}

TEST_F(MutableBigIntTest, ParityDivideU64) {
  mutable_bigint a = bigint_from_mag("1,0");
  mutable_bigint q;
  const uint64_t rem = a.divide(0x100000000ULL, q);
  assert_parity("u64_divide 1,0 4294967296", format_u64(rem));
  assert_state_parity("state_divide_quotient_u64 1,0 4294967296", q);
}

TEST_F(MutableBigIntTest, ParitySqrt) {
  auto sqrt_norm = [](mutable_bigint x) {
    mutable_bigint r = x.sqrt();
    r.normalize();
    return r;
  };
  assert_state_parity("state_sqrt 9", sqrt_norm(mutable_bigint(9u)));
  assert_state_parity("state_sqrt 10", sqrt_norm(mutable_bigint(10u)));
  assert_state_parity("state_sqrt 65536", sqrt_norm(mutable_bigint(65536u)));
  assert_state_parity("state_sqrt empty", sqrt_norm(mutable_bigint()));
}

TEST_F(MutableBigIntTest, ParityBinaryGcdStatic) {
  assert_parity("u32_binary_gcd 48 18",
                format_u32(mutable_bigint::binary_gcd(48u, 18u)));
  assert_parity("u32_binary_gcd 0 99", format_u32(mutable_bigint::binary_gcd(0u, 99u)));
}

TEST_F(MutableBigIntTest, ParityBinaryGcdMag) {
  mutable_bigint a = bigint_from_mag("48");
  mutable_bigint b = bigint_from_mag("18");
  assert_state_parity("state_binary_gcd_mag 48 18", a.binary_gcd(b));
}

TEST_F(MutableBigIntTest, ParityHybridGcd) {
  mutable_bigint a = bigint_from_mag("305419896,2596069104");
  mutable_bigint b = bigint_from_mag("65537");
  assert_state_parity("state_hybrid_gcd 305419896,2596069104 65537", a.hybrid_gcd(b));
}

TEST_F(MutableBigIntTest, ParityInverseMod32And64) {
  assert_parity("u32_inverse_mod32 7",
                format_u32(mutable_bigint::inverse_mod32(7u)));
  assert_parity("u64_inverse_mod64 7", format_u64(mutable_bigint::inverse_mod64(7u)));
}

TEST_F(MutableBigIntTest, ParityModInverseMp2) {
  assert_state_parity("state_mod_inverse_mp2 3 8", mutable_bigint(3u).mod_inverse_mp2(8));
  assert_state_parity("state_mod_inverse_mp2 5 16", mutable_bigint(5u).mod_inverse_mp2(16));
}

TEST_F(MutableBigIntTest, ParityModInverseOdd) {
  assert_state_parity("state_mod_inverse 3 7", mutable_bigint(3u).mod_inverse(mutable_bigint(7u)));
  assert_state_parity("state_mod_inverse 305419896,2596069104 65537",
                      bigint_from_mag("305419896,2596069104")
                          .mod_inverse(mutable_bigint(65537u)));
}

TEST_F(MutableBigIntTest, ParityMutableModInverse) {
  assert_state_parity("state_mutable_mod_inverse 5 21", mutable_bigint(5u).mutable_mod_inverse(mutable_bigint(21u)));
  assert_state_parity("state_mutable_mod_inverse 11 35", mutable_bigint(11u).mutable_mod_inverse(mutable_bigint(35u)));
}

TEST_F(MutableBigIntTest, ParityModInverseBp2) {
  assert_state_parity("state_mod_inverse_bp2 7 5",
                      mutable_bigint::mod_inverse_bp2(mutable_bigint(7u), 5));
}
