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
3. Copy `input_visualizer.so` into:
   - `/Library/Application Support/obs-studio/plugins/input-visualizer/`
4. Reopen OBS.
5. Add the source: `+` -> `Input Visualizer`.

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
- Themes are stored in the `themes/` directory inside the plugin data folder.
- If you update themes, rebuild to regenerate `.png` assets from `.svgz`.
