# Compositor and session integration

The Compositor MVP and Hybrid interaction milestones are **complete**. QindaQt
launches a real KWin Wayland compositor, loads an exact-release native plugin,
owns rootless XWayland, reports output/input state, and commits container model
plus live scene changes atomically. The process-local Hybrid runtime adds live
pointer and native-decoration interaction, member/public state, scene-resident
chrome, lifecycle recovery, context-menu policy, and unload restoration.
Evidence for the two completed milestone boundaries is kept distinct below.

## Pinned KWin and binary ABI

The source manifest pins one immutable upstream state:

| Field | Value |
| --- | --- |
| Release/ref | KWin `6.6.5`, `refs/tags/v6.6.5` |
| Tag object | `1b035282ff05101a3441113648a93f57fe0351c1` |
| Commit | `b04d59c03749484a8a0ed5a8d4cda515a267c59b` |
| Tree | `99868e7da683d59f3ed90b0f0fe7ebfa4be5bc2b` |
| Current downstream patches | Zero |
| KWin CMake target | `KWin::kwin`, found with version `6.6.5 EXACT` |
| Plugin factory ABI/IID | `KWin::PluginFactory`, `org.kde.kwin.PluginFactoryInterface6.6.5` |

This is a binary plugin ABI, not a compatibility range. QindaQt must rebuild
and rerun the compositor matrix for every KWin patch release. The manifest
records KWin's Qt minimum as 6.10.0 while QindaQt's baseline is Qt 6.11.

The supported ABI is not a CMake option. Configuration removes the former
cache entry, rejects a conflicting value, verifies the fixed ABI against the
manifest, and requests `find_package(KWin 6.6.5 EXACT)`. With
`QINDAQT_BUILD_KWIN_PLUGIN=ON` (the default), missing exact KWin or Qt
DBus/Widgets dependencies are fatal. Only an explicit `OFF` selects a
bridge-only build.

Pin checks are:

```sh
./compositor/tools/verify-kwin-source
./compositor/tools/verify-kwin-source --check-remote
```

The verifier also supports `--fetch NEW_DIRECTORY` and `--verify CHECKOUT`.
Patch filenames and SHA-256 values become mandatory if the currently empty
series gains entries. [ADR-0001](../adr/0001-use-kwin-as-compositor-base.md)
records this maintenance model.

## `qindaqt-wm` launcher

`qindaqt-wm` validates options, establishes QindaQt session markers and plugin
search paths, builds an argument vector, and replaces itself with
`kwin_wayland`. It is a session launcher, not a renamed KWin fork.

| QindaQt backend | KWin invocation | Qualified evidence |
| --- | --- | --- |
| DRM/KMS, default or `--drm` | `--drm` | Command construction; physical GPU/KMS remains a release gate |
| Nested Wayland, `--windowed` | `--wayland-display $WAYLAND_DISPLAY` | Weston 15 headless parent socket and mapped Qt clients |
| Virtual, `--virtual` | `--virtual` | Wayland, rootless XWayland, compositor plugin, output/input inventories, and container workflows |

The launcher accepts one to sixteen outputs, dimensions from 320x200 through
32768x32768, scales from 0.5 through 4.0, and a sanitized child socket name.
Rootless XWayland is on by default and can be disabled with `--no-xwayland`.
A `--session` process controls compositor lifetime. Production-shell builds
default it to `qindaqt-session`, which starts the notification host and shell,
provisions their private presentation token through separate one-shot inherited
descriptors, and keeps the token only in supervisor memory. The notification
host is session-resident. One unexpected shell exit consumes a fixed
one-restart budget: the host stays running while a replacement shell receives
a fresh descriptor containing the same token, the same direct KWin PID, and
the same profile/theme arguments. Host exit, replacement-start failure, a
second shell exit, or explicit stop ends the complete session. This contract is
recorded in
[ADR-0019](../adr/0019-restart-the-production-shell-once.md). Because
KWin directly launches this supervisor, it arms a kernel parent-death signal,
race-checks and validates the direct parent PID, and passes the value to the
shell with the presentation descriptor. Host and shell also die with their
supervisor, preventing PID-reuse trust after KWin exits. The shell uses that
non-secret PID only to authenticate the unique D-Bus owner that publishes
compositor and KScreenLocker state; a replacement must authenticate that path
again and receives no inherited privacy state. An
explicit `--session` still overrides the default for isolated test probes and
alternate session compositions; bridge-only builds retain an empty default.

