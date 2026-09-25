#!/usr/bin/env python3
"""
Reduce perf-suite.sh runs to per-run records, a summary, and a before/after comparison.

    perf.py ios-run <log> <ps.tsv> <out.json>          one iOS run: analyzer JSON + ps samples
    perf.py android-run <prefix> <out.json>            one Android run: <prefix>.metrics.txt,
                                                       .slc.txt, .fling-s, .mvcp-*.xml
    perf.py summary <dir>                              <dir>/*.run.json -> summary.json + summary.md
    perf.py compare <before-dir|label> <after-dir|label> [--platform ios|android]

A label is a directory under results/perf. Numbers in the summary are medians over runs.
"""
import argparse
import glob
import json
import math
import os
import re
import statistics
import sys
import xml.etree.ElementTree as ElementTree

HERE = os.path.dirname(os.path.abspath(__file__))
PERF_ROOT = os.path.join(HERE, "results", "perf")
KINDS = ("blank", "idle-shift", "reversal", "reflow", "discontinuity")


def parse_cpu_time(text):
    """ps TIME is [[dd-]hh:]mm:ss.cc; return seconds."""
    days = 0
    if "-" in text:
        day_text, text = text.split("-", 1)
        days = int(day_text)
    seconds = 0.0
    for part in text.split(":"):
        seconds = seconds * 60 + float(part)
    return days * 86400 + seconds


def ios_run(log, ps_path, out):
    with open(log + ".json") as handle:
        report = json.load(handle)
    lists = report.get("lists", {})
    record = {"platform": "ios", "log": log, "duration_s": report.get("duration", 0.0)}
    counts = {kind: {"explained": 0, "unexplained": 0} for kind in KINDS}
    for info in lists.values():
        for finding in info.get("findings", []):
            # The analyzer's own timeline hides reflows under 2pt; do the same.
            if finding["kind"] == "reflow" and finding.get("magnitude", 0) < 2.0:
                continue
            bucket = "explained" if finding.get("explained") else "unexplained"
            counts.setdefault(finding["kind"], {"explained": 0, "unexplained": 0})[bucket] += 1
    record["findings"] = counts
    # The list under test is the one that drew the most frames.
    main = max(lists.values(), key=lambda info: info["stats"]["frames"]) if lists else {}
    for key in ("jsms_p50", "jsms_p95", "mount_p50", "mount_p95", "renders", "commits",
                "scroll_s", "commits_per_scroll_s"):
        record[key] = main.get(key)
    record["frames"] = main.get("stats", {}).get("frames")
    record["corrections"] = main.get("stats", {}).get("corrections")

    samples = []
    if os.path.exists(ps_path):
        with open(ps_path) as handle:
            for line in handle:
                cells = line.split()
                if len(cells) < 5:
                    continue
                try:
                    samples.append((float(cells[0]), int(cells[1]), float(cells[2]),
                                    int(cells[3]), parse_cpu_time(cells[4])))
                except ValueError:
                    continue
    if samples:
        # The run relaunches the app, so keep only the process that ran the scenario.
        pid = samples[-1][1]
        own = [sample for sample in samples if sample[1] == pid]
        elapsed = own[-1][0] - own[0][0]
        cpu = own[-1][4] - own[0][4]
        record["cpu_pct"] = 100.0 * cpu / elapsed if elapsed > 0 else None
        record["cpu_pct_peak"] = max(sample[2] for sample in own)
        record["rss_max_mb"] = max(sample[3] for sample in own) / 1024.0
        record["rss_end_mb"] = own[-1][3] / 1024.0
        record["cpu_samples"] = len(own)
    write_json(out, record)


def read_tsv(path):
    with open(path) as handle:
        lines = handle.read().splitlines()
    try:
        start = lines.index("#BEGIN_TSV")
    except ValueError:
        return {}
    header = lines[start + 1].split("\t")
    values = lines[start + 2].split("\t")
    return dict(zip(header, values))


def to_number(value):
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return None if math.isnan(number) else number


def ui_nodes(path):
    """Text nodes from a uiautomator dump: key -> (top, bottom), keeping keys seen once."""
    try:
        root = ElementTree.parse(path).getroot()
    except (ElementTree.ParseError, OSError):
        return {}
    seen = {}
    for node in root.iter("node"):
        key = (node.get("text") or "").strip() or (node.get("content-desc") or "").strip()
        match = re.match(r"\[(-?\d+),(-?\d+)\]\[(-?\d+),(-?\d+)\]", node.get("bounds", ""))
        if not key or not match:
            continue
        x1, y1, x2, y2 = (int(value) for value in match.groups())
        if y2 <= y1 or x2 <= x1:
            continue
        seen.setdefault(key, []).append((y1, y2))
    return {key: spans[0] for key, spans in seen.items() if len(spans) == 1}


