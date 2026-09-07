# QindaQt Settings — Color route

`qindaqt-settings --page color` is the first-party per-display ICC profile
assignment surface. It composes only public boundaries: the public Display1
client for the live output inventory, the C1 discovery/import provider
(`display_color_discovery`) for the profile catalog and user imports, and the
C1 assignment store (`display_color_assignment`) for persistent per-output
assignment intents through the public Settings1 client. The route model
receives all four borrowed collaborators from one narrow engine-singleton
composition and owns a separate public display-writer port for compositor ICC
application. Private compositor objects, colord, and KWin implementation APIs
never cross into the route.

The persisted document shape is fixed by
[ADR-0066](../adr/0066-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md)
and the [Settings1 reference](../reference/settings1-v1.md); the provider
contracts are in the [Display color model](../architecture/display-color-model.md)
architecture page.

## Truth shown by the route

The page distinguishes loading, ready, degraded, stale, and unavailable
states. Authoritative truth is presented in three inventories:

| Group | Public truth | Interaction |
| --- | --- | --- |
| Displays | Stable ID, presentation name, connector, and the currently assigned ICC profile decoded from `displays.colorAssignments` | Select one display; selection is presentation-only |
| Profiles | C1 discovered catalog (bounded, origin-classified) for the selected display | Assign one profile only when the exact admission predicate passes; Remove assignment under the same fence |
| Disconnected displays | Persisted records whose output is absent from the live inventory | Read-only; C1 retains intent across transient unplug until an explicit removal draft |

Discovery semantics are unproven C0 placeholders, so no scanned profile is
presented as an sRGB default. Every state and warning has visible text;
meaning is never carried by color alone.

## Exact admission and apply lifetime

One predicate supplies both each displayed availability flag and final
dispatch admission. It requires an exact Display1 lineage (nonempty client
owner, a validated snapshot with nonempty epoch and nonzero revision, client
state `Ready` or `Degraded`), a confirmed usable Settings1 document
(`Ready` availability), no write in flight, the selected output present in
the current snapshot, and the target profile present in the current catalog.
Re-assigning the already-persisted profile is refused as a no-op before any
write. An active Display1 preview transaction closes admission so an ICC
request cannot race a topology preview or its rollback.

Assignment and removal dispatch one `SettingsAssignmentStore` draft fenced
with the confirmed Settings1 revision. The draft record carries the imported
profile's SHA-256 lineage fingerprint when the profile was imported during
this route session (a rescan can never recover the digest, so the model
retains it from the import result); previously imported or scanned profiles
carry an empty lineage, exactly as the C1 document contract records. Success
never edits presented truth
optimistically: rows update only from the authoritative document refresh.
After Settings1 confirms the desired assignment, the route resolves the current
connector and discovered ICC path and submits one public output-management
configuration. It reports the profile active only after the compositor replies
`Applied`; rejection, connection loss, and uncertain completion remain visible
failures. On route activation, display or assignment refresh, and compositor
reconnect, saved assignments for connected enabled outputs are reapplied
serially after any Display1 preview finishes.

`Conflict` surfaces "settings changed elsewhere" and rebases on the
authoritative document; `Uncertain` and `Failed` surface visible feedback.
Neither is ever replayed automatically, matching the Appearance route's
conflict/no-replay truth. Owner or epoch replacement on either client clears
actionable truth and closes admission until authoritative state returns.

Settings1 retains the last confirmed document across authority loss; the page
presents it as **stale** with every domain control closed — retained truth is
never live authority.

## Import boundary

"Import profile…" opens a bounded local file dialog (Qt `FileDialog`; no
portal mediation, no remote locations). The chosen URL must be a local file;
the C1 import seam then revalidates the complete source fail-closed (header,
size, name safety, symlink refusal) before copying it into the injected user
root with its SHA-256 lineage fingerprint. Import outcomes — imported,
already present, and every typed rejection — are visible text. Import needs
no service authority and is always admitted; the catalog rescans after a
successful import.

