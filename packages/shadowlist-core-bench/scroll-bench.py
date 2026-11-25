#!/usr/bin/env python3
"""
Reduce scroll-bench.sh runs: one json per run, then a summary table of medians per screen.

  scroll-bench.py ios <base> --t0 --t1 --cpu0 --cpu1 --lines "l0 l1"
  scroll-bench.py android <base> --t0 --t1 --cpu0 --cpu1 --clk --width --height --viewport "top bottom"
  scroll-bench.py summary <dir>
  scroll-bench.py compare-ios <base> --t0 --t1 --cpu0 --cpu1
  scroll-bench.py compare-android <base> --slow "e0 e1 ticks0 ticks1" --fling "..." --clk
  scroll-bench.py compare-ops <base>
  scroll-bench.py compare-summary <dir> [--warmup 1]
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
    flags = set()
    for path in glob.glob(os.path.join(args.dir, "*.flags")):
        with open(path) as handle:
            flags.add(handle.read().strip() or "unknown")
    lines += ["", "React Native engine flags: " + (", ".join(sorted(flags)) or "not recorded")]
    text = "\n".join(lines)
    print(text)
    with open(os.path.join(args.dir, "summary.md"), "w") as handle:
        handle.write(text + "\n")
    with open(os.path.join(args.dir, "summary.json"), "w") as handle:
        json.dump(medians, handle, indent=1)

# ---------------------------------------------------------------------------- list comparison

PHASE = re.compile(r"\[SLPHASE\] phase=(\w+) (.*)")
MOUNT = re.compile(r"\[SLMOUNT\] .* ms=([\d.]+)")
BLANK = re.compile(r"\[SLBLANK\] t=\d+ span=(\d+) frames=(\d+) slow=(\d+) blank_frames=(\d+) blank_pct=([\d.]+) blank_band=([\d.]+)")
OP = re.compile(r"\[SLOP\] (begin|done) name=([\w-]+)(.*)")
ANCHOR = re.compile(r"\[SLANCHOR\] t=\S+ ?(.*)")


def kv(text):
    out = {}
    for part in text.split():
        key, _, value = part.partition("=")
        try:
            out[key] = float(value)
        except ValueError:
            pass
    return out


def read_lines(path):
    with open(path, errors="replace") as handle:
        return handle.read().splitlines()


def mount_ms(lines):
    for line in lines:
        m = MOUNT.search(line)
        if m:
            return float(m.group(1))
    return None


def filled_ms(lines):
    """Time from BenchScreen's first render to the first frame rows covered the list. iOS logs
    it as ms=; Android logs both lines and the logcat -v epoch stamps give the difference."""
    start = None
    for line in lines:
        if "[SLMOUNTSTART]" in line:
            try:
                start = float(line.split()[0])
            except (ValueError, IndexError):
                start = None
        if "[SLFILLED]" in line:
            m = re.search(r"\[SLFILLED\] ms=([\d.]+)", line)
            if m:
                return float(m.group(1))
            try:
                return round((float(line.split()[0]) - start) * 1000, 1) if start is not None else None
            except (ValueError, IndexError):
                return None
    return None


def js_between(lines, start, end):
    """[SLFPS] windows between two line indices."""
    return reduce_windows(parse(FPS, lines[start:end]))


def compare_ios(args):
    lines = read_lines(args.base + ".log")
    result = {"mount_ms": mount_ms(lines), "filled_ms": filled_ms(lines)}
    starts = [i for i, l in enumerate(lines) if "[SLBENCH] start" in l]
    begin = starts[0] if starts else 0
    for i, line in enumerate(lines):
        m = PHASE.search(line)
        if m:
            values = kv(m.group(2))
            result[m.group(1)] = {
                "frames": values.get("frames"), "p50_ms": values.get("p50"), "p95_ms": values.get("p95"),
                "p99_ms": values.get("p99"), "max_ms": values.get("max"), "janky": values.get("janky"),
                "dropped": values.get("dropped"), "blank_frames": values.get("blank_frames"),
                "blank_pct": values.get("blank_pct"), "blank_band_pt": values.get("blank_band"),
                "offset": values.get("offset"),
                "js": js_between(lines, begin, i),
            }
            begin = i
    elapsed = float(args.t1) - float(args.t0)
    result["cpu_pct"] = round(100 * (cpu_seconds(args.cpu1) - cpu_seconds(args.cpu0)) / elapsed, 1) if elapsed > 0 else None
    rss = []
    for line in read_lines(args.base + ".rss"):
        parts = line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            rss.append(int(parts[1]) / 1024)
    if rss:
        result["rss_peak_mb"] = round(max(rss), 1)
        result["rss_end_mb"] = round(rss[-1], 1)
    write(args.base, result)


def window_rows(lines, regex, t0, t1):
    rows = []
    for line in lines:
        parts = line.split()
        try:
            stamp = float(parts[0])
        except (ValueError, IndexError):
            continue
        if t0 <= stamp <= t1:
            m = regex.search(line)
            if m:
                rows.append(m.groups())
    return rows


def compare_android(args):
    lines = read_lines(args.base + ".log")
    clk = int(args.clk)
    result = {"mount_ms": mount_ms(lines), "filled_ms": filled_ms(lines)}
    for name, spec, gfx in (("slow", args.slow, args.base + ".slow.gfx"), ("fling", args.fling, args.base + ".gfx")):
        t0, t1, c0, c1 = (float(v) for v in spec.split())
        blank = window_rows(lines, BLANK, t0, t1)
        frames = sum(int(r[1]) for r in blank)
        total, janky, durations = am.parse_frames(gfx)
        js = [(int(a), int(b), int(c), float(d)) for a, b, c, d in window_rows(lines, FPS, t0, t1)]
        result[name] = {
            "frames": total,
            "janky_pct": round(100 * janky / total, 1) if total else None,
            "p50_ms": percentile(durations, 0.5),
            "p95_ms": percentile(durations, 0.95),
            "p99_ms": percentile(durations, 0.99),
            "ui_slow_frames": sum(int(r[2]) for r in blank),
            "blank_frames": sum(int(r[3]) for r in blank),
            "blank_pct": round(sum(float(r[4]) * int(r[1]) for r in blank) / frames, 3) if frames else None,
            "blank_band_dp": max((float(r[5]) for r in blank), default=None),
            "cpu_pct": round(100 * (c1 - c0) / clk / (t1 - t0), 1),
            "js": reduce_windows(js),
        }
    if os.path.exists(args.base + ".threads0"):
        result["fling"]["threads_ms"] = thread_ms(args.base, clk)
    mem = am.parse_mem(args.base + ".mem")
    result["pss_mb"] = round(mem["pss_total_kb"] / 1024, 1) if mem.get("pss_total_kb") else None
    result["views"] = mem.get("views")
    status = "\n".join(read_lines(args.base + ".status"))
    for key, field in (("rss_peak_mb", "VmHWM"), ("rss_end_mb", "VmRSS")):
        m = re.search(field + r":\s+(\d+)", status)
        if m:
            result[key] = round(int(m.group(1)) / 1024, 1)
    write(args.base, result)


def anchor_rows(text):
    rows = {}
    for part in text.split(","):
        key, _, y = part.rpartition(":")
        try:
            rows[key] = float(y)
        except ValueError:
            pass
    return rows


def compare_ops(args):
    """Per update: JS time, frames until rows stop rendering, rows rendered, and how far the
    row at the top of the viewport moved: at the end (shift) and at worst in between (jump)."""
    lines = read_lines(args.base + ".log")
    result = {"mount_ms": mount_ms(lines), "filled_ms": filled_ms(lines)}
    last_anchor = {}
    current = None
    for line in lines:
        a = ANCHOR.search(line)
        if a:
            rows = anchor_rows(a.group(1))
            if current is not None:
                current["seen"].append(rows)
            last_anchor = rows
            continue
        m = OP.search(line)
        if not m:
            continue
        if m.group(1) == "begin":
            current = {"before": last_anchor, "seen": []}
            continue
        if current is None:
            continue
        values = kv(m.group(3))
        entry = {k: values.get(k) for k in ("ms", "longest", "renders", "frames")}
        before = current["before"]
        if m.group(2) != "jump-middle" and before:
            # Follow the first of the top rows that is still on screen afterwards.
            seen = current["seen"]
            final = seen[-1] if seen else before
            key = next((k for k in before if k in final), None)
            if key is None:
                entry["shift_pt"] = None
                entry["jump_pt"] = None
                entry["anchor_lost"] = True
            else:
                y0 = before[key]
                ys = [rows[key] for rows in seen if key in rows]
                entry["shift_pt"] = round(final[key] - y0, 1)
                entry["jump_pt"] = round(max((abs(y - y0) for y in ys), default=0.0), 1)
                entry["anchor_lost"] = False
        result[m.group(2)] = entry
        current = None
    with open(args.base + ".json", "w") as handle:
        json.dump(result, handle, indent=1)
    print("   ops " + json.dumps(result))


def spread(values):
    values = sorted(v for v in values if v is not None)
    if not values:
        return None
    return {
        "median": round(statistics.median(values), 2),
        "p95": round(values[min(len(values) - 1, int(round(0.95 * (len(values) - 1))))], 2),
        "min": round(values[0], 2),
        "max": round(values[-1], 2),
        "n": len(values),
    }


def compare_summary(args):
    """Median, p95, min and max over the measured runs, per scenario, list and metric. A
    difference inside both lists' min..max ranges is marked a tie."""
    out = {}
    for case in sorted(glob.glob(os.path.join(args.dir, "*-*"))):
        if not os.path.isdir(case) or os.path.basename(case) == "video":
            continue
        runs = {}
        for path in glob.glob(os.path.join(case, "*.json")):
            name = os.path.basename(path)[:-5]
            ops = name.endswith(".ops")
            name = name[:-4] if ops else name
            listname, _, run = name.rpartition("-")
            if not run.isdigit() or int(run) < args.warmup:
                continue
            with open(path) as handle:
                data = flat(json.load(handle), "ops." if ops else "")
            runs.setdefault(listname, []).append(data)
        metrics = {}
        for listname, rows in runs.items():
            keys = sorted({k for r in rows for k in r})
            metrics[listname] = {k: spread([r.get(k) for r in rows]) for k in keys}
        out[os.path.basename(case)] = metrics
    with open(os.path.join(args.dir, "summary.json"), "w") as handle:
        json.dump(out, handle, indent=1)
    lines = []
    for case, metrics in out.items():
        lists = list(metrics)
        lines += [f"### {case}", "", "| metric | " + " | ".join(lists) + " |", "|---|" + "---|" * len(lists)]
        keys = sorted({k for m in metrics.values() for k in m})
        for key in keys:
            cells = []
            for listname in lists:
                s = metrics[listname].get(key)
                cells.append(f"{s['median']} ({s['min']}..{s['max']}, n={s['n']})" if s else "")
            lines.append(f"| {key} | " + " | ".join(cells) + " |")
        lines.append("")
    text = "\n".join(lines)
    with open(os.path.join(args.dir, "summary.md"), "w") as handle:
        handle.write(text + "\n")
    print(text)



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
    p = sub.add_parser("compare-ios")
    p.add_argument("base")
    for flag in ("--t0", "--t1", "--cpu0", "--cpu1"):
        p.add_argument(flag, required=True)
    p = sub.add_parser("compare-android")
    p.add_argument("base")
    p.add_argument("--slow", required=True)
    p.add_argument("--fling", required=True)
    p.add_argument("--clk", default="100")
    p = sub.add_parser("compare-ops")
    p.add_argument("base")
    p = sub.add_parser("compare-summary")
    p.add_argument("dir")
    p.add_argument("--warmup", type=int, default=1)
    args = parser.parse_args()
    {"ios": ios, "android": android, "summary": summary, "compare-ios": compare_ios,
     "compare-android": compare_android, "compare-ops": compare_ops,
     "compare-summary": compare_summary}[args.mode](args)


if __name__ == "__main__":
    sys.exit(main())
