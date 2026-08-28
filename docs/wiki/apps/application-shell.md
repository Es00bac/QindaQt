# QindaQt.AppShell 1.0

`QindaQt.AppShell 1.0` is the narrow participation shell for first-party QML
applications. It standardizes application-owned lifecycle decisions, stable
action/menu export, optional Settings/session readiness, file-portal request
mediation, bounded typed errors, focus reporting, and one accessible QST-themed
window surface. It is not a route registry, domain framework, service client,
or process supervisor.

The durable extraction decision and prohibited responsibilities are recorded
in [ADR-0027](../adr/0027-extract-a-narrow-first-party-application-shell.md).
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

`src/app_shell` depends inward on Qt Core/Gui/QML/Quick plus the public QST-1 and
Controls modules. Applications may depend on AppShell; AppShell may not depend
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
KGlobalAccel. The snapshot is suitable input for a later global-menu exporter,
but this module does not implement or contact one.

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
platform/service dependencies, palette
literals, and theme selection. The installed-consumer row clears ambient QML
import paths, checks the staged headers/QML/plugin payload, recompiles a C++
consumer, runs it, and loads the staged QML module with `qmltestrunner`.

These S0 gates do not qualify a concrete app migration, a real portal backend,
Settings1/session client composition, a global-menu exporter, compositor focus,
live assistive technology, nested-session capture, or physical display/DPI.
Those become later vertical slices using this boundary; they must not be
claimed from the module tests alone.
