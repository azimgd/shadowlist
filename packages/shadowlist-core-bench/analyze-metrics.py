#!/usr/bin/env python3
"""
Reduce one device-metrics.sh run to a comparable report.

The blank-cell measure is the interesting part. A raw screencap is RGBA_8888 behind a
16-byte header, so the framebuffer can be read with nothing but the standard library.
For each sampled frame the list viewport is scanned for the tallest full-width band of
background colour (see blank_band). A band far taller than row padding is a row the
pipeline has not produced yet, which is what a user sees as a blank cell.

The background colour is normally pinned with --background so every build under
comparison is scored against the same reference; without it, it is calibrated from the
settled frame cal.raw.
"""
import argparse
import math
import os
import re
import statistics
import struct

HEADER = 16

# A background band taller than this cannot be row padding; it is a row that is not there.
BLANK_BAND_PX = 200


def read_raw(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if len(data) < HEADER + 4:
        return None
    width, height, _fmt = struct.unpack("<III", data[:12])
    if width <= 0 or height <= 0 or len(data) - HEADER < width * height * 4:
        return None
    return width, height, memoryview(data)[HEADER:]


def dominant_colour(width, pixels, y0, y1, step=7):
    """Most common pixel in the viewport of a settled frame: the list background."""
    counts = {}
    for y in range(y0, y1, step):
        row = y * width * 4
        for x in range(0, width, step):
            offset = row + x * 4
            colour = bytes(pixels[offset:offset + 3])
            counts[colour] = counts.get(colour, 0) + 1
    if not counts:
        return None
    return max(counts.items(), key=lambda item: item[1])[0]


def blank_band(width, pixels, y0, y1, background, step=4, tolerance=6, purity=0.98):
    """
    Height of the tallest FULL-WIDTH band of background inside the list viewport.

    Counting background pixels outright does not measure blank cells: a chat bubble is
    capped at 75% width, so the gutter beside every message is background and a healthy
    list reads as ~40% "blank". A missing row looks different: a horizontal band where
    background spans the entire width, far taller than the few pixels of padding between
    two bubbles.

    So a scanline counts only when it is background nearly all the way across, and the
    result is the tallest run of consecutive such scanlines. Padding between rows yields
    a band of a few pixels; a row the pipeline has not produced yields hundreds.
    """
    red, green, blue = background[0], background[1], background[2]
    longest = current = 0
    for y in range(y0, y1, step):
        row = y * width * 4
        total = hits = 0
        for x in range(0, width, step):
            offset = row + x * 4
            total += 1
            if (abs(pixels[offset] - red) <= tolerance
                    and abs(pixels[offset + 1] - green) <= tolerance
                    and abs(pixels[offset + 2] - blue) <= tolerance):
                hits += 1
        if total and hits / total >= purity:
            current += step
            longest = max(longest, current)
        else:
            current = 0
    return longest


def frame_signature(width, pixels, y0, y1, step=11):
    """
    Cheap content fingerprint of the list viewport.

    Comparing successive fingerprints answers the question every scroll benchmark has to
    answer before any of its other numbers mean anything: did the list actually move? A
    run whose frames are all identical measured a stationary list, however good its frame
    times look.
    """
    accumulator = 0
    for y in range(y0, y1, step):
        row = y * width * 4
        for x in range(0, width, step):
            offset = row + x * 4
            pixel = pixels[offset] + (pixels[offset + 1] << 8) + (pixels[offset + 2] << 16)
            accumulator = (accumulator * 1000003 + pixel) & 0xFFFFFFFFFFFF
    return accumulator


def parse_frames(path):
    """Return (total frames, janky frames, sorted frame durations in ms) from framestats."""
    with open(path, errors="replace") as handle:
        text = handle.read()

    def scalar(pattern):
        match = re.search(pattern, text)
        return int(match.group(1)) if match else None

    durations = []
    for block in re.findall(r"---PROFILEDATA---(.*?)---PROFILEDATA---", text, re.S):
        lines = [line for line in block.strip().splitlines() if line.strip()]
        if not lines:
            continue
        # Resolve columns by NAME. The framestats schema has gained columns over Android
        # releases (FrameTimelineVsyncId, DisplayPresentTime, ...), so fixed indices read
        # the wrong field and silently yield zero usable frames.
        header = [cell.strip() for cell in lines[0].rstrip(",").split(",")]
        try:
            flags_column = header.index("Flags")
            intended_column = header.index("IntendedVsync")
            completed_column = header.index("FrameCompleted")
        except ValueError:
            continue
        for line in lines[1:]:
            cells = line.strip().rstrip(",").split(",")
            if len(cells) < len(header):
                continue
            try:
                values = [int(cell) for cell in cells]
            except ValueError:
                continue
            if values[flags_column] != 0:
                continue
            intended, completed = values[intended_column], values[completed_column]
            if completed <= intended:
                continue
            duration_ms = (completed - intended) / 1e6
            if 0 < duration_ms < 2000:
                durations.append(duration_ms)
    durations.sort()
    total = scalar(r"Total frames rendered:\s+(\d+)")
    janky = scalar(r"Janky frames:\s+(\d+)")
    return total, janky, durations


def parse_mem(path):
    with open(path, errors="replace") as handle:
        text = handle.read()

    def scalar(pattern):
        match = re.search(pattern, text)
        return int(match.group(1)) if match else None

    return {
        "pss_total_kb": scalar(r"TOTAL PSS:\s+(\d+)") or scalar(r"TOTAL\s+(\d+)"),
        "native_heap_kb": scalar(r"Native Heap\s+(\d+)"),
        "gfx_kb": scalar(r"Gfx dev\s+(\d+)"),
        "views": scalar(r"Views:\s+(\d+)"),
        "view_root_impl": scalar(r"ViewRootImpl:\s+(\d+)"),
    }


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    for flag in ("--label", "--screen", "--gfx", "--mem", "--shots"):
        parser.add_argument(flag, required=True)
    parser.add_argument("--cpu-ticks", type=int, required=True)
    parser.add_argument("--clk", type=int, default=100)
    parser.add_argument("--elapsed-ns", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--width", type=int, required=True)
    # Auto-calibration picks the modal colour of one frame, which on a chat list flips
    # between the background and a bubble fill depending on where the list is parked, and a
    # run calibrated to a bubble colour measures something else entirely. Pin it so both
    # builds under comparison are scored against the same reference.
    parser.add_argument("--background", default=None,
                        help="hex background, e.g. 000000; empty calibrates from cal.raw")
    # List viewport as fractions of screen height: inside the list, clear of header chrome
    # and any composer. The defaults fit the example app's Chat screen.
    parser.add_argument("--viewport-top", type=float, default=0.16)
    parser.add_argument("--viewport-bottom", type=float, default=0.76)
    return parser.parse_args()


def main():
    args = parse_args()

    total, janky, durations = parse_frames(args.gfx)
    mem = parse_mem(args.mem)

    y0 = int(args.height * args.viewport_top)
    y1 = int(args.height * args.viewport_bottom)

    blanks, signatures, background = [], [], None
    if args.background:
        hex_colour = args.background.lstrip("#")
        background = bytes(int(hex_colour[i:i + 2], 16) for i in (0, 2, 4))
    calibration_path = os.path.join(args.shots, "cal.raw")
    if background is None and os.path.exists(calibration_path):
        frame = read_raw(calibration_path)
        if frame:
            width, _height, pixels = frame
            background = dominant_colour(width, pixels, y0, y1)
    if background and os.path.isdir(args.shots):
        names = sorted(
            (name for name in os.listdir(args.shots)
             if name.startswith("f") and name.endswith(".raw")),
            key=lambda name: int(re.sub(r"\D", "", name) or 0))
        for name in names:
            frame = read_raw(os.path.join(args.shots, name))
            if frame:
                width, _height, pixels = frame
                blanks.append(blank_band(width, pixels, y0, y1, background))
                signatures.append(frame_signature(width, pixels, y0, y1))

    elapsed_s = args.elapsed_ns / 1e9
    cpu_s = args.cpu_ticks / args.clk
    cpu_pct = 100.0 * cpu_s / elapsed_s if elapsed_s else float("nan")

    def percentile(fraction):
        if not durations:
            return float("nan")
        return durations[min(len(durations) - 1, int(len(durations) * fraction))]

    sampled = len(durations)
    worst = durations[-1] if durations else float("nan")
    over16 = sum(1 for duration in durations if duration > 16.7)
    over33 = sum(1 for duration in durations if duration > 33.3)
    blank_mean = statistics.mean(blanks) if blanks else float("nan")
    blank_worst = max(blanks) if blanks else float("nan")
    blank_frames_over_band = sum(1 for band in blanks if band > BLANK_BAND_PX)
    moved = sum(1 for a, b in zip(signatures, signatures[1:]) if a != b)
    compared = max(len(signatures) - 1, 0)
    moved_pct = 100.0 * moved / compared if compared else float("nan")

    print(f"device metrics: {args.label} / {args.screen}")
    print(f"  elapsed                    {elapsed_s:.2f} s")
    print("  -- frames --")
    print(f"  total frames rendered      {total}")
    janky_share = f"  ({100.0 * janky / total:.1f}%)" if total and janky is not None else ""
    print(f"  janky frames               {janky}{janky_share}")
    print(f"  sampled frames             {sampled}")
    if sampled:
        print(f"  frame time p50 / p90       {percentile(0.50):.2f} / {percentile(0.90):.2f} ms")
        print(f"  frame time p95 / p99       {percentile(0.95):.2f} / {percentile(0.99):.2f} ms")
        print(f"  frame time worst           {worst:.2f} ms")
        print(f"  frames > 16.7ms            {over16}  ({100.0 * over16 / sampled:.1f}%)")
        print(f"  frames > 33.3ms            {over33}  ({100.0 * over33 / sampled:.1f}%)")
    print("  -- blank cells --")
    print(f"  sampled screenshots        {len(blanks)}")
    colour = "#%02x%02x%02x" % tuple(background[:3]) if background else "n/a"
    print(f"  background colour          {colour}")
    print(f"  blank band mean            {blank_mean:.0f} px")
    print(f"  blank band worst           {blank_worst:.0f} px")
    barely_moved = not math.isnan(moved_pct) and moved_pct < 50
    print(f"  frames that changed        {moved}/{compared}  ({moved_pct:.0f}%)"
          + ("   <-- LIST BARELY MOVED" if barely_moved else ""))
    over_band_share = f"  ({100.0 * blank_frames_over_band / len(blanks):.1f}%)" if blanks else ""
    print(f"  frames with band > {BLANK_BAND_PX}px   {blank_frames_over_band}{over_band_share}")
    print("  -- memory --")
    print(f"  PSS total                  {mem['pss_total_kb']} kB")
    print(f"  native heap                {mem['native_heap_kb']} kB")
    print(f"  Gfx dev                    {mem['gfx_kb']} kB")
    print(f"  live Views                 {mem['views']}")
    print("  -- cpu --")
    print(f"  process CPU                {cpu_s:.2f} s  ({cpu_pct:.1f}% of one core)")
    print()
    print("#BEGIN_TSV")
    print("\t".join([
        "label", "screen", "total_frames", "janky", "sampled", "p50", "p90", "p95", "p99",
        "worst", "over16", "over33", "blank_band_mean_px", "blank_band_worst_px",
        f"blank_frames_over{BLANK_BAND_PX}", "frames_changed_pct", "pss_kb", "native_kb",
        "gfx_kb", "views", "cpu_s", "cpu_pct",
    ]))
    print("\t".join(str(value) for value in [
        args.label, args.screen, total, janky, sampled,
        f"{percentile(0.50):.2f}", f"{percentile(0.90):.2f}",
        f"{percentile(0.95):.2f}", f"{percentile(0.99):.2f}", f"{worst:.2f}",
        over16, over33, f"{blank_mean:.0f}", f"{blank_worst:.0f}", blank_frames_over_band,
        f"{moved_pct:.0f}", mem["pss_total_kb"], mem["native_heap_kb"], mem["gfx_kb"],
        mem["views"], f"{cpu_s:.2f}", f"{cpu_pct:.1f}",
    ]))
    print("#END_TSV")


if __name__ == "__main__":
    main()
