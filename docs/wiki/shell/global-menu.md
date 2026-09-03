# Global application menu

The global menu applet presents the focused window's application menu in the
panel. QindaQt builds it on a bounded, toolkit-neutral canonical menu/action
model with proof-bound authenticated ownership. G1 provides compatibility with
the standard AppMenu registrar and dbusmenu transports without inheriting the
registrar protocol's unauthenticated authority model. The durable choices are
in [ADR-0033](../adr/0033-canonical-menu-model-and-authenticated-menu-ownership.md)
and [ADR-0056](../adr/0056-adopt-standard-appmenu-dbusmenu-transports.md).

## Milestone boundary

G0 delivered the pure model, policy, exporter, Qt Widgets adapter, and applet
facade. G1 delivered the AppMenu registrar, asynchronous dbusmenu transport,
and shell-neutral composition coordinator. G2 now composes those accepted
boundaries in `qindaqt-shell`: the audited built-in resolves in top-panel
profiles, the shell owns the registrar on its injected session bus, consumes
authenticated active-window identity through its existing exact-owner
window-actions client, and hosts the compiled `QindaQt.Shell.GlobalMenu`
module with bounded submenu popups. `GlobalMenuAppletRuntime` is the narrow
installed component.

This milestone does not claim GTK/foreign-toolkit exporters or an installed
nested-session qualification. The full-tree snapshot protocol remains the
authority; payload-bearing deltas are still deferred.

## Canonical model

`QindaQt::Shell::GlobalMenu::Protocol` (module `QindaQt::GlobalMenuProtocol`)
owns the only menu representation that crosses any boundary. `MenuItem` is a
bounded value with kind `Action`, `Separator`, or `Submenu`; `MenuTree` adds
the owner/epoch/revision lineage used by Display1, Audio1, and Settings1.

- Text, ids, shortcut text, and radio-group names have fixed UTF-8 byte
  ceilings (menu_limits.h); a tree that exceeds any bound is invalid as a
  whole.
- Text must be well-formed Unicode: embedded NULs and isolated (unpaired)
  UTF-16 surrogate code units are rejected, because they are not
  representable scalar values.
- `text` never contains a toolkit mnemonic character. The mnemonic position is
  `mnemonicIndex`, a UTF-16 offset into `text`, or -1. Toolkit escaping
  ('&', '_', ...) exists only inside adapters.
- Ids are unique across a tree and stable across snapshots; they are the
  only trusted invocation key.
- Separators carry no content, actions have no children, `checked` requires
  `checkable`, and a radio group admits at most one checked member per
  parent. A kind outside the three known values rejects the node — its
  children are never traversed.
- `validateMenuTree` rejects hostile input as a whole rather than admitting a
  partial tree, matching every other QindaQt wire model.
- G0 keeps snapshot-only truth: a full-tree delta contract (payload-bearing
  operations with safe application order) is deferred to the transport
  milestone, where it will be designed and proven with an
  apply-to-next-tree test before any consumer exists.

## One authoritative lineage

The ownership selector is the single lineage authority. It mints the
`epoch` (only when the owning window identity changes) and advances the
`revision` (on every adoption within one epoch); the exporter stamps
exactly that epoch/revision into each accepted tree through an injected
`ExportLineageSource` seam, and shell composition backs that seam with the
selector. Owner, epoch, revision, and the invocation guard's expectations
therefore share one source of truth, and an ordinary public-API flow —
authenticate, adopt the returned proof, export, invoke — is coherent by
construction. The exporter never mints lineage itself; a pull whose owner
has no current authority is rejected without publishing. Publication
enforces the lineage/content binding at the accepted-publication authority:
within one epoch, changed content is accepted only when the revision
strictly advances, and regressed revisions or null epochs are rejected
outright as `RejectedStaleLineage` with the last accepted tree retained, so
a replay that re-pushes changed content under a consumer-observed revision
cannot become truth.

## Proof-bound authentication

`QindaQt::Shell::GlobalMenu::Ownership` (module `QindaQt::GlobalMenuOwnership`)
decides which provider may become authoritative:

- The active-window seam reports compositor-authenticated observations
  carrying a monotonic `focusGeneration` that changes on every focus move.
- Authentication reads focus, performs the bus-daemon credential lookup,
  then re-reads focus: both observations must agree on window and focus
  generation, or the attempt fails with `focus-changed`. This closes the
  sample-lookup race where focus moves mid-check.
- The registering peer must present a syntactically valid D-Bus unique name
  (the `:1.42` shape: leading colon, at least two dot-separated
  `[A-Za-z0-9_-]` elements, at most 255 bytes per the D-Bus bus-name
  maximum). Well-known names are refused at the ownership boundary
  even when the credential seam resolves them, because a well-known name can
  be re-owned later and silently change who a proof names.
- An accepted authentication returns an `AuthenticatedProvider` proof — an
  opaque, non-aggregate capability whose constructor only
  `ProviderAuthenticator` can call. It carries exactly the verified window
  identity, unique name, and focus generation. `ActiveProviderSelector::
  adopt` accepts only this proof, so the verified identity and the adopted
  identity cannot diverge and no caller can mint accepted ownership.
- `applyFocusGeneration` is the fail-closed invalidation seam: a generation
  other than the adopted proof's drops the adoption. Shell composition
  calls it on every observed focus change before any export or invocation.
- Only the currently active window can become authoritative. G1 accepts
  arbitrary bounded registrar entries for protocol compatibility, but an entry
  is only a cache claim: the composition coordinator joins the numeric id to
  an independently authenticated focused-window identity and re-runs the G0
  PID/unique-name proof before adopting it.
- `InvocationGuard` requires the request's (windowId, epoch, revision), the
  presented tree's lineage, and the selector's current lineage to agree
  exactly; any mismatch is `stale-owner` before any action lookup. It then
  rejects unknown ids, non-actions, and disabled/invisible items.

## Export lifecycle

`QindaQt::Shell::GlobalMenu::Exporter` (module `QindaQt::GlobalMenuExporter`)
pulls `MenuSnapshot` values through the toolkit-neutral `MenuSource`
interface. A snapshot carries an explicit completeness verdict: when a
source detects overflow (depth, siblings, or total items), a submenu cycle,
or the loss of its observed widget, it marks the snapshot incomplete with a
stable defect code (`too-deep`, `too-many-children`, `too-many-items`,
`submenu-cycle`, `source-destroyed`) and the exporter rejects it whole — a
bounded prefix or an authoritative empty menu is never published. A
destroyed source is a lifetime defect, not an empty application menu. Valid
snapshots are validated against the canonical bounds, stamped with the
authoritative lineage, and stored; a rejected or incomplete pull keeps the
last accepted tree, so a transiently malformed source can never regress a
previously good menu. Content that is identical under a re-advanced lineage
reports `Unchanged` but is re-stamped, so the published tree never drifts
stale against the selector.

## AppMenu registrar transport

`QindaQt::GlobalMenuRegistrar` owns the standard
`com.canonical.AppMenu.Registrar` object at
`/com/canonical/AppMenu/Registrar`. Its explicit `AppMenuRegistrar::start()`
composition root registers the object and requests the well-known name on an
injected session-bus connection; construction alone has no global side effect,
and partial startup rolls back.

- `RegisterWindow`, `UnregisterWindow`, `GetMenuForWindow`, `GetMenus`,
  `WindowRegistered`, and `WindowUnregistered` retain their standard wire
  signatures.
- The registrar stores at most 1,024 windows and caps object paths at 4,096
  UTF-8 bytes. Window zero, malformed or oversized paths, malformed caller
  identities, cross-owner replacement, and capacity or generation exhaustion
  fail closed.
- The D-Bus message's caller unique name is the owner. Only that exact peer may
  update or unregister its window; a well-known name is never stored as owner.
- Owner generations and registration generations are monotonic. A delayed
  owner-loss callback must carry the current owner generation or it cannot
  retire anything. When a unique connection disappears, all and only its
  registrations are removed and standard unregistration signals are emitted.
