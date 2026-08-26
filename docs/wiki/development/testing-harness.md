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
needs X11 clients. Test-only virtual-output/input controls are compiled out of
production builds.

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

The shell preview currently renders deterministic PNGs with its `--screenshot`
option. Its CTest matrix decodes captures at 1920x1080, 1920x1200, and
2560x1440 and verifies their exact dimensions. Input record/replay, topology
dump, panel geometry inspection, frame timing, and individual shell-service
restart remain required harness capabilities as their components land.

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

Scenario data declares output geometry/scale/rotation/refresh, profile, theme,
applications, fixture state, and input actions. Visual baselines pin fonts,
wallpaper, locale, time, animation clock, and sample data. Perceptual comparison
allows documented antialiasing tolerance; intentional baseline changes require
human review.

Tests cover Qt server-side decoration, GTK/Electron client-side decoration,
XWayland, SDL, Wine, Java, dialogs, fullscreen, crash, and unresponsive clients.
Window grouping exercises all [container invariants](../architecture/window-containers.md),
including mixed DPI, hotplug, overflow, detach, and restoration.

Virtual tests precede hardware checks on Intel, AMD, NVIDIA, hybrid graphics,
laptops, suspend/resume, touch, and stylus. Performance gates measure the
500 MiB/1% idle budget, startup, frame pacing, panel reveal, docking latency,
overview animation, and metrics overhead.
