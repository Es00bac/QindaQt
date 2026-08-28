# Development and testing harness

QindaQt development must not modify or replace the developer's active desktop.
`qindaqt-dev-session` launches with isolated XDG config/data/cache/runtime
directories and a private D-Bus session. A failed nested run can therefore be
discarded without damaging the real session.

Process and XDG isolation do **not** isolate Linux `uinput`: a `dotool` device
may be admitted by the host compositor and can move, click, or type on the
active desktop even when the nested KWin seat rejects it. Those two live input
tests are therefore absent from default builds. Registering them requires
`-DQINDAQT_ENABLE_HOST_UINPUT_TESTS=ON`; every invocation additionally requires
the exact `QINDAQT_ALLOW_HOST_UINPUT` acknowledgement shown below. Use only a
dedicated virtual seat or disposable machine. Ordinary local and CI test runs
must leave the option off.

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

The virtual-output mutation seam is narrower again: only an explicit scenario
launched with the exact `virtual` backend receives the private backend marker,
constructs the adapter, and advertises its typed add/remove methods. `wayland`
and every ordinary session omit the capability and pre-parse reject valid and
hostile requests identically. This marker is test construction metadata, not
caller authentication.

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

## Continuous integration lanes

The GitHub workflow keeps dependency policy, the portable value layer, and the
production Wayland client boundary in separate jobs:

| Job | Configuration | Evidence |
| --- | --- | --- |
| Repository policy | Ubuntu, no product build | Scenario/tool syntax, source-shape limits, strict wiki build, and links |
| Dependency-light core | Current Arch packages, `QINDAQT_BUILD_KWIN_PLUGIN=OFF`, `QINDAQT_BUILD_PRODUCTION_SHELL=OFF` | The complete registered bridge/value/service/preview suite plus an isolated preview smoke run |
| Production shell | Current Arch packages, KWin plugin `OFF`, production shell `ON` | Both backend-neutral shell-surface tests and the 1080p, WUXGA, and 1440p nested-KWin layer-surface tests |

Both build jobs fail on a missing test selection; neither uses a successful
empty CTest invocation as evidence. The production job installs and records
the resolved KWin, LayerShellQt, Qt Wayland, ECM, and KDecoration package
versions before configuring. Its rolling environment is intentional for the
public layer-shell client compatibility lane.

Arch packages `/usr/bin/kwin_wayland` with the `cap_sys_nice=ep` file
capability. An ordinary GitHub job container intentionally lacks `SYS_NICE` in
its capability bounding set, so Linux rejects that executable before KWin can
start. The disposable production-shell job accepts only that exact packaged
capability (or an already-empty capability set), removes it inside the
container, requires `getcap` to become empty, and smoke-runs
`kwin_wayland --version`. It does not grant the job or runner `SYS_NICE`, and it
does not alter the host installation. The nested virtual/QPainter matrix does
not need realtime scheduling; consequently it is functional layer-shell
evidence, not scheduler-latency or performance evidence. Do not copy this
container-only workaround onto a developer or installed desktop.

It is not evidence for QindaQt's native KWin plugin ABI. QindaQt pins KWin and
Plasma Activities to 6.6.5 exactly, while the Arch/Manjaro rolling repositories
had advanced to KWin 6.7.4 on 2026-08-26. The workflow therefore disables the
binary plugin instead of weakening or bypassing its exact CMake check. A full
default-preset build against the coherent qualified stack in the repository
`README.md` build instructions and the complete compositor selectors below
remain the ABI qualification boundary.

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
errors, lossless JSON-native settings values, and exactly one
notification-center instance in each of the ten stock profiles. The isolated
profile suite passed 3/3 in both Debug and Release for this contract revision.
That evidence qualifies the persistence boundary only; it does not claim live
shell surfaces or profile editing UI.

## Current design-token proof