- The registry is not an authentication authority. Numeric window ids become
  meaningful only through the injected `RegistrarWindowIdSource`, which maps a
  compositor-authenticated opaque identity or returns no match.

## dbusmenu client transport

`QindaQt::GlobalMenuDbusMenu` binds one injected connection to one exact
provider unique name and object path. All method calls are asynchronous and
bounded to a two-second default timeout. `LayoutUpdated` and
`ItemsPropertiesUpdated` are invalidation hints: publication always waits for
a complete `GetLayout` reply, while `GetGroupProperties` and `AboutToShow`
can request a revisioned reread. `Version`, `Status`, and `TextDirection` are
read through the standard properties interface and published only as one
validated metadata value.

The decoder accepts the recursive `(ia{sv}av)` layout and converts it to the
canonical tree as one atomic operation. It enforces the canonical depth,
children, total-item, label, id, shortcut, and per-item property-count limits,
plus a 256-byte icon-name and 256-KiB icon-data ceiling. Known properties have
exact types and closed values; unknown properties are ignored within the bounded
map. Underscore mnemonics become the
canonical UTF-16 mnemonic index, and the first bounded shortcut sequence is
normalized (`Control` to `Ctrl`, `Super` to `Meta`) without importing Qt Gui.
An invalid child, duplicate/nonpositive id, oversized icon, invalid Unicode,
or malformed known property rejects the whole layout and retains the prior
accepted snapshot.

Remote revisions form a per-owner high-water mark. A lower reply is stale; an
equal revision carrying different content is contradictory; neither can
replace accepted truth. Owner loss retires pending replies and the snapshot.
`Event` is sent at most once per admitted intent. Timeout or error is uncertain
and reported without retry, preventing duplicate application activation.

## Compositor identity consumption contract

G2 consumes `CompositorShell1.ActiveWindowIdentity` through the existing
exact-owner shell window-actions client; it must not construct another
compositor connection or treat `Compositor1.Windows` as a PID source. The
identity `revision` is the ownership model's `focusGeneration`. The client
withdraws the snapshot on its directed change signal, then publishes only a
complete monotonic reread. Composition samples identity, resolves credentials,
and samples identity again; both reads must retain the same epoch, revision,
active UUID, and action fence before `ProviderAuthenticator` may issue a proof.

For XWayland, composition selects the registrar entry whose numeric id equals
the compositor-projected `appMenuWindowId`, then requires the registrar entry's
exact unique owner to have the compositor-projected PID. For native Wayland,
the numeric id is deliberately `null`; composition uses only the paired
service name/object path announced on that credentials-owned surface, resolves
the name to one exact unique bus owner, and applies the same PID check. A null
PID, missing X11 id, half/malformed Wayland address, owner replacement, PID
mismatch, revision movement, or action-fence mismatch publishes unavailable.
Registrar contents never fill a missing compositor fact.

The client also rejects the complete identity reply unless its epoch and
`actionRevision` satisfy the public compositor action-generation rule. A
nonempty but whitespace-padded or otherwise invalid epoch must never surface as
`identityAvailable`; composition does not add a more permissive lineage parser.

This join prevents an arbitrary registrar claim from becoming authoritative,
but it does not authenticate the standard registrar itself: any local peer can
still register bogus window ids. It also does not prove an announced bus name's
owner until the shell performs the exact-owner credential lookup, prevent a
compromised application from exporting a malicious menu for its own window, or
support a legitimate exporter delegated to another PID. Those cases fail
closed under [ADR-0063](../adr/0063-project-authenticated-active-window-identity.md).

## Transport composition

