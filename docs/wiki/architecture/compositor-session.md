# Compositor and session integration

The Compositor MVP milestone is **complete**. QindaQt launches a real KWin
Wayland compositor, loads an exact-release native plugin, owns rootless
XWayland, reports output/input state, and commits container model plus live
scene changes atomically. This is the qualified compositor substrate, not yet
the finished hybrid interaction described by the
[container model](window-containers.md).

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
A `--session` process controls compositor lifetime.

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
then requires the installed plugin's live service and exact ABI.

## Runtime layers

The KWin plugin provides:

- a registry of normal non-internal windows keyed by KWin's internal UUID;
- ordered logical output inventory with stable transform names, exact scale,
  numeric mHz refresh, and change invalidation;
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

## Control security mode

The service uses the ordinary user session bus and has no caller
authentication. Production therefore advertises `controlMode: "read-only"`.
`DockWindows`, `Submit`, and `ReleaseContainer` reject with
`control-disabled` before parsing or mutation. Inventory and snapshots remain
readable. Only the isolated explicit scenario path enables the
`development-test` mutation mode. Production hybrid gestures will call typed
process-local policy instead of enabling D-Bus mutation.

Internal lifecycle cleanup uses a non-scriptable release path and is not
blocked by the external gate. On KWin `UnloadPlugin`, the plugin copies all
published container IDs, disconnects queued close reconciliation, releases
each group while scene collaborators are alive, and only then unregisters
D-Bus and tears down.

## Qualified integration matrix

The Debug and Release presets each pass the complete 40-test suite. Focused
live coverage includes:

- mapped `QBackingStore` Wayland windows and reachable rootless XWayland;
- Weston 15 headless as the parent of `qindaqt-wm --windowed`;
- virtual 1920x1080, 1920x1200, 2560x1440, 1920x1080 at 1.25 scale, and two
  common 1920x1080 outputs;
- exact KWin ABI, installed-plugin discovery, method/signal descriptor parity,
  active non-consuming input observer, and production read-only mutation
  rejection;
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

## Boundary of this milestone

The following are intentionally assigned to later milestones:

- shared outer decoration, preserved member drag regions, consuming
  pointer/keyboard grabs, split/tab/divider manipulation, client constraints,
  focus/maximize/fullscreen/workspace state, and richer restoration belong to
  **Hybrid interaction**;
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