QST-1 derivation and its QML adapter are selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.design-tokens-' --output-on-failure
```

The five tests cover schema-v1 and caller-input boundaries, property-style
metric ranges, deterministic reduced-motion/reduced-transparency/high-contrast/
text-scale transforms, exact opaque values for a loader-valid theme with alpha
in every source color, exact WCAG pair scopes across all five built-in themes,
GUI-thread-only publication, same-value suppression, and an offscreen QML
consumer observing one complete generation. The fifth gate performs a clean
staged install, configures a standalone C++ consumer against only installed
headers/static libraries, and verifies the exact QST-1 map plus representative
Qinda macOS accessibility values. The benchmark records an all-five-theme
derivation sample without an unstable absolute CI timing assertion.

These are value and software-renderer checks. They do not prove visual control
baselines, a Settings Center, Settings1 projection, assistive-technology bridge
behavior, repaint cost, memory residency, a production compositor, or physical
display behavior. Those gates begin with the later controls and application
slices. The exact role/pair contract and measurement policy are in
[QST-1 semantic design tokens](../architecture/design-tokens.md).

## Current reusable-controls proof

The compiled Controls S2 boundary is selected with:

```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-' --output-on-failure
```

The focused behavior test publishes complete QST generations and covers all
public components, direct Qt accessible interfaces, pointer-equivalent keyboard
actions, disabled/busy/error/degraded state, Information/Warning/Error/Success
and Busy announcement transitions, same-status content announcements,
status-before-content event-turn coalescing, required/error FormRow-to-editor
association, full versus partial/hostile theme previews, compact long-text flow, exact RTL
switch/slider geometry, all five built-in themes, and reduced motion and
transparency. The source-policy gate rejects theme identities, palette hex
literals, and forbidden layer/service/framework imports in production QML.

Visual rows are five themes by compact/ordinary/large widths at 100%, plus all
five ordinary-width themes at truthful 125% and 150% device-pixel ratios. The
25 deterministic CTest names each launch the same executable in a fresh process
with one exact, validated QtTest data selector. The wrapper rejects missing or
scale-incompatible rows and requires exactly the requested tagged pass; this
prevents Qt Quick software-render state from crossing window lifetimes as
specified by [ADR-0021](../adr/0021-isolate-controls-visual-rows.md). Each row
waits through a named control's published QST transition duration, then checks
the applied DPR and pixel dimensions before comparing reviewed PNG fixtures
under two required named host-font substitutions, C locale, offscreen platform,
and software rendering. This is environment determinism rather than a pin of
repository-owned font bytes. The
behavior gate separately proves reduced-motion duration projection. The gallery
includes explicit error, busy, disabled, degraded, checked, and ordinary states
so those appearances are reviewable in every row.
The staged consumer removes its previous build-confined prefix, installs the
current tree, requires the exact 14 Qt-generated QML deploy paths with no extra
QML source, and resolves representative Controls properties through strict
tooling analysis and compiled runtime loading only from that installed QML root.
Ambient source/build QML paths are absent. A separate no-threshold benchmark reports the median
PSS delta of a token-plus-controls gallery versus a matched bare Qt Quick
process from exact `smaps_rollup` PIDs.

The complete `^qindaqt\.controls-` prefix currently discovers 29 tests: one
behavior test, the 25 visual rows, source policy, staged installed import, and
the PSS measurement.

This boundary is software-renderer, package, and process-memory evidence. Live
AT-SPI, compositor focus, physical DPI/GPU output, Settings/AppShell/service
composition, and end-user application workflows remain outside S2. See
[QindaQt.Controls 1.0](../shell/controls.md) for the public contract.

## Current shell and panel-surface proof

Pure panel planning, editor transactions, visibility policy, owner-bound client,
orchestration, and backend-neutral surface reconciliation are selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.(shell-layout|shell-customization|shell-visibility|shell-orchestration|shell-surface)-' \
  --output-on-failure
```

The value modules also have standalone entry points under their corresponding
`tests/shell_*` directories. The two layout tests cover deterministic wildcard
expansion, every edge/alignment, cross-edge collision prevention, work-area
reservation, malformed inventories, checked coordinate boundaries, and the
1080p/WUXGA/1440p mixed-DPI logical matrix. They passed 2/2 in strict Debug and
Release builds and 2/2 under focused UndefinedBehaviorSanitizer
instrumentation.

The customization tests cover panel and applet commands, immutable
manifest-catalog placement decisions, exclusive coordinator lease handoff,
optimistic revisions and exhaustion headroom, all-output failure atomicity,
preview commit/cancel, durable plus provisional undo/redo, read-only editor
status, and side-effect-free command evaluation. Visibility tests cover every
hide mode, scope filtering, wire bounds, producer/consumer round trips,
owner/epoch/revision state, actual surface overlap, and fail-closed batch
validation. Client tests cover debounce, timeout/backoff, stale replies,
service loss, and exact unique-owner read/signal handoff on a private
`dbus-daemon`. Orchestration tests cover output-generation equality, profile
surface bijections, interaction leases, safe-visible planning, and policy-to-
surface translation. Surface tests cover exact anchor/margin/zone planning,
persistent in-place transitions, dismissal liveness, and prior-set retention
across backend failures. These are toolkit-neutral, fake-backend, or isolated
private-D-Bus proofs.

