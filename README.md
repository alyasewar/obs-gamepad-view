# OBS Input Visualizer

Real-time gamepad, keyboard, and mouse input overlay for OBS Studio.


## Screenshots

Add images to these folders and update the links:

- `assets/screenshots/`
- `assets/gifs/`

## Install

This is a native OBS plugin. You install it like any other plugin:

### Windows

1. Download the latest release ZIP.
2. Extract it into:
   - `C:\Program Files\obs-studio\` (or your OBS install folder)
3. Start OBS and add the source: `+` -> `Input Visualizer`.

### macOS

1. Download the latest release ZIP.
2. Copy `InputVisualizer.plugin` to:
   - `/Library/Application Support/obs-studio/plugins/`
3. Start OBS and add the source: `+` -> `Input Visualizer`.

### Linux

1. Download the latest release tar.gz.
2. Copy `input_visualizer.so` to:
   - `/usr/lib/obs-plugins/`
3. Copy the data folder to:
   - `/usr/share/obs/obs-plugins/input-visualizer/`
4. Start OBS and add the source: `+` -> `Input Visualizer`.

## Usage

1. Add a new source: `+` -> `Input Visualizer`.
2. Choose a device (gamepad, keyboard, mouse).
3. Pick a theme.
4. Adjust size, opacity, and position in the scene.

## Theme Packs

Themes live in `themes/` and each theme has a `theme.json` manifest and assets.

Example structure:

```
themes/
  light/
    theme.json
    assets/
  dark/
    theme.json
    assets/
  pastel/
    theme.json
    assets/
```

## Build

Prerequisites:

- CMake 3.21+
- OBS Studio development files
- C++17 compiler
- Xcode (for MacOS)

Build (macOS example):

```
cmake -S . -B build
cmake --build build
```

## SVGZ Conversion

The build step converts `.svgz` assets to `.png` for reliable rendering.

Dependencies:

- `rsvg-convert` (recommended), or
- macOS `qlmanage`

## License

MIT

## Third-Party Assets

See `docs/THIRD-PARTY.md` for license details.
