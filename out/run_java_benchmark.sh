#!/usr/bin/env bash
# Build and run JDK MutableBigInteger microbenchmark (CSV to stdout).
# Usage: out/run_java_benchmark.sh [iterations]
#   iterations default: 2000000 (override with BENCH_ITERS env)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCH="${ROOT}/out/java/jdk-patch"
BENCH_SRC="${PATCH}/java/math/MutableBigIntBenchmark.java"
BENCH_CLASS="${PATCH}/java/math/MutableBigIntBenchmark.class"
MAIN_CLASS="java.math.MutableBigIntBenchmark"

if [[ -z "${JAVA_HOME:-}" ]]; then
  if command -v /usr/libexec/java_home >/dev/null 2>&1; then
    JAVA_HOME="$(/usr/libexec/java_home 2>/dev/null || true)"
  fi
fi
JAVA="${JAVA_HOME:+$JAVA_HOME/bin/}java"
JAVAC="${JAVA_HOME:+$JAVA_HOME/bin/}javac"
JAVA="${JAVA:-java}"
JAVAC="${JAVAC:-javac}"

ITERS="${1:-${BENCH_ITERS:-2000000}}"

mkdir -p "${PATCH}/java/math"

need_build=0
if [ ! -f "${BENCH_CLASS}" ]; then
  need_build=1
fi
for f in "${PATCH}/java/math/"*.java; do
  if [ -f "$f" ] && [ "$f" -nt "${BENCH_CLASS}" ]; then
    need_build=1
    break
  fi
done

if [ "$need_build" -eq 1 ]; then
  echo "Compiling JDK patch-module benchmarks..." >&2
  "${JAVAC}" -encoding UTF-8 \
    --patch-module "java.base=${PATCH}" \
    -d "${PATCH}" \
    "${PATCH}/java/math/"*.java
fi

exec "${JAVA}" \
  --patch-module "java.base=${PATCH}" \
  "${MAIN_CLASS}" "${ITERS}"
