#!/usr/bin/env python3
"""Merge Google Benchmark JSON + JMH java.csv into comparison.csv and pivot table."""

import csv
import json
import re
import sys
from pathlib import Path


def parse_cpp_json(path: Path) -> list[dict]:
    data = json.loads(path.read_text())
    rows = []
    for b in data.get("benchmarks", []):
        name = b.get("name", "")
        if not name.endswith("_mean"):
            continue
        base = name[: -len("_mean")]
        limbs = 0
        m = re.match(r"BM_\w+/(\d+)", base)
        if m:
            limbs = int(m.group(1))
        else:
            base = base.split("/")[0]
        time_ns = b.get("cpu_time", b.get("real_time", 0.0))
        unit = b.get("time_unit", "ns")
        if unit == "us":
            time_ns *= 1000
        elif unit == "ms":
            time_ns *= 1_000_000
        elif unit == "s":
            time_ns *= 1_000_000_000
        rows.append(
            {
                "name": base.split("/")[0],
                "limbs": limbs,
                "language": "cpp",
                "iterations": b.get("iterations", ""),
                "time_ns": round(time_ns, 2),
            }
        )
    return rows


def parse_java_csv(path: Path) -> list[dict]:
    rows = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(
                {
                    "name": r["name"],
                    "limbs": int(r["limbs"]),
                    "language": "java",
                    "iterations": r["iterations"],
                    "time_ns": round(float(r["ns_per_op"]), 2),
                }
            )
    return rows


def write_comparison(cpp: list[dict], java: list[dict], out_path: Path) -> None:
    all_rows = cpp + java
    all_rows.sort(key=lambda r: (r["name"], r["limbs"], r["language"]))
    with out_path.open("w", newline="") as f:
        writer = csv.DictWriter(
            f, fieldnames=["name", "limbs", "language", "iterations", "time_ns"]
        )
        writer.writeheader()
        writer.writerows(all_rows)


def write_pivot(cpp: list[dict], java: list[dict], pivot_path: Path) -> None:
    cpp_map = {(r["name"], r["limbs"]): r["time_ns"] for r in cpp}
    java_map = {(r["name"], r["limbs"]): r["time_ns"] for r in java}
    keys = sorted(set(cpp_map) | set(java_map))
    with pivot_path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["name", "limbs", "cpp_ns", "java_ns", "java_div_cpp"])
        for key in keys:
            c = cpp_map.get(key)
            j = java_map.get(key)
            ratio = ""
            if c is not None and j is not None and c > 0:
                ratio = f"{j / c:.3f}"
            w.writerow(
                [
                    key[0],
                    key[1],
                    f"{c:.2f}" if c is not None else "",
                    f"{j:.2f}" if j is not None else "",
                    ratio,
                ]
            )


def main() -> None:
    if len(sys.argv) != 4:
        print(
            "usage: merge_benchmark_results.py cpp.json java.csv out_dir",
            file=sys.stderr,
        )
        sys.exit(2)
    cpp_path = Path(sys.argv[1])
    java_path = Path(sys.argv[2])
    out_dir = Path(sys.argv[3])
    out_dir.mkdir(parents=True, exist_ok=True)

    cpp = parse_cpp_json(cpp_path)
    java = parse_java_csv(java_path)
    write_comparison(cpp, java, out_dir / "comparison.csv")
    write_pivot(cpp, java, out_dir / "comparison_pivot.csv")
    print(f"Wrote {out_dir / 'comparison.csv'}", file=sys.stderr)
    print(f"Wrote {out_dir / 'comparison_pivot.csv'}", file=sys.stderr)


if __name__ == "__main__":
    main()
