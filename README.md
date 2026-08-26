# QindaQt

QindaQt is a modular Qt desktop environment centered on hybrid window
containers: ordinary application windows can be combined into a movable,
tabbed, recursively split container and separated again without changing the
applications themselves.

This repository currently contains the executable foundation for the desktop:

- a tested container-layout domain model;
- versioned desktop-profile and theme formats;
- five built-in palettes, including the mist-and-sage Qinda macOS decoration
  theme;
- a Qt Quick shell preview that renders built-in desktop profiles;
- isolated nested-session and resolution-scenario tooling;
- an agent-oriented, versioned project wiki.

The production compositor will be a deliberately small downstream KWin patch
set. Until that integration lands, the shell preview and pure domain libraries
provide a fast test surface for interaction and persistence work.

## Build

QindaQt currently requires Qt 6.11, CMake 3.25 or newer, Ninja, and a C++20
compiler.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the preview with:

```sh
./build/dev/src/shell/qindaqt-shell-preview --profile qindaqt --theme qinda-dark
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
