#!/usr/bin/env python3
"""
Reduce one device-metrics.sh run to a comparable report.

The blank-cell measure is the interesting part. A raw screencap is RGBA_8888 behind a
16-byte header, so the framebuffer can be read with nothing but the standard library.
For each sampled frame we count, inside the list viewport only, the pixels that match the
background colour calibrated from a settled frame. Bare background inside the list is a
row the pipeline has not produced yet -- exactly what a user sees as a blank cell.

Calibrating rather than hard-coding the colour keeps this honest across screens, themes
and devices, and keeps the two builds being compared on identical footing.
"""
import argparse, os, re, statistics, struct, sys

HEADER = 16

def read_raw(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if len(data) < HEADER + 4:
        return None
    width, height, _fmt = struct.unpack("<III", data[:12])
    if width <= 0 or height <= 0 or len(data) - HEADER < width * height * 4:
        return None
    return width, height, memoryview(data)[HEADER:]

def dominant_colour(width, height, pixels, y0, y1, step=7):
    """Most common pixel in the viewport of a settled frame: the list background."""
    counts = {}
    for y in range(y0, y1, step):
        row = y * width * 4
        for x in range(0, width, step):
            off = row + x * 4
            counts[bytes(pixels[off:off + 3])] = counts.get(bytes(pixels[off:off + 3]), 0) + 1
    return max(counts.items(), key=lambda kv: kv[1])[0] if counts else None

def blank_band(width, height, pixels, y0, y1, background, step=4, tol=6, purity=0.98):
    """
    Height of the tallest FULL-WIDTH band of background inside the list viewport.

    Counting background pixels outright does not measure blank cells: a chat bubble is
    capped at 75% width, so the gutter beside every message is background and a healthy
    list reads as ~40% "blank". What a missing row actually looks like is different --
    a horizontal band where background spans the entire width, far taller than the few
    pixels of padding between two bubbles.

    So a scanline counts only when it is background nearly all the way across, and the
    result is the tallest run of consecutive such scanlines. Padding between rows yields
    a band of a few pixels; a row the pipeline has not produced yields hundreds.
    """
    br, bg, bb = background[0], background[1], background[2]
    longest = current = 0
    for y in range(y0, y1, step):
        row = y * width * 4
        total = hits = 0
        for x in range(0, width, step):
            off = row + x * 4
            total += 1
            if (abs(pixels[off] - br) <= tol and abs(pixels[off + 1] - bg) <= tol
                    and abs(pixels[off + 2] - bb) <= tol):
                hits += 1
        if total and hits / total >= purity:
            current += step
            longest = max(longest, current)
        else:
            current = 0
    return longest

def frame_signature(width, height, pixels, y0, y1, step=11):
    """
    Cheap content fingerprint of the list viewport.

    Comparing successive fingerprints answers the question every scroll benchmark has to
    answer before any of its other numbers mean anything: did the list actually move? A
    run whose frames are all identical measured a stationary list, however good its frame
    times look.
    """
    acc = 0
    for y in range(y0, y1, step):
        row = y * width * 4
        for x in range(0, width, step):
            off = row + x * 4
            acc = (acc * 1000003 + pixels[off] + (pixels[off + 1] << 8) + (pixels[off + 2] << 16)) & 0xFFFFFFFFFFFF
    return acc


def parse_frames(path):
    text = open(path, errors="replace").read()
    def scalar(pattern):
        match = re.search(pattern, text)
        return int(match.group(1)) if match else None
    durations = []
    for block in re.findall(r"---PROFILEDATA---(.*?)---PROFILEDATA---", text, re.S):
        lines = [l for l in block.strip().splitlines() if l.strip()]
        if not lines:
            continue
        # Resolve columns by NAME. The framestats schema has gained columns over Android
        # releases (FrameTimelineVsyncId, DisplayPresentTime, ...), so fixed indices read
        # the wrong field and silently yield zero usable frames.
        header = [h.strip() for h in lines[0].rstrip(",").split(",")]
        try:
            i_flags = header.index("Flags")
            i_intended = header.index("IntendedVsync")
            i_completed = header.index("FrameCompleted")
        except ValueError:
            continue
        for line in lines[1:]:
            cells = line.strip().rstrip(",").split(",")
            if len(cells) < len(header):
                continue
            try:
                values = [int(c) for c in cells]
            except ValueError:
                continue
            if values[i_flags] != 0:
                continue
            intended, completed = values[i_intended], values[i_completed]
            if completed <= intended:
                continue
            ms = (completed - intended) / 1e6
            if 0 < ms < 2000:
                durations.append(ms)
    durations.sort()
    return scalar(r"Total frames rendered:\s+(\d+)"), scalar(r"Janky frames:\s+(\d+)"), durations

def parse_mem(path):
    text = open(path, errors="replace").read()
    def scalar(pattern, group=1):
        match = re.search(pattern, text)
        return int(match.group(group)) if match else None
    return {
        "pss_total_kb": scalar(r"TOTAL PSS:\s+(\d+)") or scalar(r"TOTAL\s+(\d+)"),
        "native_heap_kb": scalar(r"Native Heap\s+(\d+)"),
        "gfx_kb": scalar(r"Gfx dev\s+(\d+)"),
        "views": scalar(r"Views:\s+(\d+)"),
        "view_root_impl": scalar(r"ViewRootImpl:\s+(\d+)"),
    }

def main():
    ap = argparse.ArgumentParser()
    for flag in ("--label", "--screen", "--gfx", "--mem", "--shots"):
        ap.add_argument(flag, required=True)
    ap.add_argument("--cpu-ticks", type=int, required=True)
    ap.add_argument("--clk", type=int, default=100)
    ap.add_argument("--elapsed-ns", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    ap.add_argument("--width", type=int, required=True)
    # Auto-calibration picks the modal colour of one frame, which on a chat list flips
    # between the background and a bubble fill depending on where the list happens to be
    # parked -- and a run calibrated to a bubble colour measures something else entirely.
    # Pin it so both builds under comparison are scored against the same reference.
    ap.add_argument("--background", default=None, help="hex background, e.g. 000000")
    args = ap.parse_args()

    total, janky, durations = parse_frames(args.gfx)
    mem = parse_mem(args.mem)

    # List viewport: inside the list, clear of header chrome and any composer.
    y0, y1 = int(args.height * 0.16), int(args.height * 0.76)

    blanks, signatures, background = [], [], None
    if args.background:
        h = args.background.lstrip("#")
        background = bytes(int(h[i:i + 2], 16) for i in (0, 2, 4))
    cal = os.path.join(args.shots, "cal.raw")
    if background is None and os.path.exists(cal):
        got = read_raw(cal)
        if got:
            width, height, pixels = got
            background = dominant_colour(width, height, pixels, y0, y1)
    if background:
        frames = sorted(
            (f for f in os.listdir(args.shots) if f.startswith("f") and f.endswith(".raw")),
            key=lambda f: int(re.sub(r"\D", "", f) or 0))
        for name in frames:
            got = read_raw(os.path.join(args.shots, name))
            if got:
                width, height, pixels = got
                blanks.append(blank_band(width, height, pixels, y0, y1, background))
                signatures.append(frame_signature(width, height, pixels, y0, y1))

    elapsed_s = args.elapsed_ns / 1e9
    cpu_s = args.cpu_ticks / args.clk
    cpu_pct = 100.0 * cpu_s / elapsed_s if elapsed_s else float("nan")

    def pct(p):
        return durations[min(len(durations) - 1, int(len(durations) * p))] if durations else float("nan")

    n = len(durations)
    over16 = sum(1 for d in durations if d > 16.7)
    over33 = sum(1 for d in durations if d > 33.3)
    # A band taller than this cannot be row padding; it is a row that is not there.
    BLANK_BAND_PX = 200
    blank_mean = statistics.mean(blanks) if blanks else float("nan")
    blank_worst = max(blanks) if blanks else float("nan")
    blank_over5 = sum(1 for b in blanks if b > BLANK_BAND_PX)

    print(f"device metrics: {args.label} / {args.screen}")
    print(f"  elapsed                    {elapsed_s:.2f} s")
    print("  -- frames --")
    print(f"  total frames rendered      {total}")
    print(f"  janky frames               {janky}" + (f"  ({100.0*janky/total:.1f}%)" if total and janky is not None else ""))
    print(f"  sampled frames             {n}")
    if n:
        print(f"  frame time p50 / p90       {pct(0.50):.2f} / {pct(0.90):.2f} ms")
        print(f"  frame time p95 / p99       {pct(0.95):.2f} / {pct(0.99):.2f} ms")
        print(f"  frame time worst           {durations[-1]:.2f} ms")
        print(f"  frames > 16.7ms            {over16}  ({100.0*over16/n:.1f}%)")
        print(f"  frames > 33.3ms            {over33}  ({100.0*over33/n:.1f}%)")
    print("  -- blank cells --")
    print(f"  sampled screenshots        {len(blanks)}")
    print(f"  background colour          {'#%02x%02x%02x' % tuple(background[:3]) if background else 'n/a'}")
    print(f"  blank band mean            {blank_mean:.0f} px")
    print(f"  blank band worst           {blank_worst:.0f} px")
    moved = sum(1 for a, b in zip(signatures, signatures[1:]) if a != b)
    moved_pct = 100.0 * moved / (len(signatures) - 1) if len(signatures) > 1 else float("nan")
    print(f"  frames that changed        {moved}/{max(len(signatures)-1,0)}  ({moved_pct:.0f}%)"
          + ("   <-- LIST BARELY MOVED" if moved_pct == moved_pct and moved_pct < 50 else ""))
    print(f"  frames with band > {BLANK_BAND_PX}px   {blank_over5}"
          + (f"  ({100.0*blank_over5/len(blanks):.1f}%)" if blanks else ""))
    print("  -- memory --")
    print(f"  PSS total                  {mem['pss_total_kb']} kB")
    print(f"  native heap                {mem['native_heap_kb']} kB")
    print(f"  Gfx dev                    {mem['gfx_kb']} kB")
    print(f"  live Views                 {mem['views']}")
    print("  -- cpu --")
    print(f"  process CPU                {cpu_s:.2f} s  ({cpu_pct:.1f}% of one core)")
    print()
    print("#BEGIN_TSV")
    print("label\tscreen\ttotal_frames\tjanky\tsampled\tp50\tp90\tp95\tp99\tworst\tover16\tover33\t"
          "blank_band_mean_px\tblank_band_worst_px\tblank_frames_over200\tframes_changed_pct\tpss_kb\tnative_kb\tgfx_kb\tviews\tcpu_s\tcpu_pct")
    print(f"{args.label}\t{args.screen}\t{total}\t{janky}\t{n}\t{pct(0.50):.2f}\t{pct(0.90):.2f}\t"
          f"{pct(0.95):.2f}\t{pct(0.99):.2f}\t{durations[-1] if n else float('nan'):.2f}\t{over16}\t{over33}\t"
          f"{blank_mean:.0f}\t{blank_worst:.0f}\t{blank_over5}\t{moved_pct:.0f}\t{mem['pss_total_kb']}\t"
          f"{mem['native_heap_kb']}\t{mem['gfx_kb']}\t{mem['views']}\t{cpu_s:.2f}\t{cpu_pct:.1f}")
    print("#END_TSV")

if __name__ == "__main__":
    main()
