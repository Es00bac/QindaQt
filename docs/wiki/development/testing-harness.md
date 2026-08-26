# Development and testing harness

QindaQt development must not modify or replace the developer's active desktop.
`qindaqt-dev-session` launches with isolated XDG config/data/cache/runtime
directories and a private D-Bus session. A failed nested run can therefore be
discarded without damaging the real session.

## Backend roles

| Backend | Use |
| --- | --- |
| Nested QindaQt/KWin Wayland | Primary interactive development and end-to-end shell/compositor behavior |
| KWin virtual framebuffer | Primary deterministic automation, virtual outputs, and injected input |
| Weston headless/windowed | Reference compositor for shell-client and standard Wayland behavior |
| Xephyr | Targeted legacy X11 client/style checks only; it is not representative of the Wayland desktop |
| Shell preview | Fast profile, theme, and controller development before compositor integration |

The QindaQt nested session owns its rootless XWayland instance when a scenario
needs X11 clients. External compositor mutation methods remain present for
contract testing but return `control-disabled` in normal sessions; only an
explicit isolated scenario plus the launcher's private development marker
enables them. The input observer has no injection or event-export control.

Baseline commands are:

```sh
./tools/qindaqt-dev-session --list-scenarios
./tools/qindaqt-dev-session --validate-scenarios
./tools/qindaqt-dev-session --scenario single-1080p --backend preview --dry-run
./tools/qindaqt-dev-session --scenario single-1080p --backend preview --smoke-test --execute
```

The stable backend names are `preview`, `wayland`, `virtual`, `weston`, and
`xephyr`. Planning/dry-run is the safe default; starting backend processes
requires explicit `--execute`. Automation may add `--json` for machine-readable
results. Preview smoke mode validates real profile/theme catalogs inside an
isolated process and exits; it does not substitute for a rendered-frame test.

For the QindaQt backends, `wayland` selects `qindaqt-wm --windowed` and
`virtual` selects `qindaqt-wm --virtual`. The harness passes width, height,
scale, and output count directly. It also passes the selected scenario path,
but the launcher only exports that path as `QINDAQT_TEST_SCENARIO`; no
compositor component currently replays the declared positions, rotations,
per-output scales, or hotplug actions. Catalog validation is therefore not
evidence that those topologies were applied.

The shell preview currently renders deterministic PNGs with its `--screenshot`
option. Its CTest matrix decodes captures at 1920x1080, 1920x1200, and
2560x1440 and verifies their exact dimensions. Input record/replay, topology
dump, panel geometry inspection, frame timing, and individual shell-service
restart remain required harness capabilities as their components land.

## Current compositor proof

The focused live checks are:

```sh
ctest --test-dir build/dev \
  -R '^(session\.(nested-wayland-xwayland|parent-wayland\.weston-headless|virtual-output\.|installed-plugin-discovery)|compositor\.(kwin-plugin-nested|kwin-plugin-unload-restores-clients|production-control-read-only))' \
  --output-on-failure
ctest --test-dir build/dev \
  -R '^compositor\.(kwin-plugin-nested|kwin-plugin-unload-restores-clients)$' \
  --repeat until-fail:10 --output-on-failure
```

The milestone boundary passed the complete 40-test suite in both Debug and
Release configurations. The focused commands above isolate its live session
proofs; they do not replace the complete-suite gate.

These tests boot beneath disposable XDG trees and private D-Bus sessions. Two
or more `QBackingStore`-backed probe windows commit real Wayland buffers and
repaint resize configures. The base proof verifies the Qt Wayland path and
reachable rootless XWayland. Weston 15's headless backend provides a real
parent Wayland socket for `qindaqt-wm --windowed`.

Build-tree runs always pass `<build>/plugins` explicitly. The separate
`session.installed-plugin-discovery` test stages `cmake --install` beneath the
build tree, starts the staged launcher without a plugin-root override, and
requires the installed KWin module to publish its live service. Configuration
tests also reject a conflicting ABI selection and prove that missing KWin is a
hard error while the plugin option is `ON`; only explicit `OFF` permits a
bridge-only build.