`--test-scenario` is an explicit development marker, not implicit trust from
the environment. The launcher clears inherited test/development markers and
sets them only for a validated scenario path. It applies common width, height,
scale, and output count through KWin's virtual/windowed command line. It does
not replay scenario names, primary selection, independent positions, refresh
rates, transforms, or event sequences; the runner reports those as not applied
and rejects configurations the common-argument backend cannot represent.

## Plugin discovery and installation

The canonical build artifact is
`<build>/plugins/kwin/plugins/qindaqt_compositor.so`. Build-tree tests pass
`<build>/plugins` explicitly. Installed discovery uses the same
`KDE_INSTALL_PLUGINDIR` that owns the plugin rule; no build directory is
compiled into the launcher.

For a relative KDE layout, the launcher reconstructs the install prefix from
its executable directory, so staged and relocated installs keep launcher and
plugin aligned. Absolute KDE plugin paths use the configured absolute fallback.
A staged-install test starts the installed launcher without `--plugin-root`,
then requires the installed plugin's live service and exact ABI. It also
requires the `org.qindaqt` KDecoration3 artifact at KDecoration3's exported
relative plugin directory and verifies that a fresh isolated `kwinrc` selects
it. The launcher seeds that selection only when the key is absent, preserving
an explicit user choice.

## Compositor-MVP runtime layers

The KWin plugin provides:

- a registry of normal non-internal windows keyed by KWin's internal UUID;
- ordered logical output inventory with stable transform names, exact scale,
  numeric mHz refresh, and change invalidation;
- an independently admitted, bounded shell-visibility publisher that atomically
  snapshots exact logical outputs, normal/dialog/utility user windows, current
  workspace/activity, Hybrid whole-group maximize state, and a per-service
  epoch behind one coalesced Compositor1 invalidation;
- sanitized input-device capabilities and a non-consuming lifecycle-safe
  `InputEventSpy` with no key text/native scan codes/serials or public event
  stream;
- an atomic `DockWindows` entry point plus a revisioned seven-operation
  container protocol;
- active-page-aware layout planning, minimized inactive pages, acknowledged
  versus committed target geometry, and exact prior-frame restoration;
- automatic singleton unwrapping and close reconciliation; and
- orderly release of every group before KWin dynamically unloads the plugin.

The bridge applies operations to a candidate model, prepares the live scene,
and publishes one revision only after commit. Its staged-split API admits a
one-leaf model only inside one synchronous call and removes it after prepare or
commit rejection. Focused failure tests prove the bridge model and staging
registry remain unchanged when an adapter rejects. The KWin transaction
preflights `QPointer` lifetimes, stages restore state, reverses already-applied
frame/minimized requests when finalization fails, then updates ownership and
committed target frames together. A deliberately injected mid-KWin-commit
failure is not part of the current live harness, so it is not claimed as live
failure evidence.

For Wayland clients, `geometry` is the currently acknowledged frame and
`targetGeometry` is QindaQt's committed planned frame for a member. A minimized
inactive client can defer configure acknowledgement until activation; exposing
both values avoids pretending asynchronous state is synchronous.

## Process-local Hybrid runtime

`KWinHybridSession` owns a second, session-wide topology used by production
input. It composes narrowly scoped collaborators for semantic command
translation, recursive constraints and restore state, KWin scene transactions,
direct group placement, immutable chrome planning, one scene image per
container, a pure ordinary-chrome pointer router, whole-group context adoption,
member focus policy, transient following, dock preview, exact-modifier input
filtering, and global keyboard entry actions. It shares only the managed-window
registry with the older D-Bus bridge; the two topology models are not
interchangeable.
[ADR-0004](../adr/0004-process-local-hybrid-topology.md) records why this
authority stays inside the compositor process.

The topology coordinator stages one globally valid multi-container candidate.
The KWin transaction captures full independent state, constraint-solves every
page, applies members and focus, atomically finalizes ownership plus planned
frames, and only then publishes restore/layout maps and the topology revision.
Commit failure reverses applied members and focus. Tree-preserving group move,
resize, maximize, and restore use a separate rollback-safe reflow transaction
and do not invent a topology revision.

