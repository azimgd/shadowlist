#!/usr/bin/env python3
"""
Score ShadowRuler captures from ruler-check.sh: ruler-check.py <dir>

Per screen it lists
  off-pixel  views (not text) whose frame doesn't land on a device pixel at the capture's scale
  off-grid   declared padding, margin, gap or border values that aren't whole points or not on
             the 2 pt grid (hairlines, 1/scale, are left out)
  insets     the measured inset and gap values ShadowRuler drew, to compare equal spacings
and writes <dir>/summary.json and <dir>/summary.md.
"""
import collections
import glob
import json
import os
import sys

TEXT_HINTS = ("Text", "Paragraph", "Label")


def on_pixel(value, scale):
    return abs(value * scale - round(value * scale)) < 0.02


def is_text(node):
    return node.get("role") == "text" or any(h in node.get("className", "") for h in TEXT_HINTS)


def ancestors_text(node, by_id):
    seen = 0
    parent = by_id.get(node.get("parentId"))
    while parent is not None and seen < 64:
        if is_text(parent):
            return True
        parent = by_id.get(parent.get("parentId"))
        seen += 1
    return False


def score(path):
    data = json.load(open(path))
    snap = data["snapshot"]
    scale = snap["scale"]
    nodes = snap["nodes"]
    by_id = {n["id"]: n for n in nodes}
    off_pixel, off_grid = [], []
    for node in nodes:
        name = node.get("identifier") or node.get("className", "?")
        if not is_text(node) and not ancestors_text(node, by_id):
            b = node["bounds"]
            bad = {k: round(b[k], 3) for k in ("x", "y", "width", "height") if not on_pixel(b[k], scale)}
            if bad:
                off_pixel.append({"node": name, "id": node["id"], "source": node.get("source"), "values": bad})
        for field in ("padding", "margin", "border"):
            for edge, value in (node.get(field) or {}).items():
                if value == 0 or abs(value - 1 / scale) < 1e-3:
                    continue
                if abs(value - round(value)) > 1e-3 or round(value) % 2:
                    off_grid.append({"node": name, "id": node["id"], "field": f"{field}.{edge}", "value": round(value, 3)})
        for axis, value in (node.get("gap") or {}).items():
            if value and (abs(value - round(value)) > 1e-3 or round(value) % 2):
                off_grid.append({"node": name, "id": node["id"], "field": f"gap.{axis}", "value": round(value, 3)})
    spacing = collections.defaultdict(collections.Counter)
    for m in data.get("measurements", []):
        if m["kind"] in ("width", "height"):
            continue
        spacing[m["kind"]][round(m["value"], 2)] += 1
    return {
        "scale": scale,
        "nodes": len(nodes),
        "off_pixel": off_pixel,
        "off_grid": off_grid,
        "spacing": {k: dict(sorted(v.items())) for k, v in spacing.items()},
    }


def main():
    out = sys.argv[1]
    results = {}
    for path in sorted(glob.glob(os.path.join(out, "*.json"))):
        if os.path.basename(path) == "summary.json":
            continue
        results[os.path.basename(path)[:-5]] = score(path)
    json.dump(results, open(os.path.join(out, "summary.json"), "w"), indent=1)
    lines = ["| screen | nodes | off-pixel views | off-grid declared values | drawn spacings |", "|---|---|---|---|---|"]
    for name, r in results.items():
        grid = collections.Counter(f"{g['field'].split('.')[0]} {g['value']}" for g in r["off_grid"])
        pix = collections.Counter(g["node"] for g in r["off_pixel"])
        spacing = "; ".join(f"{k}: {', '.join(str(v) for v in vals)}" for k, vals in r["spacing"].items())
        lines.append(
            f"| {name} | {r['nodes']} | {len(r['off_pixel'])} {dict(pix.most_common(3)) if pix else ''} | "
            f"{dict(grid.most_common(6)) if grid else 0} | {spacing} |"
        )
    text = "\n".join(lines)
    open(os.path.join(out, "summary.md"), "w").write(text + "\n")
    print(text)


if __name__ == "__main__":
    main()
