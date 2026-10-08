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

Enemy click behaviour is controlled by this setting in `open-midgard.ini` beside
the executable (or in `OPEN_MIDGARD_DATA_DIR` when set):

```ini
[Gameplay]
AutoAttack=1
```

`AutoAttack=1` is the default: one click targets an enemy and attacks continuously.
Set `AutoAttack=0` for one attack per click. If the enemy is out of range, the
client moves closer before attacking once. Holding the left mouse button on an
enemy for 200 ms enables continuous attacks regardless of `AutoAttack`; with `AutoAttack=0`,
releasing it stops continuous attacks. Short clicks remain single attacks.
Restart the client after changing the setting.

All music and sound can be disabled in the same `open-midgard.ini` file:

```ini
[Audio]
Enabled=0
```

`Enabled=1` is the default. Setting it to `0` skips audio device initialization
and disables music, UI sounds, gameplay sound effects, and ambient loops,
including rain. This master switch takes precedence over the in-game audio
controls. Restart the client after changing it.

White damage numbers can be tuned in the same `open-midgard.ini` file:

```ini
[WhiteDamageNumbers]
RightDisplacement=16
MaxTravelHeight=48
MaxTravelWidth=32
DelayBetweenNumbersMs=160
```

| Setting | Meaning |
| --- | --- |
| `RightDisplacement` | Initial offset of the number's center to the right of the damaged actor, in screen pixels. |
| `MaxTravelHeight` | Maximum rise above the starting position, in screen pixels. |
| `MaxTravelWidth` | Additional travel to the right from the starting position, in screen pixels. |
| `DelayBetweenNumbersMs` | Interval between multi-hit skill numbers and minimum spacing between white numbers on the same actor, in milliseconds. |

White numbers follow an arc upward and back toward their starting height while
moving right, independently of camera rotation and attacker direction. Distances
accept `0` to `4096`; the delay accepts `0` to `10000`. Zero disables the
corresponding displacement, travel, or spacing. Attack numbers still wait for
their scheduled impact. Restart the client after editing these settings.

Yellow damage totals have their own settings in the same file:

```ini
[YellowDamageNumbers]
DelayAfterLastHitMs=320
StartingHeightDisplacement=0
MaxTravelHeight=48
```

| Setting | Meaning |
| --- | --- |
| `DelayAfterLastHitMs` | Delay after the packet's last displayed hit before the yellow total begins fading in, in milliseconds. |
| `StartingHeightDisplacement` | Initial height above the target's position, in screen pixels. Zero starts on the target. |
| `MaxTravelHeight` | Additional upward travel from the starting position, in screen pixels. |

The total stays horizontally centered on the target, fades in over 160 ms,
and rises while fading out during the last 720 ms of its 2880 ms animation.
Distances accept `0` to `4096`; the delay accepts `0` to `10000`. Zero delay
starts the fade-in with the final hit, and zero travel keeps the starting height.
Delayed totals capture the target's position when they appear; if the target
disappears first, they use its last position. Restart after changing settings.
These options require an executable containing the yellow-number settings;
editing the INI alone does not add them to an older executable.

Procedural rain and particle mist can be enabled for outdoor gameplay maps in the same INI file:

```ini
[Weather]
Rain=1
```

Set `Rain=0` (the default) to disable rain, mist and rain audio. Restart the client
after editing the setting, or change maps to reload it. This proof of concept generates translucent
rain streaks and a texture for soft, irregular mist particles entirely in code.
The blue mist drifts in varied directions, swirls and circulates across the screen,
and gently turns, changes size and fades beneath the rain and UI.
Each map retains its normal fog settings. Rain and mist automatically stop in
interiors and dungeons and resume when returning outdoors. Map detection uses
the client's `data/indoorrswtable.txt` plus standard interior, dungeon and
instance map naming rules. Custom maps with other names need classification
before they can be excluded. Both weather effects use screen coordinates;
they have no roof collision or ground splashes.

Add a WAV file at `data/wav/rain.wav` to play a continuous rain sound loop under
the same `Rain` toggle. The loop follows the sound effects volume and mute
settings. It starts after gameplay is presented, remains silent during loading
screens and stops on map exit, including transitions to interiors or dungeons.
A missing file leaves the rain visuals enabled without audio. Use a WAV prepared
for seamless looping; the client repeats the entire file.

A night filter with glowing fireflies can be enabled independently in the same INI file:

```ini
[Weather]
Night=1
```

Set `Night=0` (the default) to disable it. Restart or change maps after editing.
This proof of concept darkens outdoor gameplay maps with a
cool blue tint that also affects rain and mist. The lower screen stays slightly
brighter, and the UI retains its normal brightness. It can be used with `Rain=1`.
The same toggle adds softly glowing yellow-green fireflies with varied drift,
flutter and blinking rhythms. Their generated glow draws after the night tint
and beneath the UI. Night and fireflies use the same map detection as rain,
automatically stopping in interiors and dungeons and resuming outdoors.
These proof-of-concept particles use screen coordinates.

Procedural fire and water replacements share one proof-of-concept toggle:

```ini
[Effects]
ModernFireWater=1
```

Set `ModernFireWater=0` (the default) to use the classic rendering. Restart or
change maps after editing. This applies to every map torch emitter (RSW type 47)
and all ground water surfaces, including the torches and pools in `pay_dun01`.
Fire gains layered, flickering flame tongues, soft glow and rising embers. Emitters
at a supporting prop's foot are raised to its rendered top, accounting for the
prop's scale and rotation. Water gains larger rolling waves, bright blue color
bands, two flowing texture layers, glancing-angle highlights and thick pale
crests near shallow edges. All textures are generated in code. Effects retain
world depth testing and map water levels; this does not add
scene reflections or dynamic lighting, or replace skill animations.

## Project Goals

- Recreate the Ragnarok Online client as open source code
- Preserve the feel and behavior of the classic pre-Renewal client
- Modernize the renderer and platform layer where it makes the project easier to build, debug, and maintain
- Keep packet behavior aligned to the chosen 2008-era target profile

## License

See `LICENSE` for the project license.
