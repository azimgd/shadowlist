#!/usr/bin/env python3
"""
Reduce scroll-bench.sh runs: one json per run, then a summary table of medians per screen.

  scroll-bench.py ios <base> --t0 --t1 --cpu0 --cpu1 --lines "l0 l1"
  scroll-bench.py android <base> --t0 --t1 --cpu0 --cpu1 --clk --width --height --viewport "top bottom"
  scroll-bench.py summary <dir>
"""
import argparse
import glob
import importlib.util
import json
import os
import re
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("am", os.path.join(HERE, "analyze-metrics.py"))
am = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(am)

FPS = re.compile(r"\[SLFPS\] t=\S+ span=(\d+) frames=(\d+) slow=(\d+) longest=([\d.]+)")
UI = re.compile(r"\[SLUI\] t=\S+ span=(\d+) frames=(\d+) slow=(\d+) longest=([\d.]+)")


def reduce_windows(rows):
    """rows: (span_ms, frames, slow, longest). Frames per second, slow share, worst gap."""
    if not rows:
        return None
    span = sum(r[0] for r in rows)
    frames = sum(r[1] for r in rows)
    slow = sum(r[2] for r in rows)
    return {
        "fps": round(frames * 1000 / span, 1) if span else 0,
        "slow_pct": round(100 * slow / frames, 1) if frames else 0,
        "longest_ms": round(max(r[3] for r in rows), 1),
        "windows": len(rows),
    }


def parse(regex, lines):
    rows = []
    for line in lines:
        m = regex.search(line)
        if m:
            rows.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), float(m.group(4))))
    return rows


def cpu_seconds(value):
    # ps time: [[dd-]hh:]mm:ss.cc
    parts = value.replace("-", ":").split(":")
    total = 0.0
    for part in parts:
        total = total * 60 + float(part)
    return total


def percentile(values, q):
    if not values:
        return None
    index = min(len(values) - 1, int(round(q * (len(values) - 1))))
    return round(values[index], 1)


def ios(args):
    with open(args.base + ".log", errors="replace") as handle:
        lines = handle.read().splitlines()
    l0, l1 = (int(v) for v in args.lines.split())
    window = lines[l0:l1]
    elapsed = float(args.t1) - float(args.t0)
    result = {
        "js": reduce_windows(parse(FPS, window)),
        "ui": reduce_windows(parse(UI, window)),
        "cpu_pct": round(100 * (cpu_seconds(args.cpu1) - cpu_seconds(args.cpu0)) / elapsed, 1),
    }
    write(args.base, result)


def android(args):
    t0, t1 = float(args.t0), float(args.t1)
    rows = []
    with open(args.base + ".log", errors="replace") as handle:
        for line in handle:
            parts = line.split()
            try:
                stamp = float(parts[0])
            except (ValueError, IndexError):
                continue
            if t0 <= stamp <= t1:
                rows.append(line)
    total, janky, durations = am.parse_frames(args.base + ".gfx")
    mem = am.parse_mem(args.base + ".mem")
    top, bottom = (float(v) for v in args.viewport.split())
    y0, y1 = int(args.height * top), int(args.height * bottom)
    blanks = []
    shots = os.path.join(args.base + ".shots", "slbench-shots")
    for path in sorted(glob.glob(os.path.join(shots, "f*.raw"))):
        frame = am.read_raw(path)
        if frame:
            width, _h, pixels = frame
            blanks.append(am.blank_band(width, pixels, y0, y1, bytes((0, 0, 0))))
    elapsed = t1 - t0
    result = {"threads_ms": thread_ms(args.base, int(args.clk))} if os.path.exists(args.base + ".threads0") else {}
    result.update({
        "js": reduce_windows(parse(FPS, rows)),
        "ui": {
            "frames": total,
            "janky_pct": round(100 * janky / total, 1) if total else None,
            "p50": percentile(durations, 0.5),
            "p90": percentile(durations, 0.9),
            "p99": percentile(durations, 0.99),
        },
        "cpu_pct": round(100 * (int(args.cpu1) - int(args.cpu0)) / int(args.clk) / elapsed, 1),
        "pss_mb": round(mem["pss_total_kb"] / 1024, 1) if mem.get("pss_total_kb") else None,
        "views": mem.get("views"),
        "blank_max_px": max(blanks) if blanks else None,
        "blank_frames": sum(1 for b in blanks if b >= am.BLANK_BAND_PX),
        "shots": len(blanks),
    })
    write(args.base, result)


