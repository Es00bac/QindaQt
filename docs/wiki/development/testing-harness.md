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
enables them. The input observer has no injection or event-export control. A
separate development-only `KWin::InputDevice` injector exists only in those
isolated mutation-enabled sessions and has a bounded event schema; production
rejects its public method before parsing.

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

## Current profile-contract proof

The authoritative schema-v1 loader and typed validator are selected in any
complete build with:

```sh
ctest --test-dir build/dev -R '^qindaqt\.profile-' --output-on-failure
```

The same three tests can be configured directly from `tests/profiles` when a
later in-progress module makes the complete tree temporarily unbuildable. They
cover built-in/catalog behavior, strict JSON syntax and field types, exact
integer and range handling, global applet identity, deterministic structured
errors, and lossless JSON-native settings values. The isolated profile suite
passed 3/3 in both Debug and Release for this contract revision. That evidence
qualifies the persistence boundary only; it does not claim live shell surfaces
or profile editing UI.

## Current shell value-layer proof

Pure panel planning and editing transactions are selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.(shell-layout|shell-customization)-' \
  --output-on-failure
```

Both suites also have standalone entry points under `tests/shell_layout` and
`tests/shell_customization`. The two layout tests cover deterministic wildcard
expansion, every edge/alignment, cross-edge collision prevention, work-area
reservation, malformed inventories, checked coordinate boundaries, and the
1080p/WUXGA/1440p mixed-DPI logical matrix. They passed 2/2 in strict Debug and
Release builds and 2/2 under focused UndefinedBehaviorSanitizer
instrumentation.

The four customization tests cover panel and applet commands, immutable
manifest-catalog placement decisions, exclusive coordinator lease handoff,
optimistic revisions and exhaustion headroom, all-output failure atomicity,
preview commit/cancel, and durable plus provisional undo/redo. They passed 4/4
in strict Debug and Release builds. These are toolkit-neutral value-layer
proofs; rendered drop targets, live panel surfaces, pointer/keyboard adapters,
window-aware hiding, and output-hotplug session replacement remain unclaimed.

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
requires the installed KWin module to publish its live service. The same test
requires `org.qindaqt` at KDecoration3's KDE-relative plugin destination and
checks that a fresh isolated `kwinrc` selects that module. The focused
`session.sessiondefaults` test separately proves that the first-run seed never
overwrites an existing decoration choice. Configuration tests also reject a
conflicting ABI selection and prove that missing KWin is a hard error while the
plugin option is `ON`; only explicit `OFF` permits a bridge-only build.

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
inventory still works while `DockWindows`, `Submit`, `ReleaseContainer`, and
`InjectTestInput` return `control-disabled` without changing ownership; input
rejection occurs before payload parsing. It also requires
`ReinitializeCompositingForTest` to reject before compositor-state inspection.
Consuming pointer/keyboard docking and richer restoration were intentionally
beyond the Compositor MVP and are qualified separately below. Heterogeneous
topology replay and rotation/hotplug/lid policy belong to Platform services;
DRM/KMS, physical GPU/input devices, and suspend/resume remain Release
qualification gates.

## Completed Hybrid qualification evidence

The focused Hybrid value and adapter suite is selected with:

```sh
ctest --test-dir build/dev \
  -R '^(hybrid\.|hybrid-chrome\.|qindaqt\.(hybrid-|decoration-hover)|compositor\.(hybrid.*|development-input-protocol|container-close-prompt|kwin-(input-adapter|development-input-injector|chrome-manager|chrome-scene-lifecycle|hybrid-(scene|pages|focus|context|task-identity|recovery)|dock-preview|group-context-menu))|session\.sessiondefaults)' \
  -E '^compositor\.hybrid-pointer-' \
  --output-on-failure
