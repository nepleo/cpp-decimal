#!/usr/bin/env bash
# Fair comparison: C++ Google Benchmark + Java JMH (aligned semantics).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-${ROOT}/build}"
OUT="${ROOT}/out/benchmark"
CPP_BENCH="${BUILD}/benchmark/mutable_bigint_bench"

mkdir -p "${OUT}"

if [ ! -x "${CPP_BENCH}" ]; then
  echo "C++ benchmark binary not found: ${CPP_BENCH}" >&2
  echo "  cmake -S . -B build -DBUILD_BENCHMARKS=ON && cmake --build build" >&2
  exit 1
fi

echo "== C++ (Google Benchmark) ==" >&2
"${CPP_BENCH}" \
  --benchmark_min_time=0.1s \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_format=console \
  | tee "${OUT}/cpp_console.txt"

"${CPP_BENCH}" \
  --benchmark_min_time=0.1s \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_out="${OUT}/cpp.json" \
  --benchmark_out_format=json

echo "Wrote ${OUT}/cpp.json" >&2

echo "== Java (JMH + JDK MutableBigInteger) ==" >&2
bash "${ROOT}/out/run_java_jmh.sh"

if command -v python3 >/dev/null 2>&1; then
  python3 "${ROOT}/out/merge_benchmark_results.py" \
    "${OUT}/cpp.json" \
    "${OUT}/java.csv" \
    "${OUT}"
else
  echo "python3 not found; skip comparison.csv" >&2
fi

echo "Done. See ${OUT}/comparison_pivot.csv" >&2
