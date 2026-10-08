#!/usr/bin/env python3
"""Medians per engine and screen from bench.sh results, as a markdown table.

  ./summarize.py ../results/<label>/runs.jsonl [--json]
"""
import json
import statistics
import sys
from collections import defaultdict

METRICS = [
    ("hitchMsPerS", "hitch ms/s", "%.1f"),
    ("dropped", "dropped", "%.0f"),
    ("p95", "p95 ms", "%.1f"),
    ("p99", "p99 ms", "%.1f"),
    ("max", "max ms", "%.0f"),
    ("jankyFrames", "janky", "%.0f"),
    ("frameP95", "frame p95 ms", "%.1f"),
    ("mainCpuMsPerS", "UI CPU ms/s", "%.0f"),
    ("renderCpuMsPerS", "render CPU ms/s", "%.0f"),
    ("processCpuMsPerS", "proc CPU ms/s", "%.0f"),
    ("peakMB", "peak MB", "%.0f"),
    ("blankAvg", "blank %", "%.2f"),
]
ENGINE_ORDER = ["sl", "sl-auto", "recycler", "recycler-auto", "rn"]


def main():
    path = sys.argv[1]
    rows = [json.loads(line) for line in open(path) if line.strip()]
    groups = defaultdict(list)
    for row in rows:
        groups[(row["screen"], row["count"], row["engine"])].append(row)
    summary = {}
    print("| screen | n | engine | runs | " + " | ".join(m[1] for m in METRICS) + " |")
    print("|" + "---|" * (4 + len(METRICS)))
    for (screen, count, engine) in sorted(groups, key=lambda k: (k[0], k[1], ENGINE_ORDER.index(k[2]) if k[2] in ENGINE_ORDER else 99)):
        runs = groups[(screen, count, engine)]
        medians = {}
        cells = []
        for key, _, fmt in METRICS:
            value = statistics.median(r[key] for r in runs)
            if key == "blankAvg":
                value *= 100
            medians[key] = value
            cells.append(fmt % value)
        summary["%s/%s/%s" % (screen, count, engine)] = medians
        print("| %s | %s | %s | %d | %s |" % (screen, count, engine, len(runs), " | ".join(cells)))
    if "--json" in sys.argv:
        print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
