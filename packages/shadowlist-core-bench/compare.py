#!/usr/bin/env python3
"""
Print a before and after table for benchmark result files.

    ./compare.py results/baseline.device.txt results/optimized.device.txt
    ./compare.py before-1.txt before-2.txt before-3.txt -- after-1.txt after-2.txt after-3.txt

Matches rows by scenario and row count and prints a Markdown table. Each file is one run of
the benchmark. A side with several runs shows the median of their medians. A change is called
faster or slower only with at least 3 runs on each side, a change of 5% or more, and run
medians that don't overlap between the sides.
"""
import statistics
import sys

MIN_RUNS = 3


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


def read_runs(paths):
    """Per scenario and row count: the median of each run, and the unit."""
    runs = {}
    for path in paths:
        for key, (median, unit) in read_results(path).items():
            runs.setdefault(key, ([], unit))[0].append(median)
    return runs


def change(baseline, optimized):
    if min(baseline) <= 0 or min(optimized) <= 0:
        return "-"
    ratio = statistics.median(baseline) / statistics.median(optimized)
    if 0.95 < ratio < 1.05:
        return "unchanged"
    text = f"{ratio:,.1f}x lower" if ratio >= 1 else f"{1 / ratio:,.1f}x higher"
    runs = min(len(baseline), len(optimized))
    if runs < MIN_RUNS:
        return f"{text} ({runs} run{'s' if runs > 1 else ''}, needs {MIN_RUNS})"
    if min(baseline) <= max(optimized) and min(optimized) <= max(baseline):
        return f"{text} (within run spread)"
    return f"{ratio:,.1f}x faster" if ratio >= 1 else f"{1 / ratio:,.1f}x slower"


def main():
    args = sys.argv[1:]
    if "--" in args:
        split = args.index("--")
        before_paths, after_paths = args[:split], args[split + 1:]
    elif len(args) == 2:
        before_paths, after_paths = args[:1], args[1:]
    else:
        before_paths, after_paths = [], []
    if not before_paths or not after_paths:
        sys.exit("usage: compare.py <before.txt> <after.txt>\n"
                 "       compare.py <before.txt>... -- <after.txt>...")

    before = read_runs(before_paths)
    after = read_runs(after_paths)
    sizes = sorted({count for _, count in before})

    for size in sizes:
        print(f"\n#### {size:,} rows\n")
        print("| Scenario | Unit | Before | After | Change |")
        print("| --- | --- | ---: | ---: | ---: |")
        for (scenario, count), (baseline, unit) in before.items():
            if count != size or (scenario, count) not in after:
                continue
            optimized = after[(scenario, count)][0]
            print(f"| {scenario} | {unit} | {statistics.median(baseline):,.1f} | "
                  f"{statistics.median(optimized):,.1f} | {change(baseline, optimized)} |")


if __name__ == "__main__":
    main()
