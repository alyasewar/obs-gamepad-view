#!/usr/bin/env python3
"""Composite a controller preview the same way the plugin renderer does.

This is a spec check, not a shipping path: it performs the exact draw sequence
src/renderer.cpp performs (base sheet, then pressed sub-rects at their dst, then
stick caps translated by their axes), so the layout.json rects can be verified
by eye before any of it reaches OBS.

  python3 tools/preview.py xbox --press face_down,bumper_l --lx 0.6 --ly -0.4
"""

import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent


def rasterise(svg: pathlib.Path, scale: float) -> Image.Image:
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        out = pathlib.Path(tmp.name)
    subprocess.run(
        ["rsvg-convert", "-z", str(scale), str(svg), "-o", str(out)],
        check=True,
    )
    img = Image.open(out).convert("RGBA")
    out.unlink()
    return img


def scaled(rect, s):
    return tuple(int(round(v * s)) for v in rect)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pad")
    ap.add_argument("--press", default="", help="comma-separated part names")
    ap.add_argument("--lx", type=float, default=0.0)
    ap.add_argument("--ly", type=float, default=0.0)
    ap.add_argument("--rx", type=float, default=0.0)
    ap.add_argument("--ry", type=float, default=0.0)
    ap.add_argument("--l3", action="store_true")
    ap.add_argument("--r3", action="store_true")
    ap.add_argument("--scale", type=float, default=0.8)
    ap.add_argument("--bg", default="#0e1014")
    ap.add_argument("-o", "--out", default=None)
    args = ap.parse_args()

    pad_dir = ROOT / "controllers" / args.pad
    layout = json.loads((pad_dir / "layout.json").read_text())
    s = args.scale

    base = rasterise(pad_dir / "base.svg", s)
    sheet = rasterise(pad_dir / "overlay.svg", s)

    canvas = Image.new("RGBA", base.size, args.bg)
    canvas.alpha_composite(base)

    pressed = {p.strip() for p in args.press.split(",") if p.strip()}
    unknown = pressed - set(layout["parts"])
    if unknown:
        sys.exit(f"unknown part(s): {', '.join(sorted(unknown))}")

    for name in pressed:
        part = layout["parts"][name]
        sx, sy, sw, sh = scaled(part["src"], s)
        dx, dy = scaled(part.get("dst", part["src"][:2]), s)
        canvas.alpha_composite(sheet.crop((sx, sy, sx + sw, sy + sh)), (dx, dy))

    travel = layout["stickTravel"]
    axes = {"left": (args.lx, args.ly, args.l3), "right": (args.rx, args.ry, args.r3)}
    for side, spec in layout["sticks"].items():
        ax, ay, click = axes[side]
        cell = spec["pressed"] if click else spec["neutral"]
        sx, sy, sw, sh = scaled(cell, s)
        cx, cy = spec["centre"]
        # y axis is up-positive on the pad, down-positive on the canvas
        px = (cx + ax * travel) * s - sw / 2.0
        py = (cy - ay * travel) * s - sh / 2.0
        canvas.alpha_composite(
            sheet.crop((sx, sy, sx + sw, sy + sh)),
            (int(round(px)), int(round(py))),
        )

    out = pathlib.Path(args.out) if args.out else ROOT / "build/preview" / f"{args.pad}-composite.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(out)
    print(out)


if __name__ == "__main__":
    main()