def mvcp_check(before, after, top, bottom, tolerance_px):
    """Visible text rows must keep their screen position across an insert outside the view."""
    old, new = ui_nodes(before), ui_nodes(after)
    deltas = [new[key][0] - old[key][0] for key in old
              if key in new and top <= old[key][0] and old[key][1] <= bottom]
    if not deltas:
        return {"shared": 0, "pass": None}
    moved = sorted(abs(delta) for delta in deltas)
    median = statistics.median(deltas)
    return {"shared": len(deltas), "median_px": median, "max_abs_px": moved[-1],
            "pass": abs(median) <= tolerance_px}


def gfx_summary(path):
    """Whole-run frame percentiles and janky share from the gfxinfo histogram. The framestats
    percentiles in .metrics.txt only cover the last ~120 frames, the end of the run."""
    record = {}
    try:
        with open(path, errors="replace") as handle:
            text = handle.read()
    except OSError:
        return record
    for key in ("50", "90", "99"):
        match = re.search(rf"^{key}th percentile: (\d+)ms", text, re.MULTILINE)
        if match:
            record[f"gfx_p{key}_ms"] = float(match.group(1))
    match = re.search(r"^Janky frames: \d+ \(([\d.]+)%\)", text, re.MULTILINE)
    if match:
        record["gfx_janky_pct"] = float(match.group(1))
    return record


def android_run(prefix, out, top, bottom, tolerance_px):
    metrics = read_tsv(prefix + ".metrics.txt") if os.path.exists(prefix + ".metrics.txt") else {}
    record = {"platform": "android", "prefix": prefix}
    total = to_number(metrics.get("total_frames"))
    janky = to_number(metrics.get("janky"))
    record["frames"] = total
    record["janky_pct"] = 100.0 * janky / total if total and janky is not None else None
    for key in ("p50", "p90", "p99"):
        record[f"frame_{key}_ms"] = to_number(metrics.get(key))
    record["blank_band_mean_px"] = to_number(metrics.get("blank_band_mean_px"))
    record["blank_band_worst_px"] = to_number(metrics.get("blank_band_worst_px"))
    record["blank_frames_over200"] = to_number(metrics.get("blank_frames_over200"))
    record["frames_changed_pct"] = to_number(metrics.get("frames_changed_pct"))
    record["cpu_pct"] = to_number(metrics.get("cpu_pct"))
    pss = to_number(metrics.get("pss_kb"))
    record["pss_mb"] = pss / 1024.0 if pss else None
    record.update(gfx_summary(prefix + ".gfx"))
    commits = 0
    if os.path.exists(prefix + ".slc.txt"):
        with open(prefix + ".slc.txt", errors="replace") as handle:
            commits = sum(1 for line in handle if "[SLC]" in line)
    fling_s = None
    if os.path.exists(prefix + ".fling-s"):
        with open(prefix + ".fling-s") as handle:
            fling_s = to_number(handle.read().strip())
    record["commits"] = commits
    record["fling_s"] = fling_s
    record["commits_per_s"] = commits / fling_s if fling_s else None
    for action in ("prepend", "append"):
        before = f"{prefix}.mvcp-{action}-before.xml"
        after = f"{prefix}.mvcp-{action}-after.xml"
        if os.path.exists(before) and os.path.exists(after):
            record[f"mvcp_{action}"] = mvcp_check(before, after, top, bottom, tolerance_px)
    write_json(out, record)


def write_json(path, data):
    with open(path, "w") as handle:
        json.dump(data, handle, indent=1)
        handle.write("\n")


# Flattened metrics per platform: (name, getter, lower is better).
def ios_metrics():
    metrics = []
    for kind in KINDS:
        metrics.append((f"{kind} !!", lambda r, k=kind: r["findings"].get(k, {}).get("unexplained"), True))
        metrics.append((f"{kind} expl", lambda r, k=kind: r["findings"].get(k, {}).get("explained"), True))
    for key in ("jsms_p50", "jsms_p95", "mount_p50", "mount_p95", "commits_per_scroll_s", "cpu_pct",
                "rss_max_mb"):
        metrics.append((key, lambda r, k=key: r.get(k), True))
    return metrics


def android_metrics():
    metrics = []
    for key in ("gfx_p50_ms", "gfx_p90_ms", "gfx_p99_ms", "gfx_janky_pct",
                "frame_p50_ms", "frame_p90_ms", "frame_p99_ms", "janky_pct", "blank_band_mean_px",
                "blank_band_worst_px", "blank_frames_over200", "cpu_pct", "pss_mb", "commits_per_s"):
        metrics.append((key, lambda r, k=key: r.get(k), True))
    for action in ("prepend", "append"):
        metrics.append((f"mvcp_{action} |median px|",
                        lambda r, a=action: abs(r[f"mvcp_{a}"]["median_px"])
                        if r.get(f"mvcp_{a}", {}).get("shared") else None, True))
        metrics.append((f"mvcp_{action} fail",
                        lambda r, a=action: None if r.get(f"mvcp_{a}", {}).get("pass") is None
                        else (0 if r[f"mvcp_{a}"]["pass"] else 1), True))
    return metrics


def median(values):
    values = [value for value in values if value is not None]
    return statistics.median(values) if values else None


