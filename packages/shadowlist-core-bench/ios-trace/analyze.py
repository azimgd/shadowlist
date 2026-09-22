#!/usr/bin/env python3
"""
Turn an iOS trace log from run.sh into scroll findings and JS cost.

Input is the console log of simctl launch --console-pty with SHADOWLIST_FRAME_TRACE=1:

  [SLF] t=<s> id=<tag> frame ax=v inv=0 off= cs= vp= ins= ph= ref= hdr=pos+size stt= jump= rows=[ key@pos+size~ ]
  [SLF] t=<s> id=<tag> ev=<name> k=v ...        host events (state, drag-begin, stt-tick, refresh-*, ...)
  [SLJ] t=<s> render id= n= mounted=a..b rows= jsms= refreshing= data=...   JS commit of a list
  [SLJ] t=<s> vis win=a..b n= / vis apply range= / reached start|end / refresh pull|hold|settle
  [SCN] t=<s> <label>                            scenario markers (run.sh writes <log>.marks)
  [SL] ...                                        core trace, no timestamp (kept for --window)

Timestamps are mach_absolute_time seconds. Row positions are on screen positions along the
scroll axis, so a row that keeps its position between two frames did not move for the reader.

Findings:
  idle-shift     visible rows moved with no finger, fling or scroll to top going on.
                 The reader sees a jump. Marked explained when there's a known reason,
                 like an inverted list pinned to its bottom or a refresh inset change.
  reversal       motion flipped for one frame and went back, a jitter, or went backwards
                 during a scroll to top.
  reflow         rows moved against each other with no row above them resizing or appearing.
  blank          an empty stretch of the screen, no row or header, of at least --blank-pt.
  discontinuity  two frames that both show rows share none, meaning a jump landed.
"""
import argparse
import bisect
import json
import re
import statistics
import sys
from collections import defaultdict

LINE = re.compile(r"^\[(SLF|SLJ|SCN)\] t=([0-9.]+) (.*)$")
ROW = re.compile(r"(\S+)@(-?[0-9.]+)\+(-?[0-9.]+)(~?)")
PAIR = re.compile(r"(-?[0-9.]+)->(-?[0-9.]+)")


def kv(text):
    out = {}
    for token in text.split():
        if "=" in token:
            key, value = token.split("=", 1)
            out[key] = value
    return out


def num(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


class Frame:
    __slots__ = ("t", "id", "ax", "inv", "off", "cs", "vp", "ins", "ph", "ref", "hdr_pos", "hdr_size",
                 "stt", "jump", "rows", "pos", "size")

    def __init__(self, t, list_id, body):
        head, _, rows = body.partition(" rows=[")
        fields = kv(head)
        self.t = t
        self.id = list_id
        self.ax = fields.get("ax", "v")
        self.inv = fields.get("inv", "0") == "1"
        self.off = num(fields.get("off"))
        self.cs = num(fields.get("cs"))
        self.vp = num(fields.get("vp"))
        self.ins = num(fields.get("ins"))
        self.ph = int(num(fields.get("ph")))
        self.ref = fields.get("ref", "0") == "1"
        hdr = fields.get("hdr", "-1+0").rsplit("+", 1)
        self.hdr_pos = num(hdr[0], -1.0)
        self.hdr_size = num(hdr[1] if len(hdr) > 1 else 0)
        self.stt = fields.get("stt", "0") == "1"
        self.jump = fields.get("jump", "0") == "1"
        self.rows = [(m.group(1), float(m.group(2)), float(m.group(3)), m.group(4) == "~")
                     for m in ROW.finditer(rows.rstrip(" ]"))]
        counts = defaultdict(int)
        for row in self.rows:
            counts[row[0]] += 1
        # Short keys can collide, so skip any key that appears twice.
        self.pos = {row[0]: row[1] for row in self.rows if counts[row[0]] == 1}
        self.size = {row[0]: row[2] for row in self.rows if counts[row[0]] == 1}

    @property
    def moving(self):
        return self.ph != 0 or self.stt or self.jump


def percentile(values, fraction):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))
    return ordered[index]


