# Development, operation, and troubleshooting

Commands below run from the repository root. They are documented workflows, not
claims that this handbook change rebuilt or exercised the desktop.

## Build prerequisites

The snapshot uses CMake 3.25+, Ninja, C++20, Python 3, and Qt 6.11+. The default
presets require the release-matched KWin/Plasma Activities 6.6.6 integration,
KDecoration3, LayerShellQt, ECM, and focused KF6 dependencies. Feature targets
also require their confined providers, including fontconfig, qtermwidget6, and KF6 SyntaxHighlighting (Gentoo: `kde-frameworks/syntax-highlighting`).
`README.md`, `CMakeLists.txt`, per-module CMake files, and
`compositor/upstream/kwin.json` contain the actual requirements; package names
and a coherent pinned stack matter more than a generic distro recipe.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

`release` is a separate configure/build preset. Both defaults enable the native
plugin and production shell, strict warnings, and tests, while leaving host
uinput testing off. Do not bypass an ABI mismatch to make configuration pass.
A bridge-only build is described in `README.md`; it tests the public panel client
boundary and cannot establish native-plugin compatibility.

## Preview and real session

```sh
./build/dev/src/shell/qindaqt-shell-preview --profile qindaqt --theme qinda-dark
./tools/qindaqt-dev-session --list-scenarios
./tools/qindaqt-dev-session --scenario single-1080p --backend preview --dry-run
```

The preview demonstrates layout/rendering. A contained live session must boot the
actual compositor, production shell, services, and applications before it can
prove desktop behavior. Read the [testing harness](../development/testing-harness.md)
before executing nested rows: its private bus, output, seat, staging, cleanup,
and resource contracts protect both repeatability and the host desktop.
Production shell invocation assumes an appropriate Wayland session and service
composition, not an arbitrary standalone preview environment.

## Installation boundary

Installed-prefix checks verify package closure, desktop entries, runtime imports,
and session paths. They differ from an actual display-manager login and physical
hardware qualification. At this snapshot the latest `docs/HANDOFF.md` and
`docs/TASK_LIST.md` record a verified install boundary and the remaining privileged
install/SDDM smoke. Recheck the exact integrated commit and those records before
following deployment instructions. This handbook does not install or alter the
host session.

## Diagnose by boundary

| Symptom | Inspect first |
| --- | --- |
| CMake rejects KWin | Exact source/package ABI pin and coherent dependency versions. |
| Preview works but desktop does not | Real-session staging, service composition, production QML/token readiness, and first-paint evidence. |
| Unstyled applets or missing icons | QST facade publication, compiled runtime closure, installed desktop entries, and icon-theme provider. |
| Task or menu state is unavailable | Authenticated compositor facts, provider ownership, active-window identity, and menu-export lifecycle. |
| Control disabled or stale | Public client's owner/epoch/revision and advertised availability, then its confined backend. |
| Preference appears unsaved | Confirmed Settings1 baseline, optimistic conflict, and required fresh snapshot. |
| Display change uncertain | Display1 transaction state and journal/revert authority; avoid ad-hoc replay. |
| Test only fails in a combined run | Shared private runtime/resource collisions and the row's isolation requirements. |

Keep temporary logs, screenshots, and build artifacts under ignored build/cache
roots. Include exact commit, command, exit status, test count, reproduction, and
bounded caveats when reporting failures. See [repository catalog](catalog/repository.md)
for tooling and [quality](quality.md) for acceptance. Return to [index](index.md).

For installed bundled applications on Gentoo, use the [Portage package](../development/gentoo-apps.md). Raw build executables are not an installation artifact.
