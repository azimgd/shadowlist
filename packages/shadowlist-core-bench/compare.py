#!/usr/bin/env python3
"""
Print a before/after table from two benchmark result files.

    ./compare.py results/baseline.device.txt results/optimized.device.txt

Reads the machine-readable TSV block each run emits and pairs rows by
(scenario, rows). Prints Markdown so the output can go straight into a report.
"""
import sys


def read_results(path):
    rows = {}
    inside = False
    with open(path) as handle:
        for line in handle:
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
            scenario, count, median, _low, _high, unit = cells[:6]
            rows[(scenario, int(count))] = (float(median), unit)
    return rows


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: compare.py <before.txt> <after.txt>")

    before = read_results(sys.argv[1])
    after = read_results(sys.argv[2])
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
                if ratio >= 1.05:
                    change = f"{ratio:,.1f}x faster"
                elif ratio <= 0.95:
                    change = f"{1 / ratio:,.1f}x slower"
                else:
                    change = "unchanged"
            else:
                change = "-"
            print(f"| {scenario} | {unit} | {baseline:,.1f} | {optimized:,.1f} | {change} |")


if __name__ == "__main__":
    main()
