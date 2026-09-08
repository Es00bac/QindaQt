# QindaQt.AppShell 1.0

`QindaQt.AppShell 1.0` is the narrow participation shell for first-party QML
applications. It standardizes application-owned lifecycle decisions, stable
action/menu projection, opt-in global-menu export, optional Settings/session readiness, file-portal request
mediation, bounded typed errors, focus reporting, and one accessible QST-themed
window surface. Native menus and dialogs inherit the same semantic palette
and font as custom content, including live theme updates. It is not a route registry, domain framework, service client,
or process supervisor.

The durable extraction decision and prohibited responsibilities are recorded
in [ADR-0027](../adr/0027-extract-a-narrow-first-party-application-shell.md).
The opt-in transport composition is recorded in
[ADR-0068](../adr/0068-compose-first-party-menu-export-through-appshell.md).
[QST-1](../architecture/design-tokens.md) and
[QindaQt.Controls 1.0](../shell/controls.md) remain the presentation authorities.

## Ownership and dependency boundary

The owning application creates one `ApplicationCoordinator` on its GUI thread
and retains it for the primary-window lifetime. The coordinator owns only
bounded projection/request state. It never calls `QCoreApplication::quit`,
persists settings, opens a D-Bus connection, launches a process, creates a file
dialog, or contacts a portal. The application and its injected public adapters
resolve every emitted quit or portal request and may retain copies of value
results.

`QindaQt::AppShell::MenuExport::ApplicationMenuExport` is a separate opt-in
composition module. It borrows one coordinator, one real `QWindow`, one
injected session-bus connection, and one owned identity publisher for the
window lifetime. This keeps D-Bus and toolkit identity discovery out of the
coordinator and QML module. Destruction or an accepted close withdraws the
published identity and registrar entry as the surface retires; a close that
AppShell rejects while awaiting application consent keeps the live menu
published.

The core `src/app_shell` target depends inward on Qt Core/Gui/QML/Quick plus the public QST-1 and
Controls modules. The opt-in `menu_export` target depends on the accepted
global-menu protocol/dbusmenu/registrar public boundaries and Qt
Core/DBus/Gui. Applications may depend on AppShell; AppShell may not depend
on `src/apps`, service implementations or clients, shell internals, KWin,
LayerShellQt, or application domain models. An app that does not need Settings
or session integration declares that hook `NotRequired`; AppShell never probes
availability on its behalf.

## Public C++ contract

### Errors and bounds

Every fallible registry or resolver operation returns an `Error` with one
`ErrorCode`, a bounded diagnostic, and a recoverability flag. QML request
methods return zero/false on failure and publish the same typed state through
`lastErrorCode`, `lastErrorMessage`, and `lastErrorRecoverable`; property
setters preserve the prior valid value. Success is exactly `ErrorCode::None`.
The typed failure set distinguishes invalid arguments, duplicate/unknown
actions, unavailable operations, busy serialization, stale replies, denial,
cancellation, backend failure, and wrong-thread use.

The compatibility bounds are 256 actions, 32 menus, 64-code-unit stable IDs,
128-code-unit labels, 512-code-unit diagnostics, 32 MIME types, and 32 result
URLs. Replacement and resolution reject hostile or inconsistent values before
changing confirmed state. These are resource/safety limits, not UI truncation
rules; applications localize shorter visible labels.

### Action and menu export

`ActionSpec` contains a stable action ID, stable menu ID and label, visible and
accessible command labels, a `QKeySequence`, deterministic menu/action order,
and enabled/checkable/checked/destructive projection state. `ActionRegistry`
validates a complete replacement atomically and publishes a deterministic
two-level `QVariantList` snapshot for QML. Duplicate IDs, inconsistent labels
for one menu ID, missing shortcuts, invalid IDs, hostile lengths, or impossible
checked state reject the entire replacement.

Activation emits `activationRequested(actionId)` only for a known enabled
action. It does not toggle domain state or invoke a callback itself. The owning
application executes the command, then explicitly updates enabled/checked
projection. These are window-local actions; AppShell never registers
KGlobalAccel.

