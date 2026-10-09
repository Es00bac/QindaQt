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
independent of catalog parsing. Its public catalog bridge admits the typed
Windows origin before conversion and reuses the current-recipe/payload gate; a desktop file, executable name or window
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

## User-selected local Windows programs

The public `catalog_binding.h` bridge consumes the shared typed Windows binding and keeps Android/native origins out of this runner. The distinct `executable_file.h`/`executable_open.h` boundary admits a user-selected local PE program without manufacturing Portage guest-recipe provenance; [ADR-0370](../adr/0370-admit-user-selected-windows-programs-through-leased-pe-files.md) records this owner-requested extension.

The inspector reads at most8KiB of bounded DOS/PE/COFF/optional/section headers, accepts supported executable architectures and desktop subsystems, and refuses DOS stubs, DLLs, scripts, bad offsets and incomplete data. Its lease retains the exact descriptor and device/inode/size/mtime/ctime. Both File Manager double-click and Open With use the shared route, preserving current native user defaults and an explicitly chosen native handler. A Windows choice never falls back after submission or uncertainty.

The injected setup/profile port owns the file lease, real visible first-run setup and per-app prefix/data. It must record user-file provenance, distinguish portable/installer choice explicitly, and transactionally hold current registration/profile authority through actual dispatch. Refused/default fallback means no operational side effect; any post-submission uncertainty is OutcomeUnknown. The core copied recheck cannot close that final race by itself.

This source slice implements format/identity admission and both route adapters. The concrete [Windows Program Runner](../apps/windows-program-runner.md) now supplies worker-only profile/receipt persistence, visible setup, checked-FD native dispatch and its paired packaged MIME handler. Its injected source smoke does not start Wine; actual launch and installed-user qualification remain separate. File format admission alone never grants a blue window association, file grant, Audio1 control or menu export.

## Delivery and acceptance boundary

This slice implements the public planning/coordinator target, installed public
headers and deterministic focused tests with injected ports. Production
Portage guest-recipe loading and managed session/catalog producer composition remain subsequent slices. The user-file handler implements its own concrete Wine process/profile adapter; a complete Proton/umu adapter remains subsequent work.
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
