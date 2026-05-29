#!/usr/bin/env bash
# Build and run JMH benchmarks for JDK MutableBigInteger.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JMH_DIR="${ROOT}/out/java/jmh"
PATCH="${ROOT}/out/java/jdk-patch"
OUT="${ROOT}/out/benchmark"
JAR="${JMH_DIR}/target/benchmarks.jar"

if [[ -z "${JAVA_HOME:-}" ]]; then
  if command -v /usr/libexec/java_home >/dev/null 2>&1; then
    JAVA_HOME="$(/usr/libexec/java_home 2>/dev/null || true)"
  fi
fi
JAVA="${JAVA_HOME:+$JAVA_HOME/bin/}java"
JAVA="${JAVA:-java}"

mkdir -p "${OUT}"

JAVAC="${JAVA_HOME:+$JAVA_HOME/bin/}javac"
JAVAC="${JAVAC:-javac}"
ACCESS_SRC="${PATCH}/java/math/MutableBigIntBenchAccess.java"
ACCESS_CLS="${PATCH}/java/math/MutableBigIntBenchAccess.class"
if [ ! -f "${ACCESS_SRC}" ]; then
  echo "Missing ${ACCESS_SRC}" >&2
  exit 1
fi
if [ ! -f "${ACCESS_CLS}" ] || [ "${ACCESS_SRC}" -nt "${ACCESS_CLS}" ]; then
  echo "Compiling jdk-patch bridge (MutableBigIntBenchAccess)..." >&2
  "${JAVAC}" --patch-module "java.base=${PATCH}" -d "${PATCH}" "${ACCESS_SRC}"
fi

MVN=""
if command -v mvn >/dev/null 2>&1; then
  MVN=mvn
elif [ -x /tmp/apache-maven-3.9.6/bin/mvn ]; then
  MVN=/tmp/apache-maven-3.9.6/bin/mvn
fi

if [ -n "${MVN}" ]; then
  echo "Building JMH uber-jar (Maven)..." >&2
  ( cd "${JMH_DIR}" && "${MVN}" -q -DskipTests package )
else
  echo "mvn not found; run: bash out/java/jmh/bootstrap_jmh.sh" >&2
  bash "${JMH_DIR}/bootstrap_jmh.sh"
fi

if [ ! -f "${JAR}" ]; then
  echo "Missing ${JAR}" >&2
  exit 1
fi

JMH_OPTS=(
  -rf json
  -rff "${OUT}/jmh.json"
  -wi 2
  -i 3
  -w 500ms
  -r 500ms
  -f 1
  -tu ns
  -bm avgt
  "bench.mutable.MutableBigIntJmh"
)

if [[ -n "${JMH_BENCHMARK:-}" ]]; then
  JMH_OPTS+=("${JMH_BENCHMARK}")
fi

echo "Running JMH (JDK MutableBigInteger via patch-module bridge)..." >&2
"${JAVA}" \
  -Djmh.ignoreLock=true \
  --patch-module "java.base=${PATCH}" \
  -jar "${JAR}" \
  "${JMH_OPTS[@]}" \
  2>&1 | tee "${OUT}/jmh_console.txt"

if command -v python3 >/dev/null 2>&1; then
  python3 "${ROOT}/out/jmh_json_to_csv.py" "${OUT}/jmh.json" > "${OUT}/java.csv"
  echo "Wrote ${OUT}/java.csv" >&2
fi

echo "Done." >&2