def parse(path):
    frames = defaultdict(list)
    events = []          # (t, id, name, fields, raw)
    js = []              # (t, kind, fields, raw)
    marks = []           # (t, label)
    raw = []             # (t, line) with core [SL] lines stamped by the previous timestamp
    last_t = 0.0
    with open(path, errors="replace") as handle:
        for line in handle:
            line = line.rstrip("\r\n")
            match = LINE.match(line)
            if not match:
                if line.startswith("[SL]"):
                    raw.append((last_t, line))
                continue
            tag, t, body = match.group(1), float(match.group(2)), match.group(3)
            last_t = t
            raw.append((t, line))
            if tag == "SCN":
                marks.append((t, body))
                continue
            if tag == "SLJ":
                kind = " ".join(body.split()[:2]) if body.startswith(("vis apply", "reached", "refresh")) else body.split()[0]
                js.append((t, kind, kv(body), body))
                continue
            fields = kv(body)
            list_id = fields.get("id", "?")
            rest = body.split(" ", 1)[1] if " " in body else ""
            if rest.startswith("frame "):
                frames[list_id].append(Frame(t, list_id, rest[len("frame "):]))
            elif rest.startswith("ev="):
                name = rest.split()[0][3:]
                events.append((t, list_id, name, kv(rest), rest))
    try:
        with open(path + ".marks") as handle:
            for line in handle:
                match = LINE.match(line.strip())
                if match and match.group(1) == "SCN":
                    marks.append((float(match.group(2)), match.group(3)))
                    raw.append((float(match.group(2)), line.strip()))
    except OSError:
        pass
    raw.sort(key=lambda item: item[0])
    return frames, events, js, sorted(marks), raw