Production discovery roots are resolved by the route composition root via
`QStandardPaths::GenericDataLocation`: the writable data location's
`color/icc` is the single `UserImported` root and every remaining standard
data location's `color/icc` (production: `/usr/share/color/icc`,
`/usr/local/share/color/icc`) is a `System` root. No repository-shipped
`BuiltIn` root exists yet. Because the roots derive from the XDG locations,
tests that redirect `XDG_DATA_HOME`/`XDG_DATA_DIRS` never read host profile
directories. The C1 writer fails closed on a missing user root, so the
composition provisions the `UserImported` root at startup: a root it creates
is made mode 0700 (EUID-owned, non-group/other-writable, exactly the
writer's admission contract), while an existing directory keeps the user's
own permissions and stays subject to the writer's fail-closed validation.
Discovery runs only while the route is active (a route hook
mirrors the Bluetooth lease pattern), so an idle Settings process never
scans.

## Compositor application boundary

[ADR-0083](../adr/0083-apply-saved-color-profiles-through-public-output-management.md)
confines ICC application to the existing public display-writer port. The route
cannot change topology, mode, scale, transform, HDR, or WCG policy through this
method. The allow-list source gate accepts only the writer's public header and
continues to reject KWin/private compositor symbols, system color daemons,
private Display service modules, sibling application internals, and direct
D-Bus outside the named composition root.

## Responsive interaction and accessibility

The same vertically scrollable content serves wide and compact Settings
hosts. Page Up/Page Down and Ctrl+Home/Ctrl+End move the viewport, and focus
changes reveal the active control. Display and profile actions expose
radio-button role and checked state with accessible names and descriptions;
disconnected-assignment cards expose list-item names and descriptions. Busy,
unavailable, degraded, stale, error, and authority states are visible text.

The page computes its host-entry target from current admission truth: the
first enabled profile or unassign action, then the first enabled display
selection, then the always-admitted Import button. Service-unavailable truth
prefers Retry and falls back to Import when retry is not admitted. A fenced
domain action is never nominated. The page has no embedded Close button;
window closing stays with the Settings shell and desktop window controls.
Escape returns focus to the active Color PageTab in both layouts, and Ctrl+0
selects the appended tenth route.

## Composition and package boundary

The closed Settings registry maps only canonical `color` to the compiled
Color component. Unknown and path-like values still exit before QML or Color
composition. `ColorRouteComposition` is an engine singleton so responsive
host reconstruction does not duplicate the public clients, the assignment
store, or the discovery provider; the Settings1 client is scoped to
`displays.colorAssignments` and has its own transport, so no request-token
domain is shared with another route.

An unreachable session bus at composition startup is an expected degraded
state, not a fault. The composition never raises `qWarning`/`qCritical` on
that path: the Settings1 start failure is logged at info level on the
`qindaqt.settings.color.composition` category, and the route presents the
assignment document as unavailable through the model's normal availability
truth. This keeps every warning-fatal in-process `Main.qml` host row green
with no bus reachable; eager singleton evaluation in the Settings Center
host is therefore harmless to unrelated pages.

The `SettingsAppearanceRuntime` component installs the shared Color page
module and the executable's relative Color import path. The small
public-client composition backend is linked once into the process and into
every in-process `Main.qml` test host; keeping presentation in the shared
module makes the installed route a real runtime dependency. The dedicated
relocation test withholds that installed module while the developer tree
remains present and requires root construction to fail, then restores the
module and proves the relocated route remains resident with both host buses
poisoned.

## Verification and non-claims

Focused selection:

```sh
DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir build/dev --output-on-failure --no-tests=error \
  -R '^qindaqt\.settings-color-'
```

Every Color row pins both D-Bus addresses to nonexistent sockets in its
CTest environment, so the selector is host-bus-free on any host regardless
of the caller's environment.

The model row covers inventory/assignment/catalog projection, state truth,
unusable-document and stale closure, selection fencing across hotplug and an
active Display1 preview, and disconnected-record presentation. The apply row
covers fenced draft dispatch, startup restore, compositor callback truth,
displayed-availability-equals-admission, no-op refusal,
conflict/uncertain no-replay, in-flight fencing, owner replacement, and the
import refresh/rejection flows — all over injected fake transports and
temporary discovery roots. The composition row
redirects the XDG data locations to a fresh home and proves the composition
provisions the mode-0700 EUID-owned user import root and that a first import
through the public C1 provider succeeds on that fresh home. The warning-fatal
page row renders wide and compact software scenes, verifies action wiring,
accessible roles/descriptions, the page purpose, and the
always-admitted focus targets. The navigation-page row drives the real
Settings Center host in both layouts: Ctrl+0 selection, Color PageTab
accessibility, Escape/Tab focus entry, and exclusive wide/compact Color
loaders. Boundary/poison and installed-route rows
prove the source and relocated package boundaries. Settings Center
registry/controller tests additionally cover tenth-route order.

No test row contacts a host session/system bus, a host display, a real ICC
directory, Wayland, or hardware; writer callbacks are injected. The composition and the warning-fatal
Settings Center host rows pin both bus addresses to nonexistent sockets in
their CTest environment, so this claim is executable on any host. This
slice does not claim colord integration, HDR/WCG runtime behavior,
profile-body interpretation, physical calibration, live AT-SPI, or
nested-session visuals.

### Physical compositor application check — 2026-09-06

On the physical HDMI-A-1 display, a temporary standard sRGB ICC profile was
applied through the public KScreen output-management client. A fresh compositor
readback reported the exact profile path. Removing the assignment and reading
back again restored the original empty path. Both operations exited successfully.
This proves that the running DRM backend accepts and removes an ICC assignment;
it does not establish calibration accuracy or qualify the Settings UI end to end.

The separate Settings-page run exercised discovery, display selection, and
assignment in a private desktop. Its protocol trace contains the profile-path
request, profile-source request, and compositor acknowledgement. The nested
Wayland backend does not retain ICC state, so that acknowledgement alone cannot
prove profile application. Keep the physical readback evidence distinct from
that UI/protocol evidence.