Applet production-resolution policy is selected without opening a window with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.applet-runtime-resolution$' --output-on-failure
```

It proves exact manifest lookup, horizontal/vertical placement rejection,
invalid-edge rejection, compiled implementation registration, typed unresolved
states, grant filtering, and deny-default least authority. The QML cache build
proves the clock and notification-center components are compilable; visual
behavior and interaction require a separately isolated display test and are
not claimed by this unit selector. Manifest and catalog tests additionally
prove that the notification-center package requests no capabilities.

The production Wayland surface matrix is selected with:

```sh
ctest --test-dir build/dev \
  -R '^shell\.production-surface\.(1080p|wuxga|1440p)$' \
  --output-on-failure
```

Each row boots a disposable virtual KWin session without enabling compositor
mutation APIs. A painted ordinary client first maximizes to the complete
output. The production `qindaqt-shell` then loads the schema-valid
`qindaqt-surface-proof` fixture and maps exactly two real layer surfaces. The
fixture retains QindaQt's qualified top-bar and 52%-width shelf geometry but
sets both hide modes to `never`. This keeps the initial-publication/work-area
proof independent from the live intelligent-hide policy that a maximized
window is expected to trigger. The harness rejects a fixture with another
identity, panel set, or hide mode before starting KWin.

The bounded client protocol parser associates every role with its unique
backing `wl_surface` and explicit common `wl_output`. Layer, anchor,
exclusive-edge/zone, and desired-size setters remain pending until that backing
surface commits. Each compositor configure snapshots the then-committed role
epoch; only a `configure -> acknowledge -> non-null buffer attach -> commit`
chain can establish the active mapped epoch. The probe freezes that exact epoch
while the reduced work area is observable, before shell teardown. Later or
unacknowledged configures and uncommitted setters cannot backfill the proof.

Both roles must request layer 2 when created. The top role's committed state
must be layer 2, anchors 13, edge/zone 1/30, and desired size `(0, 30)`; the
bottom shelf role must commit layer 2, anchors 6, edge/zone 2/54, and the
profile's exact 52%-width desired size. Both mapped configure sizes are checked
against the live output. The ordinary client must then maximize to the reduced
work area and return to the complete output after shell exit. Qualified logical
sizes are
1920x1080 -> 1920x996 -> 1920x1080, 1920x1200 -> 1920x1116 -> 1920x1200, and
2560x1440 -> 2560x1356 -> 2560x1440.

This proves real top/bottom layer roles, work-area causality, and teardown at
three resolutions. `shell.surface-protocol-trace` separately exercises stream
fragmentation, bounded line/chunk/capture rejection, pending-versus-committed
state, multiple configure selection and ordering, null/unmapped attaches, and
object-ID reuse/destroy ambiguity. Relevant malformed or over-bounded input
fails closed. Rendered settings drop targets, live automatic-hide protocol
transitions and animation, partial panels, and heterogeneous multi-output
surface publication remain unclaimed. The production code consumes live
window-state inventory, but this deliberately never-hidden nested matrix is not
evidence for that separate policy and transition boundary.

## Current notification foundation proof

The model and freedesktop adapter are selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.(notifications|notification-(host|presentation))-' \
  --output-on-failure
ctest --test-dir build/dev \
  -R '^qindaqt\.notification-(surface-layout|surfaces-offscreen)$' \
  --output-on-failure
ctest --test-dir build/dev \
  -R '^qindaqt\.notification-center-(entry|applet-offscreen)$' \
  --output-on-failure
ctest --test-dir build/dev \
  -R '^qindaqt\.(session-lock-|shell-runtime-options|notification-(privacy-policy|presentation-privacy))' \
  --output-on-failure
```

The three model/adapter tests cover bounded submissions and replacement,
lifetime-safe ID allocation, ownership, close, dismiss, expiry and action
policy, immutable revisions, standard identity and signals, protocol errors,
and two independent callers on a private D-Bus. Presentation protocol and
descriptor-channel tests cover token format/comparison, exact schema, restart
epoch, normalized and D-Bus values, bounds, ordering, malformed payload
rejection, exact one-shot records, and descriptor consumption. Four host
tests cover typed startup conflicts/failures, standard and private-object
rollback/release, single-presenter authentication, targeted revision signals,
disconnect handoff, authorized dismiss/action behavior, deterministic deadline
rearm/cancel/early-fire behavior, and the concrete one-shot QTimer.

