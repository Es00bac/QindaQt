# Android Applications

Android Applications is an ordinary QindaTK desktop window for browsing supplied
Android registrations, reading current availability and requesting launches.
Its native desktop entry is `org.qindaqt.AndroidApplications`; it is a management
and setup utility. The ordinary full-desktop entry stays `NoDisplay=true`.
A focused Portage component may expose its generated setup entry only with the
QindaPortage runtime dependency. Setup/status is actionable while guest apps
remain Unavailable; helper startup never implies Android apps run.
[The shared catalog](../shell/launcher.md#managed-application-catalog-values)
owns stable Android/Windows identities. The management boundary is recorded in
[ADR-0369](../adr/0369-manage-android-applications-through-a-borrowed-runtime-port.md).

## Current implementation boundary

The QindaTK app/controller borrow the public AndroidRuntime protocol port.
The production executable now starts the fixed package-owned collector only
after protected executable/recipe and current Session1 process admission,
then injects its credential-admitted port. Missing package/session context
keeps a null port with plain recovery feedback. The helper itself reports
Unavailable with no guest rows until qualified image/package/task facts exist.
A separate SetupPort offers Runtime/Images package selection in QindaPortage.
Materialization, update/removal/data preservation, app icons, actual registered
launch execution and runtime recovery remain subsequent slices. No Waydroid
process is started by this application or its tests.

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

The ordinary desktop entry remains `NoDisplay=true`. ADR-0375 adds the concrete
native helper and permits a focused dependency-qualified setup menu entry;
qualified guest registrations and Android launches remain unavailable.
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
now uses ADR-0375 admission with null fallback; guest facts stay unavailable. No public fd/PID argv
option or automatic helper start is exposed.

Packet size/field/array bounds and exact version/direction are enforced before
publication. Death/exec change/getter checks fence stale cached truth before Qt
notifications arrive. Runtime/registration replacement invalidates dependent
operations; same-ID requests never replay and a retired attachment cannot return.
Owned-fork tests demonstrate kernel credentials/lifetime on inert synthetic
publishers, not running Android apps. Supervisor/verified guest collector and
renderer/window admission are the next concrete product layers.

## Native renderer evidence boundary

[ADR-0373](../adr/0373-separate-android-renderer-origin-from-guest-app-identity.md)
adds `QindaQt::AndroidRendererEvidence`, a display-free compositor boundary for
fresh native Wayland kernel PID/window-incarnation facts and trusted renderer
role evidence. Its descriptor adapter accepts only an already qualified producer
that is itself the native renderer. A separate app-catalog broker's credentials
cannot classify another process's windows. No title/app_id/argv matching is used.

The copied result identifies the host platform/runtime/renderer, with no AppKey,
package, guest UID, task, surface mapping, stop permission or app window count.
The private port must have authenticated a packet and retain its exact live
pidfd/executable; runtime attachment/status and the process fence are rechecked
on observation. Death or replacement withdraws evidence before queued callbacks.
A refused or unavailable result remains unknown. Consumers must observe fresh
and never reuse a cached result as app authority.

The short focused checks use inert owned forks and verify credential admission,
non-native and mismatched provider/PID refusal, process death and runtime
withdrawal/replacement. They do not launch Android or claim a guest window.
Existing KWin registry/publisher, supervisor and production composition are not
wired by this isolated source slice. The next product gate is a separately
admitted renderer bootstrap plus authenticated guest package/task/surface
association, followed by exact ordinary-window publication and mixed native/
Android container coverage. The focused native setup entry is separately gated by its QindaPortage dependency.

## Separate collector and renderer lifetime

[ADR-0374](../adr/0374-bind-separate-android-renderers-to-held-lifetimes.md) adds
an AndroidRuntime holder for an independently admitted renderer pidfd/executable
object and a copied observer joining it to the current authenticated collector
runtime attachment. The two processes can have different host PIDs; collector
inventory never substitutes its PID for a window's native kernel client PID.
Both process fences are rechecked on observation. Renderer death/exec change,
collector channel/death/exec loss or runtime withdrawal/replacement returns
unknown; an observed renderer loss permanently retires its holder.

The new KWin-only peer helper reads only a live non-internal managed native
WaylandWindow incarnation and wl_client kernel PID, and passes the split binding
through the existing public evidence gate. It retains no window pointer and
uses no title/app_id/PID-property match. The isolated native object is compiled
against the exact installed KWin release; no live compositor is started by the
focused tests.

The result still has no guest AppKey, package, UID, task, surface mapping, app
count or control permission. Existing registry/publisher/supervisor do not call
the helper yet. Trusted production bootstrap, verified materialization and
an authenticated guest package/task/surface collector remain the next product
gates before normal window identity, green badge and mixed native/Android
QindaQt containers. ADR-0375 connects only the conservative native helper; guest and renderer facts remain unavailable.

## Package-owned native bootstrap and component

[ADR-0375](../adr/0375-bootstrap-the-package-owned-android-collector.md) supplies
the actual fixed collector executable/recipe and current Session1 code/lifetime
admission. The manager creates private descriptors/key, starts the pinned helper
and injects its port only after a valid kernel-credential packet. Its copied
status stays Unavailable/no apps. Parent/context/recipe loss fences getters and
requests; only the explicitly owned child is retired. No guest polling or
activation is performed. Unchanged refresh observations deduplicate naturally.

Package using the product CMake root, never the tests or full desktop tree:

```sh
cmake -S src/apps/android_manager/component -B build/android-component -G Ninja \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBEXECDIR=libexec \
  -DQINDAQT_ANDROID_SETUP_MENU_ENABLED=ON
cmake --build build/android-component -j2
DESTDIR="$PWD/build/android-component-stage" cmake --install build/android-component \
  --component AndroidApplications --prefix /usr
```

The visible setup option requires the focused Portage package to RDEPEND on
`app-portage/qindaportage` and QindaTK. Full-desktop builds retain their hidden
entry and separate file ownership; component package adoption must not replace
paths owned by an installed desktop image. The exact four installed files are
`bin/qindaqt-android-applications`, `libexec/qindaqt/qindaqt-android-collector`,
`share/applications/org.qindaqt.AndroidApplications.desktop` and
`share/qindaqt/android-runtime/collector.json` beneath `/usr`.

Short source checks execute the real built native helper with private sealed
records, verify recipe identity and untrusted/public-mode refusal, and inject the
admitted copied port into the controller without guest rows. Source staging is
not a system/package install. Signed Portage adoption and a real native setup
menu journey remain installed gates; Android app/window/container/audio proof
remains beyond this conservative helper version.