`QindaQt::GlobalMenuTransportComposition` is instantiated by the production
shell through `GlobalMenuAppletComposition`. For XWayland, an injected focus
refresh resolves the projected numeric id, selects the exact registrar entry,
asks the bus daemon for that unique peer's PID, and runs
`ProviderAuthenticator`. For native Wayland, the same coordinator resolves the
projected service to its current exact owner and uses the projected object
path; owner replacement invalidates and rebinds through the same proof. Each
newly accepted dbusmenu layout is authenticated again, adopted into
`ActiveProviderSelector`, pulled through the unchanged `MenuExporter`, and
published to `GlobalMenuAppletAccess`. Thus remote dbusmenu revisions never
become invocation authority; the existing selector epoch/revision remains the
only accepted lineage.

An applet activation synchronously captures the published tree, runs the
existing `InvocationGuard`, converts the canonical numeric action id back to
the dbusmenu item id, and submits one `clicked` event. Missing focus mappings,
registration replacement, PID/name mismatch, focus movement, owner loss, or
stale lineage publishes unavailable and admits no event. Registrar startup
collision publishes the explicit `degraded` phase with reason
`registrar-name-owned`; missing policy/catalog authority is `unavailable`.
Stop, owner loss, and replacement clear all items before any refresh.

`ShellRuntimeApplication` constructs exactly one
`QtShellWindowActionsTransport`/`ShellWindowActionsClient` pair and lends that
client to the composition. The global-menu runtime neither discovers nor owns
a second compositor connection. `identityChanged` drives focus refresh, so the
client's owner-loss and monotonic reread rules are also the menu's stale-truth
withdrawal boundary.

## Qt Widgets adapter

`QMenuBarMenuSource` (module `QindaQt::GlobalMenuQtWidgetsAdapter`) walks a
real `QMenuBar`/`QMenu`/`QAction` tree into the canonical model. This is the
exact shape the integrated Text Editor exposes per
[ADR-0022](../adr/0022-keep-text-documents-local-and-atomic.md): persistent
`QAction` object names become ids, '&' mnemonics split into display text plus
offset, exclusive `QActionGroup` membership becomes a radio group, and
`QKeySequence` text is carried as bounded shortcut text. Visibility is
carried verbatim so presentation can omit hidden entries honestly. It is the
only target in `global_menu` that links `Qt6::Widgets`; the QtQuick shell
never gains that dependency. Apps that want stable ids across menu edits
must set persistent object names; the positional fallback is only stable
while sibling structure does not change. The observed widget tree must
outlive the source, snapshots must run on the Qt GUI thread, and menus must
not be mutated during a snapshot; violations degrade to an incomplete
snapshot, never to wrong complete truth.

## Applet facade and presentation

`GlobalMenuAppletAccess` mirrors
`NotificationCenterAppletAccess`: shell composition publishes authoritative
state, and QML reads its recursive projection and requests an activation. The
projection is honest by construction: entries carry their `kind` ("action",
"submenu", or nested "separator"), children, shortcut, and checked state;
hidden items are omitted, while nested separators are retained for popup
layout. `activate()` admits only enabled visible actions. `publishTree` is
fail-closed: invalid input publishes the unavailable state instead of any
part of its content. The facade is GUI-thread-confined, and its G1 consumer
must capture the observed window/epoch/revision at request time and run
`InvocationGuard` on that captured lineage before executing anything;
looking up a "current" tree by id at execution time would recreate the
request/content race the guard exists to close.
`GlobalMenuApplet.qml` renders entries as focusable `AbstractButton`
delegates that carry the provider-owned checked state in the accessible
attributes: the button itself never toggles locally, so interactive
activation (pointer, keyboard, or assistive-technology press through the
attached accessible signal) requests the action and the provider republishes
new truth instead of presentation inverting state on its own. Entries lay
out in a `Row` or a real `Column` for vertical panels. Overflow follows a
measured geometry contract: the fit loops consume strict upper bounds built
from real font metrics (`TextMetrics` plus a fixed safety margin) of the
labels and the "+N" indicator, iterate against the assigned width
(horizontal) or height (vertical), and reserve the indicator inside the
extent — so no real label or affordance can ever be clipped by the limit.
Hosts below the documented minimum extent degrade to indicator-only (and
the indicator hides itself when even it cannot fit) rather than painting
partial content inside the clipped root. A clamped
`maximumVisibleEntries` acts as the count cap on top of the measured fit.

