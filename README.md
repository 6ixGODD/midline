# Midline

A screen center line overlay for Windows.

## Features

- Transparent, click-through overlay
- Always on top
- Adjustable line thickness and opacity via hotkeys

## Hotkeys

| Shortcut | Action                      |
|----------|-----------------------------|
| `Ctrl` + Scroll | Adjust thickness (1-400 px) |
| `Alt` + Scroll | Adjust opacity              |

## Build

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"  # or "Visual Studio 17 2022"
cmake --build . --config Release
```

## Requirements

- Windows 7+
- CMake 3.10+
- MinGW-w64 or MSVC