```

The focused suites cover:

- every session-topology command, global ownership invariant, singleton
  normalization, lifecycle add/forget, deterministic structural ID, and
  prepare/commit rollback boundary;
- independent-to-group insertion, independent tab grouping,
  member-to-independent regrouping, cross-container member and complete-page
  moves, leaf/split-page detach, whole-page regrouping with an independent
  target, one-member page extraction, same-container page/member reorganization,
  tab activation, intentional tab-to-edge rejection, and split-ratio update;
- recursive minimum/maximum/fixed constraints, deterministic rounding,
  overflow reporting, and schema-v2 round trips of every independent-window
  restore field;
- exact-modifier gesture ownership and drag threshold; keyboard docking,
  explicit detach, complete-group movement, active-divider adjustment,
  complete-group edge/corner resize, preview/commit/cancel, cumulative baseline
  displacement, and pass-through for unrelated input;
- Qinda macOS and conventional chrome plans, logical-DPI invariance, left
  traffic lights with cluster-hover glyphs, stable logical/right-to-left visual
  tabs, pure hit precedence, thresholded natural drags, and grab cancellation;
- scene/chrome plan agreement, scene-image teardown/rebuild, ordinary
  compositor router ownership and native member/client pass-through, hover
  forwarding, anchor-relative exposure and occlusion, popup-before-decoration
  ordering, topmost-member stack ranking, coalesced
  stack/activation/output/window republish, dock-preview geometry, semantic
  chrome-drag translation, and shortcut action dispatch;
- fake-platform KWin transactions for full-state restore, focus, inactive-page
  minimization, dead members, overflow, cross-container one-step ownership,
  direct reflow, maximize/restore, independent-focus preservation, member
  maximize/fullscreen focus mode, focus-safe minimize/close/native detach,
  transient admission/following, atomic output/workspace/activity/layer
  propagation, collapsed task identity, and rollback/recovery of state, focus,
  target frames, and copied committed layout;
- nonblocking Close All/Ungroup/Cancel policy, the outer-title group context
  menu, normal-chain development input parsing/injection, compositor scene
  lifecycle, and production pre-parse input rejection; and
- KDecoration factory/metadata loading plus first-run default seeding that
  preserves an existing user choice.

The live Hybrid workflows are selected with:

```sh
ctest --test-dir build/dev \
  -R '^compositor\.hybrid-pointer-(nested|plugin-unload-restores-clients)$' \
  --output-on-failure
ctest --test-dir build/dev \
  -R '^compositor\.hybrid-pointer-(nested|plugin-unload-restores-clients)$' \
  --repeat until-fail:10 --output-on-failure