def analyze_list(list_frames, list_events, blank_pt):
    findings = []
    motions = []
    discontinuities = 0
    gesture_times = [t for t, _, name, _, _ in list_events if name in ("drag-begin", "drag-end")]
    # scrollToIndex and scrollToEnd move the list on purpose.
    command_times = [t for t, _, name, _, _ in list_events
                     if name in ("cmd-scroll-to-index", "cmd-scroll-to-end")]
    stats = defaultdict(float)
    stats["frames"] = len(list_frames)

    for index in range(1, len(list_frames)):
        prev, cur = list_frames[index - 1], list_frames[index]
        shared = [key for key in cur.pos if key in prev.pos]
        if not shared:
            if prev.rows and cur.rows:
                discontinuities += 1
                reason = []
                if cur.stt or prev.jump:
                    reason.append("scroll-to-top-jump")
                elif cur.ph == 2 and abs(cur.off - prev.off) > 0.8 * cur.vp:
                    reason.append("fast-fling")
                elif cur.inv and abs((cur.cs - cur.off) - cur.vp) < 2.0 and cur.off > prev.off:
                    reason.append("inverted-bottom-pin")
                elif any(cur.t - 1.0 <= command <= cur.t for command in command_times):
                    reason.append("scroll-command")
                findings.append({"t": cur.t, "kind": "discontinuity", "explained": ",".join(reason),
                                 "detail": f"off {prev.off:.1f}->{cur.off:.1f} ph={cur.ph} stt={int(cur.stt)}"})
            continue
        deltas = {key: cur.pos[key] - prev.pos[key] for key in shared}
        # Take the list's motion from the first visible row, not the median. An insert on
        # screen only moves the rows after it, and a median would blame both groups.
        shift = deltas[min(shared, key=lambda key: cur.pos[key])]
        motions.append((cur.t, shift, cur.ph, cur.stt or prev.stt))

        # Rows added on screen push the rows after them down while the rows above hold.
        # That's expected, so note it here and don't count it as a reflow or idle shift.
        new_keys = [row[0] for row in cur.rows if row[0] not in prev.pos and row[0] in cur.pos]
        gone_keys = [key for key in prev.pos if key not in cur.pos]
        def structure_holds(boundary, position_of):
            """Check rows above the boundary moved with the list and rows below all moved together."""
            above = [deltas[key] for key in shared if position_of(key) < boundary]
            below = [deltas[key] for key in shared if position_of(key) > boundary]
            if not below or any(abs(delta - shift) > 0.5 for delta in above):
                return False
            return max(below) - min(below) <= 0.5

        insert_in_viewport = False
        remove_in_viewport = False
        if new_keys:
            insert_in_viewport = structure_holds(
                min(cur.pos[key] for key in new_keys), lambda key: cur.pos[key])
        if gone_keys and not insert_in_viewport:
            # Same for removes, like a tree node collapsing: rows below move up, rows above hold.
            remove_in_viewport = structure_holds(
                min(prev.pos[key] for key in gone_keys), lambda key: prev.pos[key])
        structure_change = insert_in_viewport or remove_in_viewport

        # Find rows that moved against the list with no resize or insert above them.
        ordered = sorted(cur.rows, key=lambda row: row[1])
        explained = False
        unexplained = []
        for key, position, size, _ in ordered:
            if key not in deltas:
                if key not in prev.pos:
                    explained = True  # a new row pushes the rows after it
                continue
            if abs(prev.size.get(key, size) - size) > 0.5:
                explained = True
            if abs(deltas[key] - shift) > 0.5 and not explained:
                unexplained.append((key, deltas[key] - shift))
        if unexplained and not structure_change:
            worst = max(unexplained, key=lambda item: abs(item[1]))
            findings.append({"t": cur.t, "kind": "reflow", "magnitude": abs(worst[1]),
                             "detail": f"{len(unexplained)} rows, worst {worst[0]} {worst[1]:+.1f}pt ph={cur.ph}"})

        idle = not prev.moving and not cur.moving
        touched = any(prev.t < t <= cur.t for t in gesture_times)
        if idle and not touched and abs(shift) > 0.5:
            reason = []
            if insert_in_viewport:
                reason.append("insert-in-viewport")
            if remove_in_viewport:
                reason.append("remove-in-viewport")
            if any(cur.t - 1.0 <= command <= cur.t for command in command_times):
                reason.append("scroll-command")
            # At the very top, growth above like a header spinner can only push rows down.
            if cur.off <= 0.5 and cur.cs > prev.cs + 0.5 and shift > 0.0:
                reason.append("grew-at-content-start")
            # Content shorter than the screen can't scroll to absorb an insert above.
            if cur.cs <= cur.vp + 0.5 or prev.cs <= prev.vp + 0.5:
                reason.append("content-shorter-than-viewport")
            if abs(cur.ins - prev.ins) > 0.5 or cur.ref or prev.ref:
                reason.append("refresh-inset")
            # Past either edge, from a bounce or the refresh control closing.
            if min(prev.off, cur.off) < -0.5 or max(prev.off, cur.off) > max(0.0, cur.cs - cur.vp) + 0.5:
                reason.append("overscroll-settle")
            # An inverted list pinned to its bottom while content grows there.
            if cur.inv and abs((cur.cs - cur.off) - cur.vp) < 2.0 and shift < 0 and \
               (new_keys or cur.cs > prev.cs or cur.off > prev.off):
                reason.append("inverted-bottom-pin")
            findings.append({"t": cur.t, "kind": "idle-shift", "magnitude": abs(shift),
                             "explained": ",".join(reason),
                             "detail": f"rows {shift:+.1f}pt off {prev.off:.1f}->{cur.off:.1f} cs {prev.cs:.1f}->{cur.cs:.1f}"
                                       f" hdr {prev.hdr_size:.0f}->{cur.hdr_size:.0f} new={len(new_keys)}"})

        # Look for empty stretches on screen.
        start = max(0.0, -cur.off)
        end = min(cur.vp, cur.cs - cur.off)
        spans = sorted([(row[1], row[1] + row[2]) for row in cur.rows] +
                       ([(cur.hdr_pos, cur.hdr_pos + cur.hdr_size)] if cur.hdr_size > 0 else []))
        reached = start
        largest = 0.0
        where = ""
        for low, high in spans:
            if low > reached and low - reached > largest:
                largest, where = low - reached, f"{reached:.0f}..{low:.0f}"
            reached = max(reached, high)
        if end - reached > largest:
            largest, where = end - reached, f"{reached:.0f}..{end:.0f} (trailing)"
        if largest >= blank_pt and cur.rows:
            findings.append({"t": cur.t, "kind": "blank", "magnitude": largest,
                             "detail": f"{largest:.0f}pt at {where} ph={cur.ph} off={cur.off:.0f}"})

    # Find one frame reversals and backward steps during scroll to top.
    # Skip frames past an edge, where the bounce back reverses direction on its own.
    overscrolled = {
        frame.t for frame in list_frames
        if frame.off < -0.5 or frame.off > max(0.0, frame.cs - frame.vp) + 0.5
    }
    moving = [m for m in motions if abs(m[1]) >= 1.0 and m[0] not in overscrolled]
    for index, (t, shift, phase, stt) in enumerate(moving):
        if stt and shift < -0.5:
            findings.append({"t": t, "kind": "reversal", "magnitude": abs(shift),
                             "detail": f"scroll-to-top moved content {shift:+.1f}pt (backwards)"})
            continue
        if 0 < index < len(moving) - 1:
            before, after = moving[index - 1], moving[index + 1]
            if t - before[0] < 0.1 and after[0] - t < 0.1 and \
               (shift > 0) != (before[1] > 0) and (after[1] > 0) == (before[1] > 0):
                findings.append({"t": t, "kind": "reversal", "magnitude": abs(shift),
                                 "detail": f"{before[1]:+.1f} {shift:+.1f} {after[1]:+.1f}pt ph={phase}"})

    states = [e for e in list_events if e[2] == "state"]
    moved = 0
    for _, _, _, fields, _ in states:
        pair = PAIR.match(fields.get("off", ""))
        if fields.get("enabled") == "1" and pair and abs(float(pair.group(1)) - float(pair.group(2))) > 0.01:
            moved += 1
    stats["states"] = len(states)
    stats["corrections"] = sum(1 for e in states if e[3].get("enabled") == "1")
    stats["corrections_moved"] = moved
    stats["discontinuities"] = discontinuities
    return findings, stats


