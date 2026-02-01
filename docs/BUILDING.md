# Building

This project uses CMake.

## Requirements

- CMake 3.21+
- C++17 compiler
- OBS Studio development headers and libraries

## Configure

Set the OBS include and library paths with CMake options:

```
cmake -S . -B build \
  -DOBS_INCLUDE_DIR=/path/to/obs/include \
  -DOBS_LIB_DIR=/path/to/obs/lib
```

## Build

```
cmake --build build
```

## Output

- macOS/Linux: `build/input_visualizer.so`
- Windows: `build/input_visualizer.dll`
