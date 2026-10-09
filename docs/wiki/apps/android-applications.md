# Android Applications

Android Applications is an ordinary QindaTK desktop window for browsing supplied
Android registrations, reading current availability and requesting launches.
Its native desktop entry is `org.qindaqt.AndroidApplications`; it is a management
utility, not a replacement generic entry for every guest application. The entry
currently has `NoDisplay=true` while the executable injects a null port, so the
next desktop package cannot advertise a dead-end management app. The binary
remains available for source/UI qualification. Enable menu discovery only after
an admitted setup/runtime composition offers actionable setup or recovery.
[The shared catalog](../shell/launcher.md#managed-application-catalog-values)
owns stable Android/Windows identities. The management boundary is recorded in
[ADR-0369](../adr/0369-manage-android-applications-through-a-borrowed-runtime-port.md).

## Current implementation boundary

The application, Qt Core controller and public borrowed `RuntimePort` exist in
`src/apps/android_manager`. The production executable currently has no admitted
Android registration/runtime adapter and injects a null port. It shows Android
support unavailable, with no fabricated stock application rows or enabled
launch/refresh actions. A separate borrowed SetupPort now offers runtime/image
package selection in QindaPortage when its protected installed executable is
available. The native menu entry remains hidden until actionable production
setup/runtime composition is qualified. The owning adapter, materialization, package
updates/removal, data preservation workflows, real per-app icon provider,
registered launch execution and runtime recovery remain subsequent slices.
No Waydroid process is started by this application or its tests.

A supplied port owns source authentication, packaged recipe/materialization
verification, current-session/attachment freshness and actual runtime facts.
The controller borrows it on the same GUI thread. The port returns copied,
strictly validated snapshots and accepts nonblocking typed requests; it exposes
no private paths, shell commands, arbitrary executable URLs or storage objects.
This is a C++ presentation port, not a new D-Bus/wire/persistence schema.

## Catalog and status

One validated complete snapshot supplies Android-only registrations plus one
matching status per registration. Positive revisions, canonical attachment UUID,
exact registration/revision/recipe bindings, closed enums, bounded UTF-8 messages,
duplicates and counts are checked before publication. Malformed data publishes
no partial rows. A lower revision or changed same-revision snapshot withdraws
truth until a fresh valid snapshot arrives.

Runtime readiness, application state and observed window count remain separate.
Ready means launch admission, not a running guest or mapped surface. Running
can have no observed window; an absent count is unknown and is not painted as
zero. Only the supplied port changes these facts. The controller uses the public
catalog builder/search ranker, so names, keywords, sort and stable UUID identities
stay consistent with ordinary Applications values.

The QindaTK window offers search, Refresh, runtime status/feedback and per-app
Launch, plus Runtime/Images package setup selection through the independent
SetupPort. Accepted setup handoff changes neither runtime nor registration truth. Each supplied Android registration has a green toolkit semantic tint
and explicit Android text/accessible description, separate from selection,
error and runtime status. This badge describes catalog origin only. It cannot
classify a running window from a title, app_id, alias or PID. The window keeps
normal native geometry and closing; QindaQt's compositor owns any later
container/tab/tile behavior, never this QML.

## Launch and recovery truth

The controller serializes one pending launch. It sends a fresh operation UUID,
exact attachment/snapshot revision and current managed application binding. The
port must recheck the current registration revision and exact recipe digest
before dispatch. The controller sets pending before calling the port, allowing
a synchronous refusal/result without losing it. Accepted requests show Launching
and disable other launches; they never set Running or invent a window count.

Owner or recipe replacement/remove withdraws a dependent pending request.
Late/foreign operation results cannot affect a fresh request. A terminal result
requests read-only refresh; it does not repeat launch. The 30-second deadline
reports an unconfirmed outcome without claiming cancellation or stopped state.
New explicit requests are independent operations. Port destruction clears all
rows and admission. Setup/install/delete/force-stop are not implied by Launch.

## Build and short evidence

`QINDAQT_BUILD_ANDROID_MANAGER` defaults to the existing QindaTK Viewer switch,
so configurations without that dependency remain available. The executable and
native entry install as component `AndroidApplications`; the static public
`QindaQt::AndroidManager` library owns controller/port headers. An ordinary
Portage release build and component/package installation remain release gates.

```sh
cmake -S tests/apps/android_manager -B build/android-manager -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build build/android-manager -j2
ctest --test-dir build/android-manager --timeout 10 --output-on-failure
```

The three short gates cover controller inventory/search/failure/owner/recipe/
timeout behavior, offscreen QindaTK rows/input and the actual compiled executable
with `--smoke`. All use private temporary data/runtime roots, unavailable buses,
offscreen software rendering and fatal Qt warnings. UI captures show explicitly
synthetic registration values, not running Android apps. Test item lookup uses
the visual child tree and observes fresh delegates after ListView polish;
QObject ownership and a retained pre-rebuild row are not display authority.

## Installed stock runtime interface findings

Source-only inspection of laptop `app-containers/waydroid-1.6.3` found that
`tools/actions/app_manager.py::list` calls container Unfreeze/Freeze around
`IPlatform.getAppsInfo`. It is not an observational polling API and must not
back automatic status refresh. Its launch path can start a session and alter
active-app/immersive properties. A Binder launch transaction's acknowledgement
is not Running, a mapped surface or an application-window ownership proof.

`IPlatform.getAppsInfo` supplies label/package/action/component/category fields;
it supplies no signer, UID, task or surface mapping. Registration needs separately
verified package/materialization truth; live app/window facts need the admitted
runtime/producer. Unknown process/window counts must remain unknown.

The exact inspected packaged source SHA-256 values were:

| File beneath `/usr/lib/waydroid` | SHA-256 |
| --- | --- |
| `tools/actions/app_manager.py` | `8b4f4e5db8ff74514f69687215d1a5c00c9e163c20798b9d44b9004f618ddd8e` |
| `tools/interfaces/IPlatform.py` | `e9016076ba33ec811534ffd64851be43b009b8f83c6e03c044d8cecb19607d49` |
| `tools/actions/session_manager.py` | `9ae4b0d8cda39b16b534e178b16461e277fbf9741e9f043202d8609749d33dba` |

No Waydroid command, live Binder call, session start, mount or installation ran
for this inspection. QindaPortage's current native `--page packages --select
<category/name>` handoff can open a package selection without using its separate
`--ask` model action; its own GUI retains plan preview/approval. That native
handoff does not install or materialize an Android app by itself.

## Graphical package setup handoff

[ADR-0371](../adr/0371-delegate-android-package-setup-to-qindaportage.md) adds a
separate borrowed SetupPort. Runtime and Images are closed enum targets, not
arbitrary atoms/URLs or managed launch commands. The existing QindaPortage GUI
opens its Packages page with the fixed runtime/image package selected. Its
preview, user approval and privileged helper remain installation authority;
no --ask model request, automatic apply or Waydroid command is sent.

The production composition owns a dedicated native setup adapter before the
controller and checks only the fixed protected Software Manager executable.
Process/filesystem reach is confined to that target. Session/display/locale
values are forwarded without loader and model-development overrides. Unknown
setup targets, unavailable/refused software-manager dispatch and owner loss
produce plain feedback. Accepted GUI dispatch is only a handoff and is never
Android Ready/Running, package materialization or a mapped window.

The desktop entry remains `NoDisplay=true`: the runtime/registration port is still
null and no installed end-to-end setup/runtime journey is qualified yet. Manual
opening now has an actionable package-selection route where QindaPortage exists.
The three short gates include closed target/refusal/owner-loss/no-fake-registration
controls plus actual offscreen setup-button input. They use an injected native
planning port and never launch QindaPortage or a guest during verification.

## Private producer transport

[ADR-0372](../adr/0372-bind-private-android-producers-to-kernel-lifetimes.md) moves
RuntimePort/types/validation to the Qt Core service protocol. The app's previous
header aliases the same types; static consumers rebuild. The new service
RuntimePort receiver/helper ProducerEndpoint use an unnamed private descriptor,
per-packet kernel sender PID/UID, held pidfd/executable object and a private
bootstrap key. They never identify a producer from a bus name or metadata.

A trusted supervisor must first admit the installed native executable and its
session/runtime/recipe context. This transport proves the provisioned process
and its current lifetime; it does not collect guest signing identity, task/
surface ownership or window evidence. The production management composition is
still null-runtime and its desktop entry remains hidden. No public fd/PID argv
option or automatic helper start is exposed.

Packet size/field/array bounds and exact version/direction are enforced before
publication. Death/exec change/getter checks fence stale cached truth before Qt
notifications arrive. Runtime/registration replacement invalidates dependent
operations; same-ID requests never replay and a retired attachment cannot return.
Owned-fork tests demonstrate kernel credentials/lifetime on inert synthetic
publishers, not running Android apps. Supervisor/verified guest collector and
renderer/window admission are the next concrete product layers.