def summarize(path, blank_pt, min_shift):
    frames, events, js, marks, raw = parse(path)
    all_t = [f.t for fs in frames.values() for f in fs] + [e[0] for e in events] + [j[0] for j in js]
    t0 = min(all_t) if all_t else 0.0
    t1 = max(all_t) if all_t else 0.0
    report = {"file": path, "duration": t1 - t0, "t0": t0, "lists": {}, "js": {}, "marks": marks}
    events_by_list = defaultdict(list)
    for event in events:
        events_by_list[event[1]].append(event)

    for list_id, list_frames in frames.items():
        findings, stats = analyze_list(list_frames, events_by_list[list_id], blank_pt)
        findings = [f for f in findings if f["kind"] != "idle-shift" or f["magnitude"] >= min_shift]
        renders = [j for j in js if j[1] == "render" and j[2].get("id") == list_id]
        jsms = [num(j[2].get("jsms")) for j in renders]
        # Time from a JS render with rows to the list's next frame. A fling that outruns
        # JS shows up here before it shows up as a blank.
        frame_times = [frame.t for frame in list_frames]
        mount_delays = []
        for render in renders:
            if num(render[2].get("rows")) <= 0:
                continue
            frame_index = bisect.bisect_right(frame_times, render[0])
            if frame_index < len(frame_times):
                delay = (frame_times[frame_index] - render[0]) * 1000.0
                # Frames only print when the picture changes. A long gap means the render
                # changed nothing on screen, not that it took seconds to mount.
                if delay <= 250.0:
                    mount_delays.append(delay)
        report["lists"][list_id] = {
            "ax": list_frames[0].ax, "inverted": list_frames[0].inv, "stats": stats, "findings": findings,
            "renders": len(renders), "jsms_p50": percentile(jsms, 0.5), "jsms_p95": percentile(jsms, 0.95),
            "jsms_max": max(jsms) if jsms else 0.0, "rows_rendered": sum(num(j[2].get("rows")) for j in renders),
            "mount_p50": percentile(mount_delays, 0.5), "mount_p95": percentile(mount_delays, 0.95),
            "mount_max": max(mount_delays) if mount_delays else 0.0,
        }
    # Rows left hidden on screen by the native view band. Only debug builds log these.
    report["hook_on_screen_hidden"] = sum(1 for _, line in raw if "hook: on-screen hidden row" in line)
    report["js"] = {
        "vis": sum(1 for j in js if j[1] == "vis"),
        "vis_apply": sum(1 for j in js if j[1] == "vis apply"),
        "renders": sum(1 for j in js if j[1] == "render"),
        "data_changes": [(round(j[0] - t0, 3), j[2].get("id"), j[3].split("data=", 1)[1])
                         for j in js if j[1] == "render" and "data=" in j[3]],
    }
    return report, frames, events, js, marks, raw