Shared chrome uses global logical coordinates and a copied committed layout.
Production creates no chrome `QWidget`, `QWindow`, internal window, mask, or
focusable/input surface. `ChromeRenderer` produces one transparent ARGB image
per group, and KWin owns it as an `ImageItem` parented to the group's topmost
active-page member `WindowItem`. That member supplies the real stack slot, so a
later unrelated window covers the item and associated dialogs remain above the
complete member block. Missing or stale anchors hide chrome rather than leaving
a detached plan visible. This choice is recorded in
[ADR-0005](../adr/0005-scene-resident-hybrid-chrome.md).

Ordinary interaction is resolved and consumed by
`HybridChromePointerRouter` inside KWin, with hover sent back to the scene
renderer. Its live-stack exposure gate blocks input wherever any eligible real
window above the member anchor covers the point; internal windows, popups, and
transients cannot cause click-through. The filter runs at Decoration order,
after KWin's Popup filter and before native KDecoration, so the press that
dismisses a popup does not also mutate Hybrid. Member-title and client regions
pass through to KDecoration/client input. The separate dock-preview widget is
input-transparent and sets `outputOnly` on its backing `QWindow` before map so
it cannot occlude its own target. Production selects the Qinda macOS style: left
close/minimize/maximize-or-restore traffic lights reveal `x`, `_`, and `[]`
glyphs on cluster hover, and tabs are placed visually right to left without
changing logical page order. Details live in [Hybrid chrome](hybrid-chrome.md).

Ordinary shared controls, tabs, dividers, outer-title movement, and outer-edge
resize need no modifier. A plain member-title drag remains KWin's native move;
its interactive-move start atomically detaches that member and the native move
continues to the drop with restored independent size. Exact
`Meta+Shift+Left` provides the compositor docking grab for independent and
grouped titles. Thirteen autoloading actions cover member and complete-page
docking, group move/resize, split adjustment, page activation/reorder, and
group close/minimize/maximize/restore policy. Interactive arrows update from one
stable baseline, while `Enter` commits and `Escape` cancels. The filter consumes
events only while it owns a grab; unrelated input and the older independently
non-consuming input spy pass through.

Tabs carry page identity rather than only a representative member. Runtime
commands move or detach a complete page/tree, regroup it with an independent
tab target, or extract one member into a new page. A tab-to-edge request is
rejected until a typed subtree-as-split operation exists.

Member maximize/fullscreen focus mode, focus-safe minimize/close/native detach,
and dialog/transient following are active KWin policies. Popups, dialogs, and
all transients are excluded from topology even if KWin reports a normal type.
The transient adapter follows stable owner-relative geometry plus output,
desktops, and activities but never raises on its own; group stacking alone keeps
transients above the contiguous member block without moving the group above an
unrelated active window. Opaque all-KWin UUID focus tokens preserve focused
dialogs across scene commit and rollback.

Stack and activation changes plus registry output-inventory changes coalesce one
chrome republish. Scene items rank by their topmost active-page member and
resample committed geometry and live output scale. Separately, a member change
to output, workspaces, activities, Keep Above, or Keep Below queues one canonical
source per container. The rollback-safe scene transaction maps the complete
outer frame between output placement areas when needed, re-solves every page,
and applies the final context to every member without changing their independent
restore snapshots. The outer-title right-click menu exposes the same layer,
pin/workspace, activity, and Move to Output operations by mutating one live
representative and using that queued whole-group path. Failed adoption restores
every member and focus, then releases the group if it cannot remain coherent.
Scene commands also preserve an untouched active, non-minimized independent
window across add/forget, group replan, and multi-group release.

KWin compositor reinitialization is a scene-resource transition, not a topology
transition. Direct `aboutToToggleCompositing` and `aboutToDestroy` handlers
invalidate input targets and clear every chrome image, stack anchor, and
accessibility root before KWin destroys client `WindowItem` objects. Chrome
synchronization remains suspended until `compositingToggled(true)`, after the
replacement client items exist, then rebuilds from the unchanged topology and
committed layouts. `sceneCreated` is deliberately too early for republishing.

Plugin teardown disconnects queued lifecycle synchronization, destroys the
shortcut/filter, cancels preview and placement, restores temporary member focus,
and releases process-local groups while scene state and transient following are
live. It then clears shared chrome, destroys Hybrid collaborators, releases
legacy bridge groups, and finally unregisters D-Bus. A failure to release one
Hybrid group does not stop attempts for the remaining containers.

## Control security mode

