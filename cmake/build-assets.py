#!/usr/bin/env python3
"""Rasterise the controller sheets once per theme into the plugin data tree.

Each controller is authored as two SVGs carrying a <style id="iv-palette">
block of placeholder colours. This substitutes a theme's palette into that
block and rasterises, so a theme swap costs nothing at runtime -- the plugin
just loads a different pair of PNGs instead of tinting per draw.

  python3 cmake/build-assets.py --source . --out build/data [--scale 2]
"""

import argparse
import json
import pathlib
import re
import shutil
import subprocess
import sys

# SVG class name -> theme.json palette key.
PALETTE_CLASSES = {
    "shell": "shell",
    "shell-hi": "shellHi",
    "shell-lo": "shellLo",
    "recess": "recess",
    "cap": "cap",
    "cap-lo": "capLo",
    "ink": "ink",
    "hot": "accent",
    "hot-hi": "accentHi",
    "on-hot": "onAccent",
}

FALLBACK = {
    "shell": "#2b303b",
    "shellHi": "#39404e",
    "shellLo": "#20242d",
    "recess": "#171a21",
    "cap": "#5b6478",
    "capLo": "#434b5c",
    "ink": "#cdd3e0",
    "accent": "#4e8cff",
    "accentHi": "#8fb6ff",
    "onAccent": "#0b1020",
}

STYLE_RE = re.compile(
    r'(<style id="iv-palette">)(.*?)(</style>)', re.DOTALL)


def style_block(palette):
    lines = []
    for css_class, key in PALETTE_CLASSES.items():
        colour = palette.get(key, FALLBACK[key])
        lines.append(f"    .{css_class}{{fill:{colour}}}")
        lines.append(f"    .s-{css_class}{{stroke:{colour};fill:none}}")
    return "\n" + "\n".join(lines) + "\n  "


def themed_svg(svg_text, palette):
    replacement, count = STYLE_RE.subn(
        lambda m: m.group(1) + style_block(palette) + m.group(3), svg_text)
    if count != 1:
        raise SystemExit(
            f"expected exactly one <style id=\"iv-palette\"> block, found {count}")
    return replacement


def rasterise(svg_text, dest, scale, tmp_dir, stem):
    tmp_svg = tmp_dir / f"{stem}.svg"
    tmp_svg.write_text(svg_text)
    dest.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["rsvg-convert", "-z", str(scale), str(tmp_svg), "-o", str(dest)],
        check=True)
    tmp_svg.unlink()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True, help="repository root")
    ap.add_argument("--out", required=True, help="plugin data directory to populate")
    ap.add_argument("--scale", type=float, default=2.0,
                    help="rasterisation factor over the 1000px authoring canvas")
    args = ap.parse_args()

    if shutil.which("rsvg-convert") is None:
        sys.exit("rsvg-convert not found. Install librsvg (brew install librsvg).")

    source = pathlib.Path(args.source).resolve()
    out = pathlib.Path(args.out).resolve()
    controllers = sorted(p for p in (source / "controllers").iterdir()
                         if (p / "layout.json").exists())
    themes = sorted(p for p in (source / "themes").iterdir()
                    if (p / "theme.json").exists())
    if not controllers:
        sys.exit("no controllers found")
    if not themes:
        sys.exit("no themes found")

    tmp_dir = out / ".tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    for pad in controllers:
        dest = out / "controllers" / pad.name / "layout.json"
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(pad / "layout.json", dest)

    built = 0
    for theme in themes:
        manifest = json.loads((theme / "theme.json").read_text())
        palette = manifest.get("palette", {})

        dest_manifest = out / "themes" / theme.name / "theme.json"
        dest_manifest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(theme / "theme.json", dest_manifest)

        for pad in controllers:
            for sheet in ("base", "overlay"):
                svg = themed_svg((pad / f"{sheet}.svg").read_text(), palette)
                rasterise(svg, out / "themes" / theme.name / pad.name / f"{sheet}.png",
                          args.scale, tmp_dir, f"{theme.name}-{pad.name}-{sheet}")
                built += 1

    # Static data (the tint effect, locale strings) ships alongside the sheets.
    static_src = source / "data"
    if static_src.is_dir():
        shutil.copytree(static_src, out, dirs_exist_ok=True)

    shutil.rmtree(tmp_dir, ignore_errors=True)
    print(f"built {built} sheets for {len(themes)} theme(s) x "
          f"{len(controllers)} controller(s) at {args.scale}x -> {out}")


if __name__ == "__main__":
    main()