The pure presentation-client test covers exact-owner authentication, timeout
and bounded retry, invalidation coalescing, late-reply rejection, revision and
epoch policy, operation preflight and serialization, resident action success
without a revision advance, timeout/malformed/remote-failure recovery,
owner-change interruption, stale operation replies, and release. A real
private-D-Bus test runs the Qt client against successive host owners and verifies
resynchronization and action activation-token forwarding.
`qindaqt.session-supervisor` proves the secret is absent from child arguments,
both descriptor consumers start, one child's exit tears down its sibling, and
second-child startup failure rolls back the first. These tests open no display
and inject no input.

`qindaqt.notification-presentation-model` covers first-snapshot baselining
without popup replay, new/replacement ordering, monotonic expiry, center-open
suppression, popup/history caps, transient exclusion, operation preflight,
success-only popup removal, retention and renewed expiry after rejection,
serialized busy state, eight-second production error lifetime, and injected Do
Not Disturb behavior: immediate low/normal filtering, critical bypass,
urgency-changing replacements, Active/Recent retention, and no replay on
disable, including service-owner/epoch rebaseline and a rejected operation
whose popup becomes suppressed while it is in flight.
`qindaqt.notification-presentation-policy` separately covers its
default-off session lifetime, change signals, exact critical admission, and
fail-closed unknown urgency while enabled.
`qindaqt.notification-privacy-policy` and
`qindaqt.notification-presentation-privacy` cover default denial, read-only
publication, complete model/status/timer clearing, critical suppression,
transport-free operation rejection, in-flight result suppression, and unlock
baselining without popup or history replay.
`qindaqt.session-lock-authentication` and
`qindaqt.session-lock-transitions` deterministically exercise the three-owner
quorum, expected-PID match, stale owner/request fencing, early locking, active
signals, double-inactive confirmation, bounded service-object retry, and
fail-closed stop/restart. `qindaqt.session-lock-qt-transport` runs the real
asynchronous Qt adapter against a private `dbus-daemon`, with separate KDE
`AboutToLock` and freedesktop `ActiveChanged`/`GetActive` interfaces on the same
object. It also kills that isolated daemon after reaching `Unlocked` and proves
immediate transport-loss revocation. `qindaqt.shell-runtime-options` and
`qindaqt.session-supervisor` prove
the token descriptor and compositor PID form one validated launch bundle.
The supervisor fixture also becomes a Linux child subreaper and uses disposable
subprocesses to prove that KWin-parent death terminates the witnessed supervisor
and supervisor death terminates a tokenized child; it never signals a desktop
or compositor process.
`qindaqt.notification-surface-layout` covers preferred sizes at 1080p, WUXGA,
and 1440p; 200% logical geometry; compact clamping; and unusably small output
rejection, including the zero-popup 38-logical-pixel status surface, the
center's 384-logical-pixel minimum usable width, and its 384x284 result on a
compact 400x300 output. Popup minimum usable width remains 240 logical pixels.
`qindaqt.notification-surfaces-offscreen` instantiates the card, popup, active,
and recent-center QML with the software renderer, verifies literal plain-text
body/error rendering and busy control disabling, and keeps overflow actions
plus Dismiss within a 400-pixel card. It also exercises the center's
Settings1-backed accessible Do Not Disturb control without synthesizing input.
At the planner's compact 384x284 result it also proves distinct English header
controls, an explicit bidirectional focus chain, and bounded busy/error status
geometry. Translated and right-to-left header layouts remain unqualified.

`qindaqt.notification-center-entry` uses an injected shortcut registrar to
cover the shell-private applet facade, stable action identity, default `Meta+N`
sequence, callback dispatch, request-acceptance state, and independently
observable active-binding changes. It never calls KGlobalAccel or mutates the
developer's shortcut registry. `qindaqt.notification-center-applet-offscreen`
uses the software renderer to cover the disabled-without-facade fallback,
accessible open/close labels, the narrow toggle request, read-only Do Not
Disturb state and indicator, and the audited QML entry-point dispatcher.
`qindaqt.notification-surfaces-offscreen` also covers
the window-scoped Escape close route and a focusable initial target without
activating a real surface. None of these tests opens a production surface or
injects input.

