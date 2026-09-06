# QindaQt

> **Pronounced “kinda cute”** (`KYNE-duh kyoot`, /ˈkaɪndə kjuːt/).

QindaQt is a Qt desktop environment where ordinary application windows can
share a window container. Hold **Meta+Shift** while dragging to combine windows,
arrange them in tiles or tabs, and move the container as one window. Use the
container controls to rearrange its members or return a window to the desktop.

The desktop includes a panel and dock, application menus, notifications,
Settings, and a growing set of first-party applications. Settings has dedicated
pages for displays, audio, Bluetooth, networking, power, clipboard, appearance,
and color. Desktop profiles change the panel and window-control layout;
QindaPunk-inspired themes bring the wallpaper's ink-blue and amber colors into
the interface.

Start with the [handbook](docs/wiki/handbook/index.md) for everyday use. The
[project wiki](docs/wiki/index.md) also covers architecture, configuration,
building, and testing.

## Development status

QindaQt is under active development. The compositor, window containers, shell,
Settings, and platform services are implemented, but the desktop still has
interaction and hardware issues being worked through. A passing unit test does
not mean a feature has passed its real-session checks.

The [task list](docs/TASK_LIST.md) records the current desktop-completion work.
The [integration handoff](docs/HANDOFF.md) distinguishes reviewed source,
verified runtime behavior, and changes installed in the live session.

## Build

The `dev` and `release` presets build both the binary KWin integration and
the production LayerShellQt shell. The dependency contract is strict on
purpose:

| Dependency | CMake requirement | Qualified Manjaro package set |
| --- | --- | --- |
| Qt | 6.11 or newer, including Core, DBus, Gui, QML, Quick, Quick Controls, SVG, Test, and Widgets | `qt6-base 6.11.1-1`, `qt6-declarative 6.11.1-3`, `qt6-svg 6.11.1-1`, `qt6-wayland 6.11.1-1` |
| Extra CMake Modules | 6.0 or newer | `extra-cmake-modules 6.27.0-1` |
| KWin | **6.6.6 exactly** | `kwin 6.6.6` (Gentoo `kde-plasma/kwin-6.6.6`) |
| Plasma Activities | **6.6.6 exactly** | `plasma-activities 6.6.6` |
| KDecoration3 | 6.6 or newer | `kdecoration 6.6.6` |
| LayerShellQt | 6.6.6 or newer | `layer-shell-qt 6.6.6` |
| KF6 CoreAddons and GlobalAccel | 6.0 or newer | `kcoreaddons 6.27.0-1`, `kglobalaccel 6.27.0-1` |
| XDG desktop portal runtime | 1.20 or newer; QindaQt supplies only Settings | `xdg-desktop-portal` plus at least one of `xdg-desktop-portal-kde`, `xdg-desktop-portal-gtk`, or `xdg-desktop-portal-lxqt` for explicitly routed fallback families |
| fontconfig | 2.x development headers, used only by the font discovery provider ([ADR-0067](docs/wiki/adr/0067-confine-fontconfig-behind-font-discovery.md)) | `fontconfig 2.17.1-1` |

You'll also need CMake 3.25 or newer, Ninja, Python 3 for the tests, and a
C++20 compiler. KWin and Plasma Activities are pinned to exact versions
because QindaQt ships a native KWin plugin — a newer patch or minor release
is not assumed to be binary compatible.

On an Arch-derived system:

```sh
sudo pacman -S --needed \
  base-devel cmake ninja python extra-cmake-modules \
  qt6-base qt6-declarative qt6-svg qt6-wayland wayland-protocols \
  kcoreaddons kglobalaccel kdecoration kwin plasma-activities \
  layer-shell-qt dbus xdg-desktop-portal xdg-desktop-portal-kde \
  fontconfig xorg-xdpyinfo xorg-xwayland
```

Rolling repositories may already have moved past KWin 6.6.6. If so, the
default presets need a coherent 6.6.6 package snapshot or cache — don't
force CMake past its exact ABI check. You can still build and test the
production panel client on a current rolling stack without the native
plugin, using an explicit bridge-only configuration:

```sh
cmake -S . -B build/shell -G Ninja \
  -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
cmake --build build/shell
ctest --test-dir build/shell \
  -R '^shell\.production-surface\.(1080p|wuxga|1440p)$' \
  --output-on-failure --no-tests=error
```

That bridge-only matrix proves QindaQt's public layer-shell client behavior
against the installed KWin runtime. It does not qualify the native KWin
plugin ABI — the default presets and the complete compositor matrix remain
the authority for that boundary.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the preview with:

```sh
./build/dev/src/shell/qindaqt-shell-preview --profile qindaqt --theme qinda-dark
```

Run the production shell inside a QindaQt/KWin Wayland session with:

```sh
./build/dev/src/shell/qindaqt-shell --profile qindaqt --theme qinda-dark
```

Render a deterministic headless preview for visual inspection with:

```sh
./build/dev/src/shell/qindaqt-shell-preview \
  --profile qindaqt --theme qinda-dark --width 1920 --height 1080 \
  --screenshot build/previews/qindaqt-dark-1080p.png
```

List and validate nested display scenarios with:

```sh
./tools/qindaqt-dev-session --list-scenarios
./tools/qindaqt-dev-session --scenario single-1080p --backend preview --dry-run
./tools/qindaqt-dev-session --scenario single-1080p --backend preview --smoke-test --execute
```

Architecture and maintenance documentation lives in [`docs/wiki`](docs/wiki/).
Repository-specific agent rules live in [`AGENTS.md`](AGENTS.md).
