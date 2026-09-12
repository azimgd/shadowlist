#!/usr/bin/env python3
"""
Print a before/after table from two benchmark result files.

    ./compare.py results/baseline.device.txt results/optimized.device.txt

Reads the machine-readable TSV block each run emits and pairs rows by
(scenario, rows). Prints Markdown so the output can go straight into a report.
"""
import sys


def read(path):
    rows = {}
    inside = False
    for line in open(path):
        line = line.rstrip("\n")
        if line == "#BEGIN_TSV":
            inside = True
            continue
        if line == "#END_TSV":
            break
        if not inside or line.startswith("scenario\t"):
            continue
        cells = line.split("\t")
        if len(cells) < 6:
            continue
        scenario, count, median, _lo, _hi, unit = cells[:6]
        rows[(scenario, int(count))] = (float(median), unit)
    return rows


def main():
    before = read(sys.argv[1])
    after = read(sys.argv[2])
    sizes = sorted({count for _, count in before})

    for size in sizes:
        print(f"\n#### {size:,} rows\n")
        print("| Scenario | Unit | Before | After | Change |")
        print("| --- | --- | ---: | ---: | ---: |")
        for (scenario, count), (baseline, unit) in before.items():
            if count != size or (scenario, count) not in after:
                continue
            optimized = after[(scenario, count)][0]
            if optimized > 0 and baseline > 0:
                ratio = baseline / optimized
                change = f"{ratio:,.1f}x faster" if ratio >= 1.05 else (
                    f"{1 / ratio:,.1f}x slower" if ratio <= 0.95 else "unchanged")
            else:
                change = "-"
            print(f"| {scenario} | {unit} | {baseline:,.1f} | {optimized:,.1f} | {change} |")


if __name__ == "__main__":
    main()
