# Third-Party Assets

None. All controller art in `controllers/` is original to this project and
covered by the repository's MIT licence.

## History

Earlier revisions bundled Xbox SVGZ assets from the Dracula gamepad-viewer
project (MIT, Copyright (c) 2021 Dracula Theme). Those were replaced in 0.2.0 by
the hand-authored layered art used today, and removed from the tree. If you are
looking at a tag before 0.2.0, that attribution still applies:

- https://github.com/dracula/gamepad-viewer/tree/c59a7a5c65f5ed45982b7faaa093353505f223f4/xbox

## Build-time tooling

Not redistributed with the plugin, but required to build the assets:

- `rsvg-convert` (librsvg) — rasterises the controller sheets.
- Pillow — used by `tools/check_layout.py` and `tools/preview.py` only.
