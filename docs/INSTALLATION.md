# Installation

This plugin installs like any other native OBS source.

## Windows

1. Download the latest release ZIP from GitHub.
2. Close OBS.
3. Extract the ZIP into your OBS install folder:
   - `C:\Program Files\obs-studio\`
4. Reopen OBS.
5. Add the source: `+` -> `Input Visualizer`.

## macOS

1. Download the latest release ZIP from GitHub.
2. Close OBS.
3. Copy `input-visualizer.plugin` into one of:
   - `~/Library/Application Support/obs-studio/plugins/` (just you)
   - `/Library/Application Support/obs-studio/plugins/` (all users)
4. Reopen OBS.
5. Add the source: `+` -> `Input Visualizer`.

OBS will not load a bare `.so` on macOS; it has to be the `.plugin` bundle, with
the binary at `Contents/MacOS/input-visualizer`. Building from source produces
the bundle via `cmake/package-macos.sh`.

## Linux

1. Download the latest release tar.gz from GitHub.
2. Close OBS.
3. Copy the plugin binary:
   - `input_visualizer.so` -> `/usr/lib/obs-plugins/`
4. Copy the data folder:
   - `data/` -> `/usr/share/obs/obs-plugins/input-visualizer/`
5. Reopen OBS.
6. Add the source: `+` -> `Input Visualizer`.

## Notes

- If the source does not appear, check `Help -> Log Files -> View Current Log` in OBS.
- Controller sheets live in `themes/<theme>/<pad>/` inside the plugin data
  folder, with the geometry manifests in `controllers/<pad>/`.
- If you edit themes or controller art, rebuild to re-render those sheets. See
  `docs/THEMES.md`.
- The keyboard and mouse overlay needs Accessibility permission on macOS; the
  gamepad overlay does not.
