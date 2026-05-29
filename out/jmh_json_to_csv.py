#!/usr/bin/env python3
"""Convert JMH JSON results to java.csv (name,limbs,iterations,ns_per_op)."""

import json
import sys
from pathlib import Path


def benchmark_short_name(full: str) -> str:
    return full.rsplit(".", 1)[-1]


def main() -> None:
    if len(sys.argv) != 2:
        print("usage: jmh_json_to_csv.py jmh.json", file=sys.stderr)
        sys.exit(2)

    data = json.loads(Path(sys.argv[1]).read_text())
    if not isinstance(data, list):
        data = [data]

    print("name,limbs,iterations,ns_per_op")
    for row in data:
        if row.get("mode") != "avgt":
            continue
        name = benchmark_short_name(row["benchmark"])
        params = row.get("params") or {}
        limbs = int(params.get("limbs", 0))
        metric = row.get("primaryMetric", {})
        score = metric.get("score")
        if score is None:
            continue
        iters = row.get("measurementIterations", "")
        print(f"{name},{limbs},{iters},{score:.2f}")


if __name__ == "__main__":
    main()