def load_runs(directory):
    groups = {}
    for path in sorted(glob.glob(os.path.join(directory, "*.run.json"))):
        name = os.path.basename(path)[:-len(".run.json")]
        match = re.match(r"(.+)-(\d+)-(\d+)$", name)
        if not match:
            continue
        screen, count = match.group(1), int(match.group(2))
        with open(path) as handle:
            record = json.load(handle)
        # Runs recorded before the gfx fields existed read them from the .gfx next to them.
        if record.get("platform") == "android" and "gfx_p50_ms" not in record:
            record.update(gfx_summary(path[:-len(".run.json")] + ".gfx"))
        groups.setdefault((screen, count), []).append(record)
    return groups


def summarize(directory):
    groups = load_runs(directory)
    if not groups:
        sys.exit(f"no *.run.json in {directory}")
    platform = next(iter(groups.values()))[0]["platform"]
    metrics = ios_metrics() if platform == "ios" else android_metrics()
    summary = {"platform": platform, "dir": directory, "configs": {}}
    for (screen, count), runs in sorted(groups.items()):
        summary["configs"][f"{screen}-{count}"] = {
            "screen": screen, "count": count, "runs": len(runs),
            "median": {name: median([get(run) for run in runs]) for name, get, _ in metrics},
        }
    write_json(os.path.join(directory, "summary.json"), summary)
    names = [name for name, _, _ in metrics]
    lines = [f"# perf {os.path.basename(os.path.dirname(directory.rstrip('/')))} / {platform}", "",
             "Medians over runs. `!!` = unexplained analyzer findings." if platform == "ios"
             else "Medians over runs. mvcp fail = visible rows moved after an insert off screen.", "",
             "| config | runs | " + " | ".join(names) + " |",
             "| --- | ---: | " + " | ".join("---:" for _ in names) + " |"]
    for key, config in summary["configs"].items():
        cells = [fmt(config["median"][name]) for name in names]
        lines.append(f"| {key} | {config['runs']} | " + " | ".join(cells) + " |")
    with open(os.path.join(directory, "summary.md"), "w") as handle:
        handle.write("\n".join(lines) + "\n")
    print("\n".join(lines))


def fmt(value):
    if value is None:
        return "-"
    if isinstance(value, float) and not value.is_integer():
        return f"{value:.1f}"
    return f"{value:g}"


def resolve(label, platform):
    if os.path.isdir(os.path.join(label, platform)):
        return os.path.join(label, platform)
    if os.path.isfile(os.path.join(label, "summary.json")):
        return label
    return os.path.join(PERF_ROOT, label, platform)


def compare(before, after, platforms):
    for platform in platforms:
        paths = [os.path.join(resolve(label, platform), "summary.json") for label in (before, after)]
        if not all(os.path.exists(path) for path in paths):
            continue
        with open(paths[0]) as handle:
            old = json.load(handle)
        with open(paths[1]) as handle:
            new = json.load(handle)
        lower_better = {name: lower for name, _, lower in
                        (ios_metrics() if platform == "ios" else android_metrics())}
        print(f"\n### {platform}: {before} -> {after}\n")
        print(f"| config | metric | {before} | {after} | change |")
        print("| --- | --- | ---: | ---: | --- |")
        for key in old["configs"]:
            if key not in new["configs"]:
                continue
            for name, value in old["configs"][key]["median"].items():
                other = new["configs"][key]["median"].get(name)
                if value is None and other is None:
                    continue
                print(f"| {key} | {name} | {fmt(value)} | {fmt(other)} | "
                      f"{change(value, other, lower_better.get(name, True))} |")


def change(before, after, lower_is_better):
    if before is None or after is None:
        return "-"
    if before == after:
        return "same"
    if before == 0:
        return "worse" if (after > 0) == lower_is_better else "better"
    ratio = (after - before) / abs(before) * 100.0
    better = (ratio < 0) == lower_is_better
    if abs(ratio) < 5:
        return f"{ratio:+.0f}%"
    return f"{ratio:+.0f}% {'better' if better else 'worse'}"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    ios = sub.add_parser("ios-run")
    ios.add_argument("log")
    ios.add_argument("ps")
    ios.add_argument("out")
    android = sub.add_parser("android-run")
    android.add_argument("prefix")
    android.add_argument("out")
    android.add_argument("--top", type=int, default=0, help="list viewport top, px")
    android.add_argument("--bottom", type=int, default=100000, help="list viewport bottom, px")
    android.add_argument("--tolerance", type=float, default=6.0, help="MVCP tolerance, px")
    summary = sub.add_parser("summary")
    summary.add_argument("dir")
    comparison = sub.add_parser("compare")
    comparison.add_argument("before")
    comparison.add_argument("after")
    comparison.add_argument("--platform", choices=("ios", "android"))
    args = parser.parse_args()
    if args.command == "ios-run":
        ios_run(args.log, args.ps, args.out)
    elif args.command == "android-run":
        android_run(args.prefix, args.out, args.top, args.bottom, args.tolerance)
    elif args.command == "summary":
        summarize(args.dir)
    else:
        compare(args.before, args.after, [args.platform] if args.platform else ["ios", "android"])


if __name__ == "__main__":
    main()