The opt-in `ApplicationMenuExport` turns that same deterministic snapshot into
lineage-free canonical content and lends it to the complete standard dbusmenu
v4 server owned by `QindaQt::GlobalMenuDbusMenu` at
`/org/qindaqt/AppShell/Menu`. It never imports the canonical exporter or mints
owner/epoch/revision metadata. The authenticated shell selector remains the
only lineage authority after it decodes the remote snapshot. Menu changes
publish whole snapshots, and a dbusmenu `clicked` event calls
`ApplicationCoordinator::activateAction()` once. The coordinator therefore
rechecks the same known/enabled consent gate used by the in-window menu before
emitting `actionRequested`; transport failures and uncertain replies are never
replayed.

All bus calls are asynchronous. An unavailable bus or identity fails closed.
The composition watches the standard AppMenu registrar owner, withdraws its
old association on loss/replacement, and registers again only against the new
exact owner. On XWayland, the Qt platform must be `xcb` and the real `QWindow`
WId is the registrar window id. On native Wayland, there is deliberately no
numeric id: the confined Qt platform adapter announces the injected unique bus
name and object path through Qt's KDE appmenu platform hook after a native
Wayland surface exists. Those are exactly the facts G2 projects to the shell;
the shell, not the application, proves their bus-owner PID matches the focused
surface. `published` means only that the endpoint and association exist. The
optional first-party composition also observes the current registrar owner's
bounded host acknowledgment. Its visibility callback starts visible and
becomes hidden only when the QindaQt panel has accepted that exact endpoint;
every absent, foreign, stale, failed, or withdrawn acknowledgment restores the
local `MenuBar`. The shared callback drives the QML File Manager menu and the
widget menus in Text Editor and Terminal through the same rule.

### Lifecycle and quit ownership

`requestQuit(reason)` serializes one pending quit decision and emits
`quitDecisionRequested(id, reason)`. The application evaluates dirty documents,
terminal jobs, transfers, or other domain state and calls `resolveQuit()` with
the exact ID. Approval emits `quitApproved`; rejection emits `quitRejected`.
Stale IDs cannot close the window, and repeated requests while a decision is
pending fail `Busy`.

The reusable QML surface intercepts ordinary window close, requests a decision,
and closes only after `quitApproved`. Neither collaborator terminates the
process. This preserves one primary ordinary top-level and application-owned
consent while remaining compatible with compositor decorations and Hybrid
window containers.

### Settings and session hooks

Settings and session hooks each project one `IntegrationState`:

| State | Meaning |
| --- | --- |
| `NotRequired` | This app does not consume the integration. |
| `Ready` | The owning adapter has confirmed current usable state. |
| `Degraded` | The app remains usable with a bounded limitation. |
| `Unavailable` | The capability cannot currently be used. |

Only the owning composition can set these states. AppShell combines degraded
and unavailable details into one visible/accessible notice; it does not invent
fallback data, retry policy, or readiness from elapsed time. A service restart
must be fenced by the real client before it publishes `Ready` again.
`hasUnavailableIntegration` is the read-only aggregate used by the QML surface
to label a genuinely unavailable feature separately from a still-usable,
limited capability; detailed state and recovery policy remain application
inputs.

### File-portal mediation

Open-file, save-file, and select-folder requests publish typed `PortalRequest`
values. One request may be pending at a time. Titles, suggested base names and
MIME filters are validated and bounded before emission. A portal adapter calls
`resolvePortal()` with the exact request ID, acceptance flag, absolute URLs and
typed error. Stale, over-broad, relative, or internally inconsistent replies
leave the request pending and fail closed.

AppShell deliberately does not instantiate `QFileDialog` or speak the desktop
portal protocol. This keeps sandbox policy, grants, parent-window handles and
backend ownership in a separately testable adapter. Cancellation is a normal
typed result and does not become the coordinator's ambient last error.

## QML surface