```

Both select three painted, independently owned Wayland probe windows; the
unload workflow maps a fourth independent client as well. They require
`Capabilities.hybrid.ready` plus `inputFilterInstalled`. The harness
starts one persistent `dotool` process, attempts to admit its host uinput
keyboard/pointer, and reports the detected devices when successful. In the
qualified KWin virtual environment that seat did not admit host uinput, so the
harness records the concrete failure and uses the isolated
`qindaqt-development-input` device. That device is added through KWin input
redirection, and its absolute-pointer, left-button, and left Meta/Shift events
traverse the ordinary spy/filter/controller chain. The fallback is never
constructed in production and is not physical-device evidence.

`compositor.hybrid-pointer-nested` performs a complete exact
`Meta+Shift+Left` title drag. It requires the process-local topology revision to
advance, the source and target to share one owner, the planned frames to form a
valid split with its divider gap, `Containers` to report the actual nonzero
revision and `hybrid-process` authority, and `Snapshot` to return the matching
schema-1 model. While that group remains owned, the workflow requests a
development-gated compositor reinitialization, observes the inactive-to-active
transition, and requires the same container/revision with one visible anchored
scene item afterward. It moves an unrelated client over shared chrome and
proves the covered point cannot click through, proves the popup-dismiss press is
consumed before Hybrid, keeps a normal-type transient dialog outside topology
with focus preserved, and first verifies transient association does not pull the
group above an unrelated client. A later exposed shared-title press then raises
the group above that client as one contiguous unit.

The workflow then performs a plain left-button drag from the native KDecoration
member title to empty desktop space. Detach must occur at KWin's
interactive-move start; the dragged window keeps its original independent size,
continues to the pointer drop, and matches its target frame there. The sibling
must return to its exact original current and target frames, every
owner/container must clear, and the topology revision must advance again. The
dragged member is not compared to its original position because the native move
intentionally relocates it. This proves native-decoration fall-through and
detach, not a synthetic member-strip route; preview/chrome pixels remain
offscreen-renderer evidence rather than a live screenshot baseline.

`compositor.hybrid-pointer-plugin-unload-restores-clients` creates the same
process-local group and first requires three mapped clients to expose live
server-side `QindaDecoration` instances. It calls KWin's
`/Plugins.UnloadPlugin` while the group is owned, then requires the plugin and
`org.qindaqt.Compositor` service to disappear. Independent KWin queries must
show the exact pre-group frames and non-minimized state; all four clients remain
exposed, and a grouped source must still retitle and resize. No legacy bridge
container is used.

The Hybrid boundary is the focused selector and live-workflow selector above,
plus the already-qualified prior-milestone suite. The shared Debug and Release
registries each passed 93/93; those totals include later-milestone Shell tests,
so the explicit Hybrid selector remains authoritative and passed 48/48 in both
configurations. A fresh strict-warning Debug bridge-only configuration with
`QINDAQT_BUILD_KWIN_PLUGIN=OFF` and `QINDAQT_BUILD_SHELL=OFF` passed 45/45.

The applied virtual subset passed at 1920x1080, 1920x1200 WUXGA, 2560x1440,
fractional 1080p at 125%, and dual common-1080p. Production read-only rejection,
live outer-title menu and queued group-context adoption, scene restart, native
detach, grouped plugin-unload restoration, decoration proof, and continued
client usability passed in Debug and Release.

Focused ASan+UBSan qualification passed all 47 registered Shell-off Hybrid
tests with leak detection and halt-on-error, then passed the QML decoration
test under preloaded ASan+UBSan runtimes after an instrumented Shell/QML-cache
build, for 47+1 exact-selector coverage and zero findings. Both
`compositor.hybrid-pointer-nested` and
`compositor.hybrid-pointer-plugin-unload-restores-clients` passed ten
consecutive Debug repetitions. Strict documentation, link, source-shape,
whitespace, and independent final-audit gates passed. Together these results
complete the Hybrid interaction milestone.

Moving one live group between heterogeneous scales, physical input, DRM/KMS and
GPU vendors, suspend/resume, hotplug/rotation/lid policy, and performance/memory
budgets remain Platform or Release qualification work. Do not substitute the
older D-Bus bridge workflow for the process-local evidence accepted in
[ADR-0004](../adr/0004-process-local-hybrid-topology.md).

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
right-to-left tabs. A Qt Quick shell-preview interaction test and an offscreen
Hybrid-chrome renderer test independently move hover state into and out of the
traffic-light cluster and verify glyph visibility. Neither is a screenshot of
the live KDecoration or KWin scene item.

## Determinism and acceptance

Scenario data can declare output geometry/scale/rotation/refresh, profile,
theme, applications, fixture state, and input actions. Backends must report
which declarations they actually consume before a run counts as coverage.
Visual baselines pin fonts, wallpaper, locale, time, animation clock, and sample
data. Perceptual comparison allows documented antialiasing tolerance;
intentional baseline changes require human review.

Release tests must cover Qt server-side decoration, GTK/Electron client-side
decoration, XWayland, SDL, Wine, Java, dialogs, fullscreen, crash, and
unresponsive clients. Window grouping must exercise all
[container invariants](../architecture/window-containers.md), including mixed
DPI, hotplug, overflow, detach, and restoration.

Virtual tests precede hardware checks on Intel, AMD, NVIDIA, hybrid graphics,
laptops, suspend/resume, touch, and stylus. Performance gates measure the
500 MiB/1% idle budget, startup, frame pacing, panel reveal, docking latency,
overview animation, and metrics overhead.