These presentation checks do not start a compositor or inject input. They are
not evidence for real layer-role mapping, screen placement, visual baselines,
focus transfer, keyboard navigation, assistive-technology behavior,
KGlobalAccel registration/remapping or live dispatch, compositor acceptance of
the center's activate-on-show request, multi-output migration, live Do Not
Disturb or lock-transition interaction, persistence, sound, multi-seat or
alternative-locker behavior, or live operation-result interaction.

## Current Settings1 and persistent quieting proof

Settings persistence and its consumers are selected with:

```sh
ctest --test-dir build/dev -R '^qindaqt\.settings-' --output-on-failure
ctest --test-dir build/dev \
  -R '^qindaqt\.(notification-quieting-settings-bridge|notification-quieting-controls-offscreen|notification-surfaces-offscreen)$' \
  --output-on-failure
```

Schema/model tests cover the v2 default/type, immutable-v1 loading, explicit
valid v1 migration, corrupt/unsupported rejection, and atomic documents.
Protocol tests cover ordinary and real-QtDBus recursive JSON-native
display-shaped values, shared aggregate byte/node budgets, depth/list/map/key
bounds, canonical `g:"v"` null, signed/unsigned integer edges, exact finite
double bit patterns, malformed Unicode/NUL rejection, direct-transport
pre-marshalling defense, and bounded fixed reply envelopes. Repository/service tests cover
copy-on-write save failure, no-op, conflict, validation, revision exhaustion,
validated QindaQt profile precedence/migration/rejection, hostile opaque
transactions, private-bus name collision, release, and restart. Client tests
cover serialized activation/backoff, synchronous-start recovery, exact
commit/invalidation lineage, exact UnknownKey empty-authority acceptance and
fabricated-map rejection, timeout uncertainty without replay, stale-owner
subscription and pending-reply generation fencing, activation completion
without an owner, repeated-start-failure Retry truth, same-owner epoch and
equal-revision contradiction rejection, same-object stop/start, replacement epoch/rebaseline,
real private-bus nested Object commit, profile fallback after user removal,
persistence/reconstruction with exact null/list/map/numeric metatypes,
oversized persistent-startup rejection, and local daemon loss. Controller tests
cover owner loss while accepted-save/conflict intent is pending and persistent
validation/save/revision-exhaustion diagnostics without replay.
Settings-app/shell QML tests cover failure/Retry/recovery, stable confirmed
diagnostics, structural state, focus, and accessibility semantics.
The bridge proves fail-quiet initialization, retained last-confirmed values,
independent privacy precedence, ordinary-controller reopen, repeated shell
reconstruction with one service, both-side reconstruction from one file, and
no replay.

The service process-lifecycle test creates a real activation directory and two
successive private `dbus-daemon` instances. It records the first activated
service's exact PID, owner, and epoch, terminates that daemon, and requires the
PID to disappear within five seconds. It then activates through the same
descriptor on a replacement daemon, requires a distinct live process/owner/
epoch, and terminates the second daemon with the same no-orphan assertion. An
exact-executable cleanup guard prevents a failed assertion from leaving either
fixture process resident. The same test accepts explicit
`QINDAQT_TEST_SETTINGS_SERVICE_{DATA_ROOT,EXECUTABLE,SCHEMA_DIR}` paths so the
release gate can rerun the identical daemon-loss/reactivation proof through the
installed activation descriptor, binary, and schemas in an isolated prefix.

This evidence uses private D-Bus and offscreen software rendering. It does not
claim a real session bus, live assistive technology, compositor focus,
KGlobalAccel dispatch, or pointer/keyboard automation.

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

The D0 hotplug row starts from the 1920x1080 virtual scenario, records the
current `outputGeneration`, adds one bounded logical-size output, and requires
exactly one later accepted generation after coalescing. It treats
`OutputsChanged` as an invalidation hint, then rereads `Outputs` and
`ShellVisibilitySnapshot` until both carry the same new `outputGeneration` and
output set. Removal must converge the same way. A separate launch without the
exact virtual marker sends both valid and hostile requests and proves identical
`control-disabled` replies with no accepted inventory generation or backend
change. This virtual proof makes no DRM, GPU, connector, monitor, lid, or
physical hotplug claim.

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
cmake -S . -B build/host-uinput -G Ninja \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=ON
cmake --build build/host-uinput
QINDAQT_ALLOW_HOST_UINPUT=I_UNDERSTAND_THIS_CAN_CONTROL_THE_HOST_DESKTOP \
ctest --test-dir build/host-uinput \
  -R '^compositor\.hybrid-pointer-(nested|plugin-unload-restores-clients)$' \
  --output-on-failure
