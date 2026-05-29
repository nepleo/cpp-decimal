#include <cstdint>

#include <benchmark/benchmark.h>

#include "bigdecimal.h"

namespace {

jarray<uint32_t> make_mag(int limbs) {
  jarray<uint32_t> mag(limbs);
  for (int i = 0; i < limbs; ++i) {
    mag[i] = static_cast<uint32_t>(0x9e3779b9u + static_cast<uint32_t>(i) * 0x85ebca6bu);
  }
  return mag;
}

mutable_bigint make_value(int limbs) {
  if (limbs <= 0) {
    return mutable_bigint();
  }
  if (limbs == 1) {
    return mutable_bigint(0x12345678u);
  }
  return mutable_bigint(make_mag(limbs));
}

}  // namespace

static void BM_DefaultCtor(benchmark::State& state) {
  for (auto _ : state) {
    mutable_bigint n;
    benchmark::DoNotOptimize(n);
  }
}
BENCHMARK(BM_DefaultCtor);

static void BM_UintCtor(benchmark::State& state) {
  for (auto _ : state) {
    mutable_bigint n(0xdeadbeefu);
    benchmark::DoNotOptimize(n);
  }
}
BENCHMARK(BM_UintCtor);

static void BM_JarrayCtor(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  const jarray<uint32_t> mag = make_mag(limbs);
  for (auto _ : state) {
    mutable_bigint n(mag);
    benchmark::DoNotOptimize(n);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * limbs * sizeof(uint32_t));
}
BENCHMARK(BM_JarrayCtor)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_CopyCtor(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  const mutable_bigint src = make_value(limbs);
  for (auto _ : state) {
    mutable_bigint copy(src);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_CopyCtor)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_CopyAssign(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  const mutable_bigint src = make_value(limbs);
  mutable_bigint dst;
  for (auto _ : state) {
    dst = src;
    benchmark::DoNotOptimize(dst);
  }
}
BENCHMARK(BM_CopyAssign)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_MoveCtor(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  for (auto _ : state) {
    mutable_bigint src = make_value(limbs);
    mutable_bigint dst(std::move(src));
    benchmark::DoNotOptimize(dst);
    benchmark::DoNotOptimize(src);
  }
}
BENCHMARK(BM_MoveCtor)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_MoveAssign(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  for (auto _ : state) {
    mutable_bigint src = make_value(limbs);
    mutable_bigint dst;
    dst = std::move(src);
    benchmark::DoNotOptimize(dst);
    benchmark::DoNotOptimize(src);
  }
}
BENCHMARK(BM_MoveAssign)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

// Restore non-zero state outside the timed section (aligns with JMH @Setup(Level.Iteration)).
static void BM_Clear(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  mutable_bigint n;
  for (auto _ : state) {
    state.PauseTiming();
    n = make_value(limbs);
    state.ResumeTiming();
    n.clear();
    benchmark::DoNotOptimize(n);
  }
}
BENCHMARK(BM_Clear)->Arg(1)->Arg(16)->Arg(64);

static void BM_Reset(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  mutable_bigint n;
  for (auto _ : state) {
    state.PauseTiming();
    n = make_value(limbs);
    state.ResumeTiming();
    n.reset();
    benchmark::DoNotOptimize(n);
  }
}
BENCHMARK(BM_Reset)->Arg(1)->Arg(16)->Arg(64);

static void BM_Mul(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  const mutable_bigint x = make_value(limbs);
  constexpr uint32_t k_mul = 0x9e3779b9u;
  for (auto _ : state) {
    mutable_bigint z;
    x.mul(k_mul, z);
    benchmark::DoNotOptimize(z);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * limbs * sizeof(uint32_t));
}
BENCHMARK(BM_Mul)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_Multiply(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  const mutable_bigint x = make_value(limbs);
  const mutable_bigint y = make_value(limbs);
  for (auto _ : state) {
    mutable_bigint z;
    x.multiply(y, z);
    benchmark::DoNotOptimize(z);
  }
  const int64_t bytes = static_cast<int64_t>(limbs) * sizeof(uint32_t) * 2;
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * bytes);
}
BENCHMARK(BM_Multiply)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_Divide(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  mutable_bigint x = make_value(limbs);
  const mutable_bigint y = make_value(std::max(1, limbs / 2));
  for (auto _ : state) {
    mutable_bigint q;
    mutable_bigint r = x.divide(y, q);
    x = make_value(limbs);
    benchmark::DoNotOptimize(q);
    benchmark::DoNotOptimize(r);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * limbs * sizeof(uint32_t));
}
BENCHMARK(BM_Divide)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_Sqrt(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  mutable_bigint x = make_value(limbs);
  for (auto _ : state) {
    mutable_bigint r = x.sqrt();
    x = make_value(limbs);
    benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Sqrt)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_HybridGcd(benchmark::State& state) {
  const int limbs = static_cast<int>(state.range(0));
  mutable_bigint x = make_value(limbs);
  const mutable_bigint y = make_value(limbs);
  for (auto _ : state) {
    mutable_bigint g = x.hybrid_gcd(y);
    x = make_value(limbs);
    benchmark::DoNotOptimize(g);
  }
}
BENCHMARK(BM_HybridGcd)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

static void BM_ModInverse(benchmark::State& state) {
  mutable_bigint x(3u);
  const mutable_bigint mod(65537u);
  for (auto _ : state) {
    mutable_bigint inv = x.mod_inverse(mod);
    x = mutable_bigint(3u);
    benchmark::DoNotOptimize(inv);
  }
}
BENCHMARK(BM_ModInverse);

BENCHMARK_MAIN();
