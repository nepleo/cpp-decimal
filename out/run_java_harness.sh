#!/usr/bin/env bash
# Build and run MutableBigIntHarness against JDK java.math.MutableBigInteger
# (from $JAVA_HOME, via --patch-module java.base). Does NOT use javamath/.
#
# Usage: out/run_java_harness.sh <command> [args...]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCH="${ROOT}/out/java/jdk-patch"
HARNESS_SRC="${PATCH}/java/math/MutableBigIntHarness.java"
CLASS_FILE="${PATCH}/java/math/MutableBigIntHarness.class"
MAIN_CLASS="java.math.MutableBigIntHarness"

if [[ -z "${JAVA_HOME:-}" ]]; then
  if command -v /usr/libexec/java_home >/dev/null 2>&1; then
    JAVA_HOME="$(/usr/libexec/java_home 2>/dev/null || true)"
  fi
fi
JAVA="${JAVA_HOME:+$JAVA_HOME/bin/}java"
JAVAC="${JAVA_HOME:+$JAVA_HOME/bin/}javac"
JAVA="${JAVA:-java}"
JAVAC="${JAVAC:-javac}"

if [[ ! -f "${HARNESS_SRC}" ]]; then
  echo "missing harness: ${HARNESS_SRC}" >&2
  exit 1
fi

if [[ -n "${JAVA_HOME:-}" && -f "${JAVA_HOME}/lib/src.zip" ]]; then
  JDK_SRC_ZIP="${JAVA_HOME}/lib/src.zip"
elif [[ -n "${JAVA_HOME:-}" && -f "${JAVA_HOME}/src.zip" ]]; then
  JDK_SRC_ZIP="${JAVA_HOME}/src.zip"
else
  JDK_SRC_ZIP=""
fi

mkdir -p "${PATCH}/java/math"

need_build=0
if [ ! -f "${CLASS_FILE}" ]; then
  need_build=1
elif [ "${HARNESS_SRC}" -nt "${CLASS_FILE}" ]; then
  need_build=1
else
  for f in "${PATCH}"/java/math/*.java; do
    if [ -f "$f" ] && [ "$f" -nt "${CLASS_FILE}" ]; then
      need_build=1
      break
    fi
  done
fi

if [ "$need_build" -eq 1 ]; then
  echo "Compiling harness with JDK java.math.MutableBigInteger (patch-module java.base)..." >&2
  if [ -n "${JDK_SRC_ZIP}" ]; then
    echo "  JAVA_HOME=${JAVA_HOME}" >&2
    echo "  JDK sources: ${JDK_SRC_ZIP}" >&2
  fi
  _harness_sources=()
  while IFS= read -r _src; do
    _harness_sources+=("$_src")
  done < <(find "${PATCH}/java/math" -name '*.java' 2>/dev/null | sort)
  "${JAVAC}" -encoding UTF-8 \
    --patch-module "java.base=${PATCH}" \
    -d "${PATCH}" \
    "${_harness_sources[@]}"
fi

exec "${JAVA}" \
  --patch-module "java.base=${PATCH}" \
  "${MAIN_CLASS}" "$@"
