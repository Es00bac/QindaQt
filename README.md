# QindaQt

QindaQt is a modular Qt desktop environment centered on hybrid window
containers: ordinary application windows can be combined into a movable,
tabbed, recursively split container and separated again without changing the
applications themselves.

This repository currently contains the executable foundation for the desktop:

- a qualified KWin-based compositor and hybrid window-container runtime;
- a tested container-layout domain model;
- versioned desktop-profile and theme formats;
- five built-in palettes, including the mist-and-sage Qinda macOS decoration
  theme;
- a Qt Quick shell preview and a production LayerShellQt panel process with
  owner-bound compositor visibility and safe-visible recovery;
- manifest- and capability-policy-gated production applet resolution, with a
  live locale-aware clock as the first audited built-in implementation;
- transactional panel/applet editing, window-aware visibility policy, and an
  installable freedesktop notification host with an owner-bound asynchronous
  shell client and descriptor-only session authentication;
- an essential-process supervisor that starts the notification host and shell,
  couples their lifetimes, and never places its generated token in argv or env;
- isolated nested-session and resolution-scenario tooling;
- an agent-oriented, versioned project wiki.

The compositor, hybrid interaction milestone, and initial real panel surfaces
are implemented. QindaQt is still under construction as a daily-use desktop:
the remaining live applets, global menu, settings-center presentation, platform
services, packaging, and physical-hardware qualification remain roadmap work.

## Build

The `dev` and `release` presets build both the binary KWin integration and the
production LayerShellQt shell. Their dependency contract is deliberately
strict:

| Dependency | CMake requirement | Qualified Manjaro package set |
| --- | --- | --- |
| Qt | 6.11 or newer, including Core, DBus, Gui, QML, Quick, Quick Controls, Test, and Widgets | `qt6-base 6.11.1-1`, `qt6-declarative 6.11.1-3`, `qt6-wayland 6.11.1-1` |
| Extra CMake Modules | 6.0 or newer | `extra-cmake-modules 6.27.0-1` |
| KWin | **6.6.5 exactly** | `kwin 6.6.5-4` |
| Plasma Activities | **6.6.5 exactly** | `plasma-activities 6.6.5-1` |
| KDecoration3 | 6.6 or newer | `kdecoration 6.6.5-1` |
| LayerShellQt | 6.6.5 or newer | `layer-shell-qt 6.6.5-2` |
| KF6 CoreAddons and GlobalAccel | 6.0 or newer | `kcoreaddons 6.27.0-1`, `kglobalaccel 6.27.0-1` |

CMake 3.25 or newer, Ninja, Python 3 for tests, and a C++20 compiler are also
required. The KWin and Plasma Activities entries are exact because QindaQt
ships a native KWin plugin; a newer patch or minor release is not assumed to be
binary compatible.

On an Arch-derived system, the corresponding package names are:

```sh
sudo pacman -S --needed \
  base-devel cmake ninja python extra-cmake-modules \
  qt6-base qt6-declarative qt6-wayland \
  kcoreaddons kglobalaccel kdecoration kwin plasma-activities \
  layer-shell-qt dbus xorg-xdpyinfo xorg-xwayland
```

Rolling repositories may already have moved beyond KWin 6.6.5. In that case,
the default presets must use a coherent 6.6.5 package snapshot/cache; do not
force CMake past its exact ABI check. A current rolling stack can still build
and test the production panel client without the native plugin through an
explicit bridge-only configuration:

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
against the installed KWin runtime. It does not qualify the native KWin plugin
ABI; the default presets and complete compositor matrix remain the authority
for that boundary.

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