`ApplicationShell` is an `ApplicationWindow` with a required coordinator, a
default page-content slot, a deterministic exported menu bar, one Qinda
`DegradedNotice`, and an initial-focus seam. It consumes semantic QST roles and
QindaQt.Controls without theme IDs, palette literals, ambient Quick Style, or
service knowledge.

The application supplies `initialFocusItem`. After construction the surface
uses `Qt.TabFocusReason`, reports each named active-focus owner to the
coordinator, and preserves native MenuBar keyboard handling and action
shortcuts. The native top-level exposes its window title through Qt's QWindow
accessibility bridge; the item-derived page pane carries the application name
and current degraded description. The degraded notice and application controls
carry their own accessible roles, names, states, and descriptions. Interactive
app content remains application-owned and must supply its own complete tab
order and accessible metadata. Attaching Qt Quick's `Accessible` object
directly to `ApplicationWindow` is invalid because it is not an `Item` or
`Action`; tests exercise the native window interface and page pane separately.

Example composition shape:

```qml
import QindaQt.AppShell 1.0
import QindaQt.Controls 1.0 as Qinda

ApplicationShell {
    coordinator: appCoordinator
    initialFocusItem: mainAction

    Qinda.Button {
        id: mainAction
        objectName: "mainAction"
        text: qsTr("Open")
        accessibleDescription: qsTr("Choose a document to open")
    }
}
```

The C++ composition must set the exact desktop file name before any window maps
and publish one complete QST-1 generation before constructing the surface.
AppShell does neither because desktop identity and theme selection remain
application policy.

## Verification boundary

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.app-shell-' --output-on-failure
```

Registered tests cover deterministic action ordering, atomic hostile-input
rejection, unavailable activation, check-state projection, quit ownership and
stale fencing, optional integration/degraded projection, serialized portal
requests and invalid results, focus-name bounds, offscreen QST/Controls loading,
initial keyboard focus, native-title window identity, application-named page
accessibility, and the visible degraded notice. A static policy gate rejects
platform/service dependencies from the core and rejects ambient bus lookup or
private-toolkit-hook leakage from the opt-in exporter. Private-bus rows add a
real dbusmenu client, fake registrar owner replacement/loss, exact X window-id
registration, native-Wayland no-numeric-id behavior, exactly-once activation,
disabled-action refusal, rejected-close retention, and accepted-close teardown.
The transport server row covers all v4 methods, depth/property filtering,
explicit grouped calls, the empty-ID all-items form, and malformed-snapshot
retention. The static matcher rejects a local lineage issuer or dbusmenu
interface and proves itself with service-lookup poison. The remaining policy
gate rejects palette literals, and theme selection. The installed-consumer row
clears ambient QML import paths, checks the staged headers/QML/plugin payload,
recompiles a C++ consumer, runs it, and loads the staged QML module with
`qmltestrunner`.

These gates do not qualify a real portal backend,
Settings1/session client composition, compositor focus,
live assistive technology, nested-session capture, or physical display/DPI.
Those become later vertical slices using this boundary; they must not be
claimed from the module tests alone.

[QindaQt Text Editor](text-editor.md) is the first migrated first-party
consumer: it publishes its File/Edit actions through `ActionRegistry`, routes
close consent through `requestQuit`/`resolveQuit`, and mediates Open/Save As
through a fail-closed-by-default `PortalRequest` adapter. Its
[AppShell participation](text-editor.md#appshell-participation) section
records the consumer-side contract and focused test row; a real portal
backend and Settings/session hook composition remain separate, still-unqualified
outcomes. File Manager, Text Editor, and Terminal retain the shared composition
beside their primary window/coordinator and supply its local-menu visibility
callback. The callback starts visible, hides only after the current QindaQt
registrar confirms that a live renderer accepted the exact endpoint, and
restores visibility when that proof is withdrawn or cannot be revalidated.
The cross-process acknowledgment, focus-retention rule, and fallback behavior
are defined by
[ADR-0077](../adr/0077-acknowledge-global-menu-hosting-before-hiding-local-menus.md).