QINDAQT_ALLOW_HOST_UINPUT=I_UNDERSTAND_THIS_CAN_CONTROL_THE_HOST_DESKTOP \
ctest --test-dir build/host-uinput \
  -R '^compositor\.hybrid-pointer-(nested|plugin-unload-restores-clients)$' \
  --repeat until-fail:10 --output-on-failure
```

The CMake opt-in controls test registration. The per-invocation acknowledgement
is checked before `dotool` or KWin starts; without it, CTest records the test as
skipped with exit code 77. This double gate also protects an old build directory
whose cache still has the dangerous option enabled.

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

## Current Audio1 proof

The focused Audio1 boundary is selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.audio-(protocol|client|qt-transport|activation|service|wireplumber-(runtime|reset-lifecycle))$' \
  --output-on-failure
```

Protocol tests cover exact fixed signatures, round trips, malformed enums,
lineage, sorting and uniqueness, target compatibility, finite normalized
levels, UTF-8 byte limits, and oversized-array rejection. Fake transport and
backend tests cover snapshot/model changes, operation validation, stale handles,
exact owner/epoch/revision binding, invalidation coalescing, owner replacement,
equal-revision contradiction rejection, queued exactly-once completion for all
public result classes, stopped/superseded backend generations, malformed
backend-outcome normalization, timeouts, uncertain outcomes, and the no-replay
rule. Private session-bus tests
cover delayed operations across successive unique owners plus executable D-Bus
activation. They tear down the constructing daemon, prove that exact activated
PID exits, activate a fresh PID/owner/epoch on a replacement daemon, and repeat
the no-orphan proof. Cleanup revalidates `/proc/<pid>/exe` before bounded
TERM-to-KILL fallback, so a reused PID is never signalled.

`qindaqt.audio-wireplumber-runtime` creates its own `XDG_RUNTIME_DIR`, PipeWire
socket, WirePlumber `policy` profile, state/config roots, and intentionally
unreachable session-bus address. It creates only disposable null sinks/sources,
starts a synthetic playback stream against that private remote, and exercises
real libwireplumber graph discovery, default, normalized volume, mute, stream
target metadata, WirePlumber restart/epoch advance, and stale-handle rejection.
It then races eight submitted operations against consecutive WirePlumber
disconnect/reconnect cycles before backend teardown, covering cancellation and
late-completion lifetime. A separate production-adapter loop repeatedly stops
before draining Qt callbacks for 250 cycles against an unreachable private
runtime, bounds file-descriptor growth, proves no post-stop publication,
restarts without draining an older run, and accepts only the fresh
generation/epoch. It is serial and must fail rather than fall through to the
host graph.

`qindaqt.audio-wireplumber-reset-lifecycle` uses a private worker-only scheduler
probe to pause after the disconnect idle is attached, confirms that explicit
stop has queued its higher-priority cleanup, then releases the signal turn. It
repeats two complete private PipeWire loss/stop/restart/second-loss cycles and
requires every second loss to advance epoch and publish `pipewire-unavailable`,
with bounded file descriptors and exact child reaping. This ordering test must
remain deterministic; an unforced timing loop does not cover the source/latch
ownership contract.

This runtime proof does not use physical microphones, speakers, or input and
does not qualify USB/HDMI/Bluetooth/jack/multichannel behavior, suspend/resume,
hotplug, realtime latency, hardware gain mappings, resource budgets, or Audio
Settings/shell UI. Those are later isolated hardware and integrated-session
gates; a `wpctl`-based test or production fallback is not equivalent evidence.

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
laptops, suspend/resume, touch, and stylus. The first integrated-session gate is
a bootable QindaQt desktop under an isolated parent Wayland compositor with a
private runtime directory, private buses, test-only input devices, and captured
screenshots. It must exercise 1920x1080, 1920x1200 WUXGA, and 2560x1440 plus the
required scale/profile/theme variants without connecting to the host seat or
moving the host pointer. See [ADR-0015](../adr/0015-qualify-function-before-resource-refinement.md).

Performance gates initially measure a 1,024 MiB aggregate idle PSS ceiling and
1% average idle CPU budget, startup, frame pacing, panel reveal, docking
latency, overview animation, and metrics overhead. The memory ceiling is a
bring-up budget, not a reason to block functional integration prematurely;
measure first, then lower it from evidence after the nested desktop is stable.
