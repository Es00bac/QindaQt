# Public ordinary Windows application runner

`QindaQt::WindowsRunner` is a Qt Core launch coordinator for registered ordinary
Windows applications. It has no dependency on QindaLutris game records, store
clients, shell internals or compositor objects. Its fixed Wine prefix and Proton
environment rules follow the existing QindaLutris launch planner, with a strict
managed recipe boundary described by
[ADR-0366](../adr/0366-coordinate-ordinary-windows-launches-through-public-ports.md).

## Registration and launch boundary

A Windows binding contains a canonical non-null lowercase registration UUID,
a positive registration revision and a 64-character lowercase recipe SHA-256.
The stable Applications ID is `qindaqt.managed.windows.<UUID>`; a relaunch or
recipe revision retains this ID. The runner's compact Windows-only binding is
independent of catalog parsing. A catalog adapter must admit the public typed
Windows origin before conversion; a desktop file, executable name or window
class cannot mint that origin.

The registration port returns a copied admitted recipe and materialization
receipt. Catalog values contain no executable path, argv, prefix or environment.
The runner refuses stale revision/digest bindings, missing or unready profiles,
unbounded/control-containing arguments and malformed runtime pins. It plans
one absolute program plus literal arguments, with no shell interpolation or
silently truncated input. A Proton/umu recipe requires its exact packaged Proton
entrypoint and Steam Runtime receipt. There is no rolling-build discovery,
game-store identity, downloader or runtime fallback.

The runner starts with an empty environment and copies only current-session
display, locale and audio transport fields. It fixes the profile home/XDG
directories and Wine prefix. Proton plans own `PROTONPATH`, `GAMEID=umu-0`,
`STORE=none` and `UMU_RUNTIME_UPDATE=0`. Loader, Python and runtime override
variables cannot leak from the inherited session or catalog metadata.

## Payload and process ports

Three constructor-visible borrowed ports outlive the same-thread coordinator:

- The registration port resolves current trusted recipe/profile state.
- The payload port verifies root-owned package provenance, exact payload
  size/SHA-256 and the private materialization receipt through stable handles.
  Its lease holds those handles and detects replacement through dispatch.
- The launch port consumes the checked handles and the complete literal plan.
  It submits exactly once and reports Accepted, Refused or OutcomeUnknown.

The runner compares every leased pin, checks freshness, then re-resolves the
entire recipe after payload inspection. Removal, update, a changed path under an
unchanged revision, or payload replacement blocks dispatch. An accepted spawn
reports **Starting**. It does not report Running, a visible window, successful
input, normal app close or audio rendering. A lost/uncertain dispatch reports
OutcomeUnknown and is never replayed automatically. A broker must enforce its
operation-ID ledger before calling this lower-level coordinator.

Ports perform blocking filesystem/runtime work off a GUI thread. The core
owns no process scope, force-stop command, deletion authority or shared-runtime
termination. Data and peer applications remain intact on all refusal paths.
Per-app close/relaunch and compositor association use the separate
[foreign runtime lifecycle contract](foreign-applications.md).

## Delivery and acceptance boundary

This slice implements the public planning/coordinator target, installed public
headers and deterministic focused tests with injected ports. Production
Portage recipe loading, stable-handle payload adapters, a real Wine/umu process
adapter and the catalog/session dispatch composition remain subsequent slices.
The Proton process adapter must refuse until it can demonstrate that the leased
Steam Runtime/Proton tuple is exactly the one it executes.

Run `cmake -S tests/windows_runner -B .cache/windows-runner-build -G Ninja
-DCMAKE_BUILD_TYPE=Release -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`, build that focused
tree once, then `ctest --test-dir .cache/windows-runner-build --output-on-failure`.
Tests cover stable identity, stale registrations, payload replacement, update
during inspection, literal arguments, inherited override removal and uncertain
dispatch without automatic replay. Test processes have a 30-second cap and do
not start Wine, alter a host profile or use the live session.

The previous private Notepad/WordPad diagnostic is separate from this target.
The October 9 attempt was canceled at the user's process-change request; its
short incomplete result was preserved and its owned unit settled. It qualifies
no user journey. The full launch/type/save/resize/independent close/relaunch
journey, authenticated Windows blue member identity, public Audio1 integration
and standard Win32 HMENU Global Menu remain required. The
[Global Menu capability contract](foreign-applications.md#windows-application-global-menu-capability)
preserves same-process PID/focus/epoch authority and the local menu until hosted
acknowledgement; this runner does not implement or claim it.
