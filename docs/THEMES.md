# Themes and controller art

Two things combine to produce what you see on screen:

- **Controller art** in `controllers/<pad>/` — the geometry. One directory per
  controller type (`xbox`, `ps5`).
- **Themes** in `themes/<theme>/` — the palette and timings. One `theme.json`
  each, no art of its own.

The build step (`cmake/build-assets.py`) renders every controller once per
theme, so a theme swap at runtime is just loading a different pair of PNGs.
Nothing is tinted per frame.

## Controller art

```
controllers/
  xbox/
    base.svg       rest state: shell, wells, unlit buttons
    overlay.svg    pressed state, plus a spare strip of loose sprites
    layout.json    which rect on the sheet is which input
  ps5/
    ...
```

Both SVGs share a 1000x680 authoring canvas. `overlay.svg` is taller (1000x1000)
because rows below 680 are a **spare strip**: sprites that either collide in
place (the bumper and trigger arcs overlap) or that the renderer positions
itself (stick caps). Rows 0..680 are pixel-aligned with `base.svg`, so an
in-place sprite's source rect is also its destination.

### layout.json

```jsonc
{
  "canvas": { "w": 1000, "h": 680 },   // the pad's own coordinate space
  "sheet":  { "w": 1000, "h": 1000 },  // overlay.svg's canvas
  "stickTravel": 22,                   // how far a cap slides at full deflection
  "parts": {
    "face_down": { "src": [668, 314, 64, 64] },          // in place: dst == src
    "bumper_l":  { "src": [340, 700, 208, 86],           // parked in the strip
                   "dst": [186, 144] }                   // ...drawn here
  },
  "sticks": {
    "left": { "centre": [336, 272],
              "neutral": [40, 700, 116, 116],
              "pressed": [180, 700, 116, 116] }
  }
}
```

Parts are named by **function**, not by console: `face_down` is A on an Xbox pad
and Cross on a DualSense, so the renderer has one code path for both. The full
list is the `IvPart` enum in `src/input_state.h`.

### Palette hooks

Every SVG carries a `<style id="iv-palette">` block. The build step replaces its
whole contents with the active theme's colours, so **author against the class
names, never against literal hex values**:

| Class      | Stroke form  | Theme key   | Used for                        |
|------------|--------------|-------------|---------------------------------|
| `shell`    | `s-shell`    | `shell`     | Controller body                 |
| `shell-hi` | `s-shell-hi` | `shellHi`   | Top sheen, touchpad face        |
| `shell-lo` | `s-shell-lo` | `shellLo`   | Triggers at rest, shadowed edge |
| `recess`   | `s-recess`   | `recess`    | Stick wells                     |
| `cap`      | `s-cap`      | `cap`       | Button and stick cap faces      |
| `cap-lo`   | `s-cap-lo`   | `capLo`     | Cap shadows, bumpers at rest    |
| `ink`      | `s-ink`      | `ink`       | Labels and glyphs               |
| `hot`      | `s-hot`      | `accent`    | Pressed state                   |
| `hot-hi`   | `s-hot-hi`   | `accentHi`  | Pressed highlight, triggers     |
| `on-hot`   | `s-on-hot`   | `onAccent`  | Glyphs sitting on the accent    |

### Checking your work

After editing art or rects:

```
python3 tools/check_layout.py
```

This rasterises each `overlay.svg` and confirms every rect is inside the sheet,
actually contains ink, lands inside the canvas, and does not share inked pixels
with a neighbour. The two SVGs per pad duplicate their geometry (rsvg will not
follow cross-file references), so this is what catches a coordinate changed in
one file and not the other.

To see a pad composited the way the renderer draws it:

```
python3 tools/preview.py xbox --press face_down,bumper_l --lx 0.6 --ly -0.4 --r3
```

## theme.json

```jsonc
{
  "id": "dark",
  "name": "Dark",
  "palette": {
    "shell": "#2b303b", "shellHi": "#39404e", "shellLo": "#20242d",
    "recess": "#171a21", "cap": "#5b6478", "capLo": "#434b5c",
    "ink": "#cdd3e0", "accent": "#4e8cff", "accentHi": "#8fb6ff",
    "onAccent": "#0b1020",
    "background": "#14161bcc"      // only drawn if "Draw backdrop" is on
  },
  "animation": {
    "pressFadeMs": 60,             // button reaching full press
    "releaseFadeMs": 140,          // and returning to rest
    "connectFadeMs": 220,          // whole pad fading in on hot-plug
    "disconnectFadeMs": 320
  }
}
```

Colours accept `#rgb`, `#rrggbb` and `#rrggbbaa`. An unparseable value falls
back to the built-in default rather than blanking the source.

Press timings are deliberately asymmetric: a press should register immediately,
while a slower release keeps a quick tap legible on stream.

## Adding a theme

1. Copy `themes/dark/` to `themes/my-theme/` and edit the palette.
2. Rebuild (`cmake --build build`), or run the asset step directly:
   `python3 cmake/build-assets.py --source . --out build/data`
3. Add the id to the theme dropdown in `input_visualizer_properties`
   (`src/plugin.cpp`) and to `data/locale/en-US.ini`.

## Adding a controller

1. Copy `controllers/xbox/` to `controllers/my-pad/`.
2. Redraw `base.svg` and `overlay.svg` on the same 1000x680 canvas, keeping the
   `<style id="iv-palette">` block.
3. Update the rects in `layout.json`, then run `tools/check_layout.py`.
4. Add the id to the layout dropdown and to `iv_resolve_pad` in
   `src/plugin.cpp` if it should participate in auto-detection.
