# Qt Build And Compliance

## Build Inputs

The Qt spike is guarded by `RO_ENABLE_QT6_UI_SPIKE`.

When enabled, CMake now:

- looks for a compiler-compatible Qt 6 desktop kit under `RO_QT_ROOT`, `RO_QT_ROOT` in the environment, `QTDIR`, or `C:\Qt`;
- requires a Qt kit that matches the active compiler toolchain;
- requires `windeployqt.exe` from the same Qt kit so deployment uses matching plugins and QML imports.

For the default Visual Studio x64 build, the expected Qt layout is a kit root such as:

- `C:/Qt/6.11.0/msvc2022_64`

The helper script is:

- `tools/build_qt_ui_spike.ps1`

The MinGW preset is:

- `cmake --preset mingw-qt-x64`
- `cmake --build --preset build-mingw-qt-x64`

It configures with:

- `RO_ENABLE_QT6_UI_SPIKE=ON`
- `RO_QT_ROOT=<detected-or-explicit-kit>`

## Open-Source Compliance Scaffolding

This repository now includes a Qt notice bundle under:

- `third_party/qt`

That bundle is copied into built outputs as:

- `licenses/qt`

The bundle includes:

- a prominent Qt LGPL notice;
- the LGPL 3.0 and GPL 3.0 license texts;
- installation information explaining that the application dynamically links Qt DLLs and that compatible replacements can be used;
- source-offer guidance describing what must accompany a distributed build.

## Deployment

When `RO_ENABLE_QT6_UI_SPIKE=ON`, CMake runs `windeployqt` after build:

- once for the normal build output directory;
- once for `RO_DEV_DEPLOY_DIR` (default `C:/ragnarok/client`) when `RO_ENABLE_DEV_DEPLOY=ON`.

This deployment also copies `third_party/qt` into `licenses/qt` in the target output directory.

To keep the output root smaller, the deploy script now writes `qt.conf` beside `Runerok.exe` and relocates Qt-owned directory trees under:

- `thirdparty/qt`

That subtree contains the plugin, QML, and translation directories that Qt can resolve through `qt.conf`. Startup-critical DLLs remain beside the executable because they still need to be found by the Windows loader before Qt code runs.

## Important Limitation

This is technical compliance scaffolding, not legal advice.

If you distribute Qt under LGPL, you still need to ensure your release process provides the exact corresponding Qt source or a valid written offer and that your distribution terms do not restrict LGPL rights. The files in `third_party/qt` are intended to make that packaging work visible and repeatable, not to replace legal review.
