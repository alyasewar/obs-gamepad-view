# Building

## Requirements

- CMake 3.21+
- C++17 compiler (Xcode command line tools on macOS)
- OBS Studio 30 or newer, with development headers and libraries
  (the plugin uses the `gs_image_file_ex_*` API, which replaced the older
  `gs_image_file_*` one)
- Python 3
- `rsvg-convert` — `brew install librsvg` (macOS) or `apt install librsvg2-bin`
- Pillow, for the asset checks only — `pip install Pillow`

## Configure and build

```
cmake -S . -B build \
  -DOBS_INCLUDE_DIR=/path/to/obs/include \
  -DOBS_LIB_DIR=/path/to/obs/lib

cmake --build build
```

The build rasterises the controller sheets into `build/data/` as part of the
default target, so the plugin binary and its assets stay in step.

## Output

- macOS/Linux: `build/input_visualizer.so`
- Windows: `build/input_visualizer.dll`
- Assets: `build/data/`

## Asset scale

`IV_ASSET_SCALE` controls how far above the 1000px authoring canvas the sheets
are rendered. The default of 2 keeps the pad crisp when a scene scales the
source up. Drop it to halve the texture memory held for the active theme:

```
cmake -S . -B build -DIV_ASSET_SCALE=1 ...
```

The renderer reads the real texture size at load and derives the factor itself,
so changing this needs no code edit.

## Rebuilding assets alone

```
python3 cmake/build-assets.py --source . --out build/data --scale 2
```

## Checks

```
cmake --build build --target check_layout   # rects vs. rendered sheets
ctest --test-dir build                      # JSON reader and layout loader
```

`check_layout` and `layout_selftest` have no OBS dependency, so both can run
without an OBS SDK present:

```
python3 tools/check_layout.py
clang++ -std=c++17 -I src tools/layout_selftest.cpp src/json.cpp src/layout.cpp \
  -o build/layout_selftest && ./build/layout_selftest .
```

## Installing the build (macOS)

OBS only loads `.plugin` bundles on macOS, so the linked binary has to be
packaged before OBS will see it:

```
./cmake/package-macos.sh build/input_visualizer.so build/data
```

That writes `input-visualizer.plugin` into
`~/Library/Application Support/obs-studio/plugins/`. Pass a third argument to
install somewhere else. Restart OBS, then confirm it loaded via
**Help -> Log Files -> View Current Log** — you should see `input-visualizer`
under `Loaded Modules`.

## Building without an OBS SDK installed

The headers OBS ships in its app bundle are not enough to compile against. To
build against the exact version you run, fetch matching headers:

```
git clone --depth 1 --filter=blob:none --sparse \
  https://github.com/obsproject/obs-studio.git /tmp/obs-headers
cd /tmp/obs-headers && git sparse-checkout set libobs
git fetch --depth 1 origin tag 32.2.2 && git checkout 32.2.2
```

libobs also needs simde and a generated `obsconfig.h`. Note that the image-file
API was renamed on master (`gs_image_file_ex_*`); this plugin targets the
`gs_image_file4_*` names that OBS 32.x exports, so build against a 32.x tag
rather than master.