The service uses the ordinary user session bus and has no caller
authentication. Production therefore advertises `controlMode: "read-only"`.
`DockWindows`, `Submit`, `ReleaseContainer`, `InjectTestInput`,
`AddVirtualOutputForTest`, `RemoveVirtualOutputForTest`, and
`ReinitializeCompositingForTest` reject with `control-disabled` before request
parsing, runtime inspection, or mutation. The development-only
`DevelopmentShellSurfaces` inventory likewise rejects before inspecting KWin
state; it is not one of the production-readable snapshots. Only the isolated
explicit scenario path enables the
`development-test` mutation mode. Production Hybrid gestures call typed
process-local policy instead of enabling D-Bus mutation.

Development test sessions construct one combined keyboard/pointer
`KWin::InputDevice` and register it with KWin input redirection. The versioned,
bounded `InjectTestInput` method emits only the documented absolute pointer,
left button, and small fixed keyboard allowlist through the ordinary input
chain; shutdown releases held state before removing the device. Production
does not construct the injector, and its gate-before-parse reply is identical
for malformed and oversized input. This is a deterministic nested-session seam,
never a public automation API or physical-device claim.

The notification live workflow also joins the filtered compositor-owned
`DevelopmentShellSurfaces` view with a separately PID-authenticated, read-only
shell snapshot. The compositor view contains only QindaQt's two notification
scopes, while the shell view contains bounded presentation/window/focus state
and no mutation methods. Both are admitted only inside the same explicit
private development session. The shell fails its development startup if it
cannot match Compositor1 to the supervisor-provisioned KWin PID and live
development capabilities. [ADR-0020](../adr/0020-authenticate-private-live-evidence.md)
records this qualification-only boundary.

Output observation is owned by one GUI-thread inventory collaborator. It
samples KWin's semantic output order and complete stable field set, validates
the whole projection against the shared shell-visibility bounds, and advances
one decimal-string generation only when that canonical projection changes.
`Outputs` returns the retained generation. Shell visibility copies outputs from
that same immutable generation and carries its `outputGeneration`, so the two
surfaces never race independent live Workspace samples. Invalid transitional
samples retain the prior complete projection and emit no false generation.

Virtual output mutation has a stricter construction gate than the other test
methods because KWin's public `OutputBackend` ABI has no capability query. The
launcher clears inherited backend proof and sets it only for an explicit
scenario using the exact virtual backend. Only then does the plugin construct
the adapter or advertise the two typed methods. Requests validate bounded ASCII
names, logical dimensions, scale, owned count, total count, and both KWin's
requested and `Virtual-`-prefixed name forms before creation. Ownership is the
requested name mapped to KWin's exact returned `QPointer`, never connector-name
discovery. D-Bus is unpublished before teardown removes only those live owned
outputs synchronously. A non-virtual development session gets the same
pre-parse `control-disabled` response as production.

That same development gate exposes the no-input
`ReinitializeCompositingForTest` method. It queues
`KWin::Compositor::reinitialize()` for the next event-loop turn so the D-Bus
reply can return before synchronous scene teardown begins, then replies
`status: "scheduled"`; a missing compositor/reinitializer instead returns
`compositor-reinitialize-unavailable`. The nested workflow separately observes
the compositor's inactive-to-active transition and requires the same live
Hybrid revision/container with one visible anchored scene item afterward.
Admission alone is not restart evidence, and production never enables this
test seam.

Internal lifecycle cleanup uses a non-scriptable release path and is not
blocked by the external gate. On KWin `UnloadPlugin`, the plugin copies all
published legacy container IDs, disconnects queued close reconciliation,
releases each group while scene collaborators are alive, and only then
unregisters D-Bus. Process-local Hybrid groups follow the earlier shutdown
ordering described above.

`Capabilities` also carries an optional `hybrid` diagnostic object with runtime
readiness, input-filter installation, all-shortcut registration, a decimal
string session topology revision, container count, reconciled/visible/anchored
scene-chrome counts, persistent context-quarantine count, verified group-stack
publication count, and the current stack failure diagnostic. These are live
coherence values: failed raises revoke their affected publication, and a
successful complete synchronization clears the failure string. `Containers`
publishes each process-local group at that actual revision with
`authority: "hybrid-process"`; `Snapshot` returns its schema-1 model. These are
read-only observations, not a public mutation authority. The exact fields and
authority distinction are in the
[Compositor1 reference](../reference/compositor-control-v1.md).

## Qualified integration matrix

The Debug and Release presets each pass the complete 40-test suite. Focused
live coverage includes:

- mapped `QBackingStore` Wayland windows and reachable rootless XWayland;
- Weston 15 headless as the parent of `qindaqt-wm --windowed`;
- virtual 1920x1080, 1920x1200, 2560x1440, 1920x1080 at 1.25 scale, and two
  common 1920x1080 outputs;
- exact KWin ABI, installed-plugin discovery, method/signal descriptor parity,
  active non-consuming input observer, output generation/projection parity,
  and production read-only mutation rejection;
- three clients through dock revision 1, page creation revision 2, page
  activation/reactivation revisions 3–4, third-member detach/restore revision
  5, singleton unwrap/restore revision 6, then redock and explicit release;
  and
- four clients in two containers dynamically unloaded through KWin's
  `/Plugins.UnloadPlugin`, followed by independent `/KWin.getWindowInfo`
  verification of every exact frame/minimized state and a client retitle/resize
  usability check.

The main plugin workflow and unload workflow each pass ten consecutive final
stress repetitions. Scenario parsing explicitly rejects heterogeneous common
modes/scales, transforms, and fractional dimensions that cannot produce
integral logical extents.

## Completed Hybrid qualification

Focused tests cover the complete semantic command set, including complete-page
move/detach/regroup and one-member page extraction; atomic candidate publication;
recursive constraints and restore state; exact modifier and keyboard parity;
Qinda macOS painting; the ordinary compositor router; scene-image lifetime and
anchor exposure; scene/chrome agreement; stack/activation/output
synchronization; whole-group context adoption and its outer-title menu; member
focus mode; transient admission/following; lifecycle focus preservation;
compositor scene teardown/rebuild; KWin rollback; group placement and close
policy; deterministic development input; and release-all continuation. The
KDecoration factory loads in a focused test; staged
installation verifies its artifact and first-run default. Qualification results
for this expanded set are tracked separately below.

The live pointer workflow drives exact `Meta+Shift+Left` through KWin's normal
input chain, creates one process-local split, observes one shared owner and valid
divider geometry, and reads the actual nonzero Hybrid revision and schema-1
snapshot. While the group is still live, the workflow proves compositor
reinitialization restores the unchanged Hybrid revision/container with one
visible anchored scene item, a covering ordinary window blocks shared input,
the popup-dismiss press does not fall through, and a focused normal-type
transient stays outside topology. A plain native KDecoration member-title drag
then detaches at KWin's interactive-move start. The dragged member preserves its
independent size and follows the pointer to the drop, while the sibling returns
to its exact original current and target frames and every owner clears. The
moved member is intentionally not required to return to its original position.
The host uinput device was not admitted by the virtual seat, so the workflow
records that failure and uses the development-only KWin input device; it does
not claim physical input coverage.

A separate four-client workflow creates the same Hybrid-owned group, verifies
three live mapped decorations are `QindaDecoration` instances, unloads the
plugin through KWin, and requires the plugin, service, and Hybrid authority to
disappear. KWin must then report exact original frames and non-minimized state,
and all clients must remain exposed while one is retitled and resized. Offscreen
tests prove dock-preview/chrome rendering; the live workflow functionally
exercises compositor and native-decoration input but is not a screenshot
baseline.

The explicit focused and live selectors define the Hybrid boundary; the shared
registry total also contains later-milestone Shell tests. Final Debug/Release,
focused Hybrid, bridge-only, applied virtual-display, production read-only,
live menu/unload, sanitizer/stress, documentation/source, and independent-audit
results are recorded in the
[testing harness](../development/testing-harness.md). Together they complete
Hybrid interaction.

## Post-Hybrid milestone boundaries

Hybrid interaction is complete. The following remain explicit later-milestone
boundaries:

- persisted topology/session restore and live mixed-output DPI migration remain
  later compositor, Platform, or Release work rather than claims of this slice;
- shell panels/docks, global menu, notifications, applet rendering, and direct
  customization belong to **Shell and customization**;
- heterogeneous topology application, display mutation, rotation/hotplug/lid
  behavior, and platform device policy belong to **Platform services**; and
- physical DRM/GPU/input/tablet coverage, suspend/resume, performance/memory,
  packaging, migration, recovery, and upgrades belong to
  **Release qualification**.

These are explicit next boundaries, not unreported Compositor MVP failures.
The complete behavioral contract remains in
[Window containers](window-containers.md), and test details are maintained in
the [development harness](../development/testing-harness.md).
