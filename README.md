# OpenMidgard

OpenMidgard is an open source recreation of the Ragnarok Online client.

The current direction of the project is intentionally biased toward the pre-Renewal 2008-era client experience: classic presentation, classic game flow, and packet behavior aligned to that period rather than later Renewal-era client behavior.

This repository is focused on rebuilding the client as source code, modernizing the renderer and platform layer where needed, and keeping the gameplay/client feel anchored to the classic 2008 style.

## Status

- Open source Ragnarok Online client recreation in active development.
- Current work includes the world renderer, UI systems, networking, effects, and modern rendering backends.
- The project currently prefers the 2008 pre-Renewal client style and behavior.

## Packet Version Target

The current packet target is:

- `packet_ver 23`
- `2008-09-10aSakexe`

More specifically, this is the intended target for client-originating map-server packets.

## Progress Video

Current progress showcase:

- https://www.youtube.com/watch?v=ApS1wgUmgzE

## Repository Layout

- `src/` — client source code
- `cmake/` — CMake helpers and deployment scripts
- `docs/` — project notes and focused implementation docs
- `third_party/` — third-party notices and bundled support files

## Prerequisites

At a minimum you will need:

- CMake 3.20 or newer
- A C++17 compiler
- Git
- A legally obtained Ragnarok Online data/runtime setup for testing

For runtime asset discovery, the client looks for things like `data.grf`, `data/clientinfo.xml`, and `data/` near the executable or current working directory. You can also point it at a custom runtime root with:

- `OPEN_MIDGARD_DATA_DIR=/path/to/runtime`

Resources use this priority, highest first: loose files in `data/`, then
`patchRunerok.grf`, then `data.grf`. Only these two GRFs are loaded; `data.ini`
is not used. A missing file falls through to the next source.

The CMake option `RO_READ_DATA_FOLDER` defaults to `ON`. Configure with
`-DRO_READ_DATA_FOLDER=OFF` and rebuild to ignore loose files in `data/`, including
sprites, palettes, client-info XML, and Lua scripts. Set it back to `ON` to enable
the overrides. Other runtime disk paths, such as `BGM/` and `System/`, remain
available.

## Building On Windows

Windows currently has the most complete build flow.

### Option 1: Visual Studio 2022

Available presets:

- `vs2022-win32`
- `vs2022-x64`

Configure and build:

```powershell
cmake --preset vs2022-win32
cmake --build --preset build-release --config Release
```

Or for x64:

```powershell
cmake --preset vs2022-x64
cmake --build --preset build-release-x64 --config Release
```

Notes:

- Native Direct3D 11 and Direct3D 12 backends are only enabled on Windows MSVC builds.
- Windows builds copy `Runerok.exe` to `C:/ragnarok/client` by default. Override the folder with `-DRO_DEV_DEPLOY_DIR=<path>` or disable copying with `-DRO_ENABLE_DEV_DEPLOY=OFF`.

### Option 2: MinGW + Qt 6

The repository includes a ready-made preset for the current Qt-based runtime path:

- `mingw-qt-x64`

Configure and build:

```powershell
cmake --preset mingw-qt-x64
cmake --build --preset build-mingw-qt-x64
```

Debug build:

```powershell
cmake --preset mingw-qt-x64-debug
cmake --build --preset build-mingw-qt-x64-debug
```

Expected environment:

- Qt 6 desktop kit compatible with the chosen compiler
- For the included preset, the project expects a Qt/MinGW layout similar to:
  - `C:/Qt/6.11.0/mingw_64`
  - `C:/Qt/Tools/mingw1310_64`

Notes:

- On Windows, the Qt-enabled MinGW build is one of the main day-to-day workflows in this repo.
- If Qt is installed elsewhere, set `RO_QT_ROOT` to the matching kit root.
- Qt deployment is handled automatically for Windows Qt builds.

## Building On Linux

Linux has an included preset for the non-Windows Qt/Vulkan path:

- `linux-qt-vulkan`

Configure and build:

```bash
cmake --preset linux-qt-vulkan
cmake --build --preset build-linux-qt-vulkan -j
```

What this means in practice:

- Non-Windows builds force the Qt 6 UI path on.
- Native Direct3D 11 and Direct3D 12 backends are disabled automatically.
- The Linux path is Vulkan-oriented.

Recommended dependencies:

- `cmake`
- `ninja`
- `g++` or `clang++` with C++17 support
- Qt 6 development packages, including QML/Quick
- Vulkan development packages / SDK

Practical note:

- On Ubuntu-like systems, the Qt runtime may also need the `QtQml.WorkerScript` module package installed separately for the QML UI to start correctly.

## Building On macOS

macOS does not currently have a checked-in preset, but the project does have a non-Windows CMake path you can configure manually.

Recommended tools:

- Xcode Command Line Tools or Apple Clang with C++17 support
- `cmake`
- `ninja`
- Qt 6

Example configure and build:

```bash
cmake -S . -B build-macos \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRO_ENABLE_DEV_DEPLOY=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos

cmake --build build-macos -j
```

Notes:

- On non-Windows platforms, the project enables the Qt 6 UI path automatically.
- Native Direct3D backends are disabled automatically.
- The macOS path is less exercised than the Windows build flow, so expect some platform-specific integration work.

## Running The Client

After building, run:

- Windows: `Runerok.exe` (deployed to `C:\ragnarok\client\Runerok.exe` by default)
- Linux/macOS: `open-midgard`

Make sure your runtime directory contains the client data/GRF files and configuration files the client expects, or set `OPEN_MIDGARD_DATA_DIR` to point at them.

## Project Goals

- Recreate the Ragnarok Online client as open source code
- Preserve the feel and behavior of the classic pre-Renewal client
- Modernize the renderer and platform layer where it makes the project easier to build, debug, and maintain
- Keep packet behavior aligned to the chosen 2008-era target profile

## License

See `LICENSE` for the project license.