def print_report(report, frames, events, js, marks, timeline_limit):
    t0 = report["t0"]
    print(f"== {report['file']}  duration={report['duration']:.1f}s lists={len(report['lists'])}")
    for list_id, info in sorted(report["lists"].items(), key=lambda item: -item[1]["stats"]["frames"]):
        counts = defaultdict(int)
        worst = defaultdict(float)
        for finding in info["findings"]:
            kind = finding["kind"] + ("(explained)" if finding.get("explained") else "")
            counts[kind] += 1
            worst[kind] = max(worst[kind], finding.get("magnitude", 0.0))
        found = " ".join(f"{kind}={counts[kind]}(max {worst[kind]:.0f})" for kind in sorted(counts)) or "clean"
        stats = info["stats"]
        print(f"list {list_id} ax={info['ax']} inv={int(info['inverted'])} frames={int(stats['frames'])}: {found}")
        print(f"  host: states={int(stats['states'])} corrections={int(stats['corrections'])} "
              f"moved={int(stats['corrections_moved'])}")
        print(f"  js:   renders={info['renders']} jsms p50={info['jsms_p50']:.1f} p95={info['jsms_p95']:.1f} "
              f"max={info['jsms_max']:.1f} rows={int(info['rows_rendered'])} "
              f"mountms p50={info['mount_p50']:.1f} p95={info['mount_p95']:.1f} max={info['mount_max']:.1f}")
    print(f"js: vis={report['js']['vis']} vis_apply={report['js']['vis_apply']} renders={report['js']['renders']}"
          f"  hook-on-screen-hidden={report['hook_on_screen_hidden']}")

    items = [(t, f"mark {label}") for t, label in marks]
    for t, list_id, name, fields, rest in events:
        if name in ("drag-begin", "drag-end", "decel-end", "stt-start", "stt-land", "stt-finish",
                    "refresh-pull", "refresh-prop", "refresh-settle", "cmd-scroll-to-index",
                    "cmd-scroll-to-end"):
            items.append((t, f"[{list_id}] {rest}"))
    for t, kind, fields, body in js:
        if kind in ("reached start", "reached end", "refresh pull", "refresh hold", "refresh settle") or \
           (kind == "render" and "data=" in body):
            items.append((t, f"js {body}"))
    for list_id, info in report["lists"].items():
        for finding in info["findings"]:
            if finding["kind"] == "reflow" and finding.get("magnitude", 0) < 2.0:
                continue
            flag = "  " if finding.get("explained") else "!!"
            extra = f" [{finding['explained']}]" if finding.get("explained") else ""
            items.append((finding["t"], f"{flag} [{list_id}] {finding['kind']}{extra}: {finding['detail']}"))
    items.sort(key=lambda item: item[0])
    print(f"-- timeline ({len(items)} items{', truncated' if len(items) > timeline_limit else ''})")
    for t, text in items[:timeline_limit]:
        print(f"{t - t0:8.3f}  {text}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log")
    parser.add_argument("--blank-pt", type=float, default=120.0, help="smallest uncovered span reported")
    parser.add_argument("--min-shift", type=float, default=0.5, help="smallest idle shift reported (pt)")
    parser.add_argument("--timeline", type=int, default=150, help="max timeline lines")
    parser.add_argument("--json", help="also write the report as JSON here")
    parser.add_argument("--window", help="print raw lines between two relative times, e.g. 12.3:12.8")
    args = parser.parse_args()

    report, frames, events, js, marks, raw = summarize(args.log, args.blank_pt, args.min_shift)
    if args.window:
        low, high = (float(v) for v in args.window.split(":"))
        for t, line in raw:
            if low <= t - report["t0"] <= high:
                print(f"{t - report['t0']:8.3f}  {line}")
        return
    print_report(report, frames, events, js, marks, args.timeline)
    if args.json:
        with open(args.json, "w") as handle:
            json.dump(report, handle, indent=1, default=str)


if __name__ == "__main__":
    sys.exit(main())