The virtual matrix verifies 1920x1080, 1920x1200, 2560x1440, 1920x1080 at
1.25 fractional scale, and two common 1920x1080 outputs. It compares Qt client
logical geometry/integer buffer scale with compositor logical geometry and
exact scale. The runner explicitly reports that names, primary selection,
positions, refresh rates, transformations, and scenario events were not
applied. Heterogeneous common modes/scales, transformations, and non-integral
logical extents fail instead of being misreported as coverage.

The main plugin workflow discovers three independent windows. It calls atomic
`DockWindows` at revision 1, creates a third-window page at revision 2,
activates and reactivates pages at revisions 3–4, detaches and restores the
third member at revision 5, and detaches the second member to trigger automatic
singleton unwrap/restore at revision 6. It then redocks and explicitly releases
the pair. Every independent window must regain its original frame and
ownership, inactive pages must retain the committed target frame without
expanding the active outer frame, and the final container inventory must be
empty. Invalid ratio and already-owned requests must reject without extra
state. The workflow also proves the live input schema, active non-consuming
observer, exact ABI, and method/event capability parity.

The unload workflow maps four real Wayland clients, groups them into two
containers, and calls KWin's `/Plugins.UnloadPlugin`. Independent
`/KWin.getWindowInfo` queries must then show the exact original frame and
non-minimized state for all four clients; a retitle/resize check proves a
client remains usable after teardown. The main page workflow and this unload
workflow each passed ten consecutive final repetitions.

A separate plugin-loaded launch omits `--test-scenario`. It proves read-only
inventory still works while `DockWindows`, `Submit`, and `ReleaseContainer`
all return `control-disabled` without changing ownership. The remaining
unproved areas are intentionally beyond the Compositor MVP boundary: consuming
pointer/keyboard docking and richer state restoration belong to Hybrid
interaction; heterogeneous topology replay and rotation/hotplug/lid policy
belong to Platform services; and DRM/KMS, physical GPU/input devices, and
suspend/resume remain Release qualification gates below.

## Required display matrix

Single-output scenarios cover:

- 1920x1080 at 100%, 125%, and 150%;
- 1920x1200 WUXGA at 100% and portrait rotation; and
- 2560x1440 at 100%, 125%, and 150%.

Multi-output scenarios cover dual 1080p horizontal and vertical layouts; mixed
1080p/1440p scaling; portrait WUXGA beside landscape 1440p; laptop plus external
display and lid closure; negative coordinates; unequal heights; gaps; mirroring;
adjoining-edge panels; primary-output transfer; and repeated hotplug, reorder,
rotation, and scale changes.

Every built-in [layout profile](../shell/layout-profiles.md) runs at 1080p,
WUXGA, and 1440p in light, dusk, and dark themes. Focused pairwise coverage may
run per change, but the complete matrix is a release gate. Qinda macOS also has
a focused decoration regression covering left-side traffic lights and
right-to-left tabs. A Qt Quick interaction test moves the pointer into and out
of the traffic-light cluster and verifies that hover glyph visibility follows.

## Determinism and acceptance

Scenario data can declare output geometry/scale/rotation/refresh, profile,
theme, applications, fixture state, and input actions. Backends must report
which declarations they actually consume before a run counts as coverage.
Visual baselines pin fonts, wallpaper, locale, time, animation clock, and sample
data. Perceptual comparison allows documented antialiasing tolerance;
intentional baseline changes require human review.

Tests cover Qt server-side decoration, GTK/Electron client-side decoration,
XWayland, SDL, Wine, Java, dialogs, fullscreen, crash, and unresponsive clients.
Window grouping exercises all [container invariants](../architecture/window-containers.md),
including mixed DPI, hotplug, overflow, detach, and restoration.

Virtual tests precede hardware checks on Intel, AMD, NVIDIA, hybrid graphics,
laptops, suspend/resume, touch, and stylus. Performance gates measure the
500 MiB/1% idle budget, startup, frame pacing, panel reveal, docking latency,
overview animation, and metrics overhead.
