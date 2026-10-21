# ADR-0428: Pause controller desktop input only for the focused application

- Status: Accepted
- Date: 2026-10-21
- Owners: Controller runtime and compositor integration
- Supersedes: [ADR-0347](0347-controller-desktop-integration.md), only its clause that any other same-user open of a related input/js/hidraw node suppresses desktop output
- Superseded by: None
- Related: [ADR-0397](0397-observe-controller-consumers-off-the-compositor-thread.md) (observation threading and invalidation, kept as is); [ADR-0426](0426-dualsense-desktop-layout-and-held-actions.md); [ADR-0427](0427-share-programmable-input-actions-and-controller-layers.md)

## Context

A Bluetooth DualSense was connected and SDL had discovered it, yet Controllers1
reported `reason: "game"` and desktop control stayed off. The only other process
holding its nodes (`/dev/hidraw4`, `/dev/input/event17`, `/dev/input/js0`) was
`qindastream`, a QtWebEngine video app with no gamepad code of its own.
Chromium's gamepad service (Firefox's and Electron's behave the same way) opens
every attached gamepad once any web page touches the Gamepad API and keeps it
open for the life of the process. ADR-0347 treats any same-user process holding
a related node as a game, so a browser or web-engine app, focused or not,
disabled desktop control for as long as it ran.

The defect is the rule, not its timing. Reconnecting the pad or restarting the
app hides it only until the next page touches the Gamepad API. Two facts
separate a game that must be left alone from the holders ADR-0347 caught:

- A web engine's hold is passive. It is opened for web content that may never
  read it, and says nothing about what the user is doing.
- Desktop input can only leak into the application that receives it, which is
  the one that owns the focused window. A holder in another window, or a
  background daemon, receives nothing from desktop control.

## Decision

A controller's desktop output is suppressed as `game` only when a holder of one
of its device nodes is **(a)** not a passive web-engine host and **(b)** part of
the application that owns the currently focused (active) window. A background
holder or a passive web-engine holder suppresses nothing. Steam priority, the
locked/inactive-session gates, the profile `enabled` switch and the
neutral-controls re-arm rule are unchanged.

**(a) Passive web-engine host.** The holder's `/proc/<pid>/maps` contains any of
`/libQt6WebEngineCore.so`, `/libxul.so`, `/libcef.so` or `icudtl.dat`. Chromium,
Electron, NW.js, QtWebEngine and CEF all map their ICU data file; Gecko links
ICU into libxul. Maps are read only for processes already found holding a
watched node, in bounded chunks, and are never followed through a link. A
holder whose maps cannot be read, or exceed the 4 MiB bound, is unclassified and
is treated as a possible game, never as passive.

**(b) Part of the focused application.** The compositor plugin reports the
active window's client process (`Window::pid()`, absent for compositor-internal
windows) on every activation. A holder H is part of the focused process F's
application when any of these holds:

1. H is F.
2. One is an ancestor of the other, following `/proc/<pid>/stat` parents for at
   most 32 hops. The walk stops at pid 1, at the compositor and at any process
   whose `/proc` directory is not owned by the user. This covers gamescope and
   launchers.
3. They share a session id (stat field 6) that differs from the compositor's.
   The shell starts applications with `QProcess::startDetached`, so each gets
   its own session and its children inherit it; services spawned by
   `qindaqt-session` share the compositor's session, which therefore never
   counts.
4. They share a cgroup v2 path (the `0::` line) that is not a login-session
   scope (`session-*.scope`), not the root and not the compositor's own.
   QindaLutris runs games in `systemd-run --user --scope` units and umu/Proton
   calls `setsid` inside them, so the session id alone misses them. On a desktop
   where every application shares one login scope this rule never fires and
   rules 1-3 and 5 decide.
5. Both are Wine processes (comm ends in `.exe`, or the process image or
   mappings are `wine`, `wine64`, `wine-preloader` or `wine64-preloader`) with
   the same `WINEPREFIX` from `/proc/<pid>/environ`, an unset value being the
   user's default `~/.wine`. winebus holds the HID nodes in `winedevice.exe`,
   whose detached services have their own session, while the game's `.exe` owns
   the window. Wine truncates comm to 15 bytes, so a long image name is
   identified by its loader instead.

A relation that needs an unreadable fact does not exist. The compositor's own
window never owns a game.

**Observation, focus and invalidation.** All `/proc` reads stay on the ADR-0397
worker. A scan takes the focused pid and the compositor's own session and cgroup
(`getsid(0)` and `/proc/self/cgroup` in production, injectable in tests). The
state keeps `busyPaths` (every watched node held by another same-user process,
informational) and adds `gamePaths` (the held nodes that suppress),
`consumerPresent` (some holder is not a passive web-engine host, focused or not)
and a sanitized name per `gamePaths` entry. The runtime pauses a controller only
for `gamePaths`.

A focus change always requests a fresh scan with the new pid, but it invalidates
(releases held desktop input and waits for the result) only when the last
published observation had a non-passive holder. With none, a focus change cannot
alter the verdict and never releases input, so a held Meta-drag that activates
another window keeps working. With one, focusing its application stops desktop
output before the application can receive leaked desktop input, and focusing
away resumes only after neutral controls. A scan computed for another focus is
kept when it names no non-passive holder and rescanned when it does. Generation
fencing, the single in-flight scan, coalescing and joined teardown are as
ADR-0397 decided.

**Controllers1.** `reason` keeps its values; `game` now means the focused
application is reading the controller directly. A controller whose reason is
`game` may carry an optional `consumer` string, at most 32 letters, digits,
spaces and `. _ - +`. The field is additive; `schemaVersion` stays 2 and
consumers that ignore it keep working.

## Consequences

A browser, a web-engine app, a game in another window and background daemons no
longer disable the pad, and desktop control no longer depends on reconnecting
the pad or restarting an app that touched the Gamepad API.

Accepted trade-offs:

- A browser, Electron, NW.js or CEF game that reads the pad through the Gamepad
  API shares the controller with desktop control while it is focused. Turn the
  profile's desktop control off in Settings for that controller.
- A background remapper daemon that holds the nodes is not recognized as a
  consumer, because nothing it does is the focused application's input.
- The relation rules are heuristics over process facts. An application whose
  processes share neither a private session, a private cgroup, an ancestor nor a
  Wine prefix with its window process is not related; a window whose pid the
  compositor cannot report (an X11 client without XRes) cannot own a game.
  Conversely, a deployment that gives every application one shared service
  cgroup or session other than the compositor's, or several applications started
  from one terminal session, relates them to one another.
- An unreadable holder is not passive and an unreadable focused process relates
  only by identity.

A focus change costs one extra observation scan; while a non-passive holder is
known, the controller briefly reports `checking-game` until it returns.

The focused suite adds fake `/proc` trees for each relation rule, the compositor
exclusions and read bounds, observer cases for invalidation and stale-result
handling, SDL runtime cases for the full focus lifecycle, and the Settings text.
The wiki's [controller input](../architecture/controller-input.md) page owns the
current rule.

## Revisit when

The compositor or SDL can report which client reads a gamepad, which would
replace the process-tree heuristics; launchers publish a per-application cgroup
for every application, which would make rule 4 sufficient; or a web-engine game
needs desktop control and the Gamepad API can be told apart from a passive hold.