def thread_ms(base, clk):
    """CPU ms per thread group over the fling window: ui, render, js, other."""
    def load(path):
        out = {}
        with open(path) as handle:
            for line in handle:
                name, _, ticks = line.strip().rpartition("|")
                parts = ticks.split()
                if name and len(parts) == 2:
                    out.setdefault(name, []).append(int(parts[0]) + int(parts[1]))
        return {k: sum(v) for k, v in out.items()}
    before, after = load(base + ".threads0"), load(base + ".threads1")
    groups = {"ui": 0, "render": 0, "js": 0, "other": 0}
    for name, ticks in after.items():
        delta = (ticks - before.get(name, 0)) * 1000 / clk
        key = ("ui" if name.endswith("example") else "render" if name == "RenderThread"
               else "js" if name == "mqt_v_js" else "other")
        groups[key] += delta
    groups["total"] = sum(groups.values())
    return {k: round(v) for k, v in groups.items()}


def write(base, result):
    with open(base + ".json", "w") as handle:
        json.dump(result, handle, indent=1)
    print("   " + json.dumps(result))


def flat(result, prefix=""):
    out = {}
    for key, value in (result or {}).items():
        if isinstance(value, dict):
            out.update(flat(value, prefix + key + "."))
        elif isinstance(value, (int, float)) and value is not None:
            out[prefix + key] = value
    return out


def summary(args):
    by_screen = {}
    for path in sorted(glob.glob(os.path.join(args.dir, "*-*.json"))):
        screen = os.path.basename(path).rsplit("-", 1)[0]
        with open(path) as handle:
            by_screen.setdefault(screen, []).append(flat(json.load(handle)))
    keys = []
    for runs in by_screen.values():
        for run in runs:
            for key in run:
                if key not in keys and not key.endswith("windows"):
                    keys.append(key)
    medians = {
        screen: {k: round(statistics.median([r[k] for r in runs if k in r]), 1)
                 for k in keys if any(k in r for r in runs)}
        for screen, runs in by_screen.items()
    }
    screens = list(by_screen)
    lines = ["| metric | " + " | ".join(f"{s} (n={len(by_screen[s])})" for s in screens) + " |",
             "|---|" + "---|" * len(screens)]
    for key in keys:
        lines.append(f"| {key} | " + " | ".join(str(medians[s].get(key, "")) for s in screens) + " |")
    text = "\n".join(lines)
    print(text)
    with open(os.path.join(args.dir, "summary.md"), "w") as handle:
        handle.write(text + "\n")
    with open(os.path.join(args.dir, "summary.json"), "w") as handle:
        json.dump(medians, handle, indent=1)


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    for mode in ("ios", "android"):
        p = sub.add_parser(mode)
        p.add_argument("base")
        for flag in ("--t0", "--t1", "--cpu0", "--cpu1"):
            p.add_argument(flag, required=True)
        if mode == "ios":
            p.add_argument("--lines", required=True)
        else:
            p.add_argument("--clk", default="100")
            p.add_argument("--width", type=int, required=True)
            p.add_argument("--height", type=int, required=True)
            p.add_argument("--viewport", required=True)
    s = sub.add_parser("summary")
    s.add_argument("dir")
    args = parser.parse_args()
    {"ios": ios, "android": android, "summary": summary}[args.mode](args)


if __name__ == "__main__":
    sys.exit(main())
