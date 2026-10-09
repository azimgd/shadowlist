#!/usr/bin/env python3
"""Medians per engine, screen and axis from bench.sh results, as a markdown table.

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
CONTENT_METRICS = [
    ("contentBlankAvg", "content blank %", "%.2f"),
    ("contentBlankFrames", "content blank frames", "%.0f"),
]
ENGINE_ORDER = ["sl", "sl-auto", "recycler", "recycler-auto", "rn"]


def main():
    path = sys.argv[1]
    rows = [json.loads(line) for line in open(path) if line.strip()]
    rows = [row for row in rows if "skipped" not in row]
    show_axis = any(row.get("axis", "y") != "y" for row in rows)
    metrics = METRICS + ([m for m in CONTENT_METRICS if any(m[0] in row for row in rows)])
    groups = defaultdict(list)
    for row in rows:
        groups[(row["screen"], row["count"], row["engine"], row.get("axis", "y"))].append(row)
    summary = {}
    axis_head = "axis | " if show_axis else ""
    print("| screen | n | engine | " + axis_head + "runs | " + " | ".join(m[1] for m in metrics) + " |")
    print("|" + "---|" * (4 + (1 if show_axis else 0) + len(metrics)))
    order = lambda k: (k[0], k[1], ENGINE_ORDER.index(k[2]) if k[2] in ENGINE_ORDER else 99, k[3])
    for (screen, count, engine, axis) in sorted(groups, key=order):
        runs = groups[(screen, count, engine, axis)]
        medians = {}
        cells = []
        for key, _, fmt in metrics:
            values = [r[key] for r in runs if key in r]
            if not values:
                cells.append("")
                continue
            value = statistics.median(values)
            if key in ("blankAvg", "contentBlankAvg"):
                value *= 100
            medians[key] = value
            cells.append(fmt % value)
        name = "%s/%s/%s" % (screen, count, engine) + ("/%s" % axis if show_axis else "")
        summary[name] = medians
        axis_cell = "%s | " % axis if show_axis else ""
        print("| %s | %s | %s | %s%d | %s |" % (screen, count, engine, axis_cell, len(runs), " | ".join(cells)))
    if "--json" in sys.argv:
        print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
