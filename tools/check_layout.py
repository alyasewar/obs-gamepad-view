#!/usr/bin/env python3
"""Verify each controller's layout.json against its rendered overlay sheet.

base.svg and overlay.svg duplicate their geometry (rsvg will not follow
cross-file references), so a coordinate edited in one and not the other would
silently mis-slice sprites at runtime. This catches that:

  * every src rect is inside the sheet, and every dst lands inside the canvas
  * every src rect actually contains ink -- an empty slice means the sprite
    moved out from under its rect
  * no two src rects overlap, so slicing one sprite cannot bleed in a neighbour

Run: python3 tools/check_layout.py
"""

import json
import pathlib
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTROLLERS = ROOT / "controllers"


def rasterise(svg: pathlib.Path) -> Image.Image:
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        out = pathlib.Path(tmp.name)
    subprocess.run(["rsvg-convert", str(svg), "-o", str(out)], check=True)
    img = Image.open(out).convert("RGBA")
    out.unlink()
    return img


def intersection(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    x0, y0 = max(ax, bx), max(ay, by)
    x1, y1 = min(ax + aw, bx + bw), min(ay + ah, by + bh)
    if x0 >= x1 or y0 >= y1:
        return None
    return (x0, y0, x1, y1)


def has_ink(sheet, box):
    return sheet.crop(box).getchannel("A").getextrema()[1] > 8


def bleeds(sheet, a, b):
    """Two src rects only actually bleed if their shared region carries ink.

    The four face buttons sit on a diamond, so adjacent bboxes clip corners --
    but the circles never reach those corners, leaving them transparent. Only
    ink in the shared region can contaminate a slice.
    """
    box = intersection(a, b)
    return box is not None and has_ink(sheet, box)


def check(pad_dir: pathlib.Path) -> list[str]:
    errors = []
    layout = json.loads((pad_dir / "layout.json").read_text())
    sheet = rasterise(pad_dir / "overlay.svg")
    sw, sh = sheet.size
    cw, ch = layout["canvas"]["w"], layout["canvas"]["h"]

    if (sw, sh) != (layout["sheet"]["w"], layout["sheet"]["h"]):
        errors.append(f"sheet is {sw}x{sh}, layout.json declares "
                      f"{layout['sheet']['w']}x{layout['sheet']['h']}")

    rects = {name: tuple(p["src"]) for name, p in layout["parts"].items()}
    for side, spec in layout["sticks"].items():
        rects[f"stick_{side}_neutral"] = tuple(spec["neutral"])
        rects[f"stick_{side}_pressed"] = tuple(spec["pressed"])

    for name, (x, y, w, h) in sorted(rects.items()):
        if x < 0 or y < 0 or x + w > sw or y + h > sh:
            errors.append(f"{name}: src {x},{y},{w},{h} falls outside the {sw}x{sh} sheet")
            continue
        alpha = sheet.crop((x, y, x + w, y + h)).getchannel("A")
        lo, hi = alpha.getextrema()
        if hi == 0:
            errors.append(f"{name}: src {x},{y},{w},{h} is empty -- sprite moved?")
        else:
            covered = sum(1 for px in alpha.getdata() if px > 8) / float(w * h)
            if covered < 0.10:
                errors.append(f"{name}: src rect only {covered:.0%} covered -- likely mis-placed")

    for name, part in sorted(layout["parts"].items()):
        dx, dy = part.get("dst", part["src"][:2])
        _, _, w, h = part["src"]
        if dx < 0 or dy < 0 or dx + w > cw or dy + h > ch:
            errors.append(f"{name}: dst {dx},{dy} ({w}x{h}) falls outside the {cw}x{ch} canvas")

    names = sorted(rects)
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            # both sticks intentionally share one pair of cap sprites
            if a.startswith("stick_") and b.startswith("stick_") and a.split("_", 2)[2] == b.split("_", 2)[2]:
                continue
            if bleeds(sheet, rects[a], rects[b]):
                errors.append(f"{a} and {b} share inked pixels: {rects[a]} / {rects[b]}")

    return errors


def main():
    failed = False
    for pad_dir in sorted(p for p in CONTROLLERS.iterdir() if (p / "layout.json").exists()):
        errors = check(pad_dir)
        if errors:
            failed = True
            print(f"FAIL {pad_dir.name}")
            for e in errors:
                print(f"  - {e}")
        else:
            print(f"ok   {pad_dir.name}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
