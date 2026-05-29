#!/usr/bin/env bash
# Build JMH uber-jar: compile jdk-patch bridge + bench.mutable (Maven or manual).
set -euo pipefail

JMH_DIR="$(cd "$(dirname "$0")" && pwd)"
PATCH="${JMH_DIR}/../jdk-patch"
MVN_BIN=""
if command -v mvn >/dev/null 2>&1; then
  MVN_BIN=mvn
elif [ -x /tmp/apache-maven-3.9.6/bin/mvn ]; then
  MVN_BIN=/tmp/apache-maven-3.9.6/bin/mvn
fi

if [ -z "${MVN_BIN}" ]; then
  echo "mvn required (brew install maven or use /tmp/apache-maven-3.9.6)" >&2
  exit 1
fi

echo "Building with Maven (patch-module java.base for bench access)..." >&2
( cd "${JMH_DIR}" && "${MVN_BIN}" -q -DskipTests package )
echo "Built ${JMH_DIR}/target/benchmarks.jar" >&2