An enabled submenu opens one `GlobalMenuPopup`. The popup keeps a stack of at
most six menu levels, skips disabled entries and separators during Up/Down
navigation, enters with Right, returns with Left, activates once with
Return/Enter/Space, and closes on Escape, outside press, or focus loss. It
exposes `PopupMenu`/`MenuItem` roles, names, descriptions, focusability, and
provider-owned checked state. Reaching the depth cap fails closed. Popup
activation calls the facade exactly once; the existing invocation guard and
no-replay dbusmenu client remain the sole execution lineage.

`BuiltinAppletContent.qml` hosts this compiled module like Launcher, Audio,
Bluetooth, and Power. The panel factory injects only the facade; panel rows
never receive a bus object or transport.

## Manifest, policy, and packaging

The existing `global-menu` manifest requests `global-menu.read` and
`windows.activate`. The audited built-in policy grants only
`global-menu.read`; it explicitly denies `windows.activate` because menu
events do not activate arbitrary windows. The compiled built-in registry maps
`QindaQt.Shell.GlobalMenu` to the in-process host. Stock placement remains
limited to QindaQt, macOS, and Unity families, each on a top panel and only
when `workflow.globalMenu` is true.

`GlobalMenuAppletRuntime` installs the production shell, global-menu module
library/plugin/QML metadata and sources, the embedded registrar/transport
implementation, manifest, policy, QindaQt profile, default theme, and the shared
Controls/Tokens/Launcher closure. Every other shell-carrying component also
installs the global-menu module library because the shell has a direct runtime
dependency. The component-closure gate authenticates that relocation.

## Remaining boundaries

- No general application framework, foreign-application injection, or
  arbitrary command execution: the model carries menu values, never launch
  payloads, and invocation authorization never executes anything itself.
- No private KWin/KDE ABI: the runtime consumes only the public exact-owner
  shell window-actions client and its authenticated identity snapshot.
- No GTK/foreign-toolkit exporter, arbitrary exporter injection, or
  payload-bearing canonical delta contract is part of G2.
- The registrar/dbusmenu tests use private `dbus-run-session` connections only;
  the installed-package row uses offscreen source poison. Neither qualifies a
  real login session, foreign toolkit, or nested installed desktop.

## Verification

Focused gates: `qindaqt.global-menu-protocol`,
`qindaqt.global-menu-ownership` (authentication and proof issuance),
`qindaqt.global-menu-ownership-lineage` (selector lineage and guarded
invocation), `qindaqt.global-menu-exporter`,
`qindaqt.global-menu-qt-widgets-adapter` (offscreen),
`qindaqt.global-menu-applet-access`,
`qindaqt.global-menu-composition` (authenticate → adopt → export → invoke
over the public seams), `qindaqt.global-menu-applet-qml-offscreen`
(behavior/activation cases),
`qindaqt.global-menu-applet-qml-accessibility-offscreen` (real accessible
press and provider-owned checked state), and
`qindaqt.global-menu-applet-qml-overflow-offscreen` (measured-geometry
overflow, vertical layout, and below-minimum host cases). Transport rows are
`qindaqt.global-menu-registrar-private-bus`,
`qindaqt.global-menu-dbusmenu-decoder`,
`qindaqt.global-menu-dbusmenu-private-bus`,
`qindaqt.global-menu-transport-composition-private-bus`, and
`qindaqt.global-menu-transport-boundary-poison`. G2 adds
`qindaqt.global-menu-runtime-composition-private-bus`,
`qindaqt.global-menu-runtime-boundary-poison`,
`qindaqt.global-menu-applet-submenu-qml-offscreen` under
`QT_FATAL_WARNINGS=1`, `qindaqt.global-menu-installed-package`, and the shared
`qindaqt.shell-runtime-component-closure`. Live installed-session
qualification remains unbuilt and unclaimed; see the
[testing harness](../development/testing-harness.md).
