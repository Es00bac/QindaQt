# QindaQt Settings — Power route

`qindaqt-settings --page power` is the first-party power-supply, profile, and
brightness and session-control surface. The route model receives an injected
`PowerClient` and purpose-built `SessionActionsClient`; a narrow
process-lifetime QML composition owns both clients and their injected Qt bus
connections. The same composition separately owns the bounded KScreenLocker
preference adapter described in [ADR-0091](../adr/0091-configure-kscreenlocker-preferences-through-settings.md).
UPower, power-profiles-daemon, login1, ScreenSaver, sysfs, the resident Power
service, and the supervisor never cross into the Power model or page.

The wire values and bounds are fixed by the [Power1 reference](../reference/power1-v1.md),
and the platform authority remains owned by the
[Power architecture](../architecture/power-service.md).

## Truth shown by the route

The route presents only validated, bounded public snapshot copies:

| Group | Public truth | Interaction |
| --- | --- | --- |
| Power supplies | AC-adapter presence plus up to eight batteries or UPS devices, with state, exact percentage or coarse level, upstream time estimate, and textual warning severity | Read-only inventory |
| Power profiles | Active profile and at most four supported profiles | Select a different profile only when the exact snapshot admits it |
| Profile holds | Profile, bounded application name, and reason for each public hold | Read-only; daemon cookies and release authority are not exposed |
| Internal brightness | Normalized 0–10000 position and exact observed raw value/maximum | Read-only disabled slider because Power1 version 1 has no internal-display mutation |
| Keyboard brightness | Normalized 0–10000 position and exact raw value/maximum | Keyboard- and pointer-operable slider when Power1 admits mutation |
| Screen lock | Saved automatic-idle-lock preference and timeout | Enable/disable idle locking; adjust the retained one-to-240-minute timeout only while it is enabled |
| Session | Typed availability for Lock, Log out, Suspend, Restart, and Shut down | Lock and Suspend dispatch directly; Log out, Restart, and Shut down require confirmation |

Every state and warning has visible text; meaning is not carried by color
alone. Unknown values are labeled unknown instead of manufacturing a number or
estimate. The route shows loading, ready, degraded, stale, unavailable, queued,
pending, convergence-wait, failed, and uncertain states separately.

## Screen-lock preference boundary

The Screen lock section is separate from Session actions. It reads and writes
only KScreenLocker's documented `kscreenlockerrc` `[Daemon]` `Autolock` and
`Timeout` keys, retaining `LockOnResume`, `RequirePassword`, and every other
locker preference. Each mutation first re-reads the stored pair and applies
only the intended key, so an external edit to the untouched key survives the
save; a failed reload rejects the change instead of overwriting a config the
page cannot read. A successful save is followed by the standard KDE screen
locker's `configure` request. The section reports a saved-but-not-reloaded
result when that request fails and offers one explicit retry; it never
silently rolls back or claims live adoption. Retry re-runs the step that
actually failed — reload, save, or live reload — and only a persisted change
may reach the `configure` request, so a reported success never describes an
unsaved change. The current timeout stays stored while automatic
locking is off, and the page disables its duration selector until it is
turned on again. The selector offers common idle durations and retains a
previous valid custom duration so opening the page never silently changes it.

## Exact admission and operation lifetime

One predicate supplies both each displayed availability flag and final
dispatch admission. It requires a retained validated snapshot, a nonempty exact
client owner, a client state of `Ready` or `Degraded`, nonzero epoch/revision,
`Ready` or `Degraded` snapshot availability, the relevant capability, a
current supported profile or keyboard handle, target `canSet` truth, and no
conflicting debounce, operation, or convergence fence. A failed refresh may
retain bounded display rows, but stale client state disables every domain
control and refuses requests until authoritative truth returns.

Profile selection dispatches once. Keyboard slider gestures enter a 120 ms
single-shot debounce; further changes for that same exact row replace the
queued normalized value. A burst therefore sends at most one raw request.
Dispatch re-resolves the row against the unchanged owner, epoch, and revision,
converts the final normalized value with the public integer brightness math,
and submits the exact raw value. Returning a gesture to the admitted normalized
value cancels its queued predecessor; a distinct normalized position that
rounds to the current raw value is also a no-op. A different target or profile
remains fenced while a debounce is queued.

Every submitted operation pins request ID, kind, owner, epoch, revision, target,
and expected result. Success never edits presented truth optimistically. The
route waits for a matching authoritative snapshot at or beyond the result's
observed revision and keeps controls fenced for at most five seconds. Owner or
epoch replacement, malformed or mismatched completion, timeout, and uncertain
outcomes clear the local fence with visible no-replay feedback. The public
client performs its own bounded transport timeout and resnapshot.

## Session-action boundary

Power1 version 1 remains free of session actions. `PowerRouteComposition`
constructs the public session-actions client and injects it as an opaque
presentation object; the Power model owns no session invokable. QML consumes
only typed availability, pending, feedback, and five request methods. The
allow-list gate admits that public header only in the named composition and
rejects private Power services/adapters, direct login1/ScreenSaver identifiers,
UPower, sysfs, sibling application internals, and direct D-Bus from QML/model
code. The independent poison rows prove those escapes are rejected.

Session1 deliberately authenticates only the supervised shell PID. A separate
Settings process therefore presents Log out as unavailable; it does not proxy
or weaken that authentication. Lock and login1-backed actions retain their own
standard-service availability.

## Responsive interaction and accessibility

The same vertically scrollable content serves wide and compact Settings hosts.
Page Up/Page Down and Ctrl+Home/Ctrl+End move the viewport, and focus changes
reveal the active control. Supply, hold, and brightness cards expose list-item
names and descriptions. Profile buttons expose radio-button role and checked
state. Brightness sliders expose slider role, target name, normalized value,
and exact raw value in both visible and accessible descriptions.

The page computes its host-entry target from current admission truth: the
automatic screen-lock toggle when enabled, then the first enabled profile
action, then the first enabled keyboard slider, then Retry,
then the route surface. Closing Settings remains a single window-level action;
the page does not duplicate it. A disabled internal slider or fenced domain
action is never nominated. Escape returns focus to the active Power PageTab in
both layouts, and Ctrl+8 selects the appended eighth route. Tab from that
PageTab lands on the automatic screen-lock toggle first, since the Screen lock
section now precedes Power profiles on the page.

## Composition and package boundary

The closed Settings registry maps only canonical `power` to the compiled Power
component. Unknown and path-like values still exit before QML or Power
composition. `PowerRouteComposition` is an engine singleton so responsive host
reconstruction does not duplicate the public client or request domain.

The `SettingsAppearanceRuntime` component installs the shared Power page module
and the executable's relative Power import path. The small public-client
composition backend is linked once into the process; keeping presentation in
the shared module makes the installed route a real runtime dependency. The dedicated relocation test
withholds that installed module while the developer tree remains present and
requires root construction to fail, then restores the module and proves the
relocated route remains resident with both host buses poisoned.

## Verification and non-claims

Focused selection:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir build/dev --output-on-failure --no-tests=error \
  -R '^qindaqt\.settings-power-'
```

The model row covers bounded inventory, labels, raw values, holds, shared
profile admission, exact lineage, retained-stale presentation/admission
closure, convergence, retry-status recovery, owner replacement, and opaque
session-client injection without model authority. The slider row proves burst coalescing, normalized-to-raw
conversion, normalized and raw-equivalent no-ops, invalid/stale rejection, and
no dispatch after authority change. The screen-lock model row covers INI
round-trip preserving unrelated keys, merge-latest saves that keep external
edits to the untouched key in both directions, retry that re-runs a failed
load/save before any live configure is requested or claimed, saved-versus-live
failure truth, and clamped one-to-240-minute timeout bounds.
The warning-fatal page row renders wide and compact software scenes, verifies
Power and session action wiring, destructive confirmation, accessible
roles/descriptions, disabled internal truth, and an
always-admitted focus target. Boundary/poison and installed-route rows prove
the source and relocated package boundaries. Settings Center tests additionally
cover eighth-route order, Ctrl+8, PageTab semantics, Escape/Tab entry, and
exclusive wide/compact loaders.

No row contacts a host session/system bus, UPower, power-profiles-daemon,
login1, ScreenSaver, sysfs, Wayland, or hardware. Screen-lock rows use
temporary `kscreenlockerrc` copies and a fake live-configure client. This
slice does not claim live host action success, live screen-locker preference
adoption, internal-display mutation, hold acquisition/release,
charge thresholds, persistence beyond the bounded screen-lock preference
file, live AT-SPI, physical brightness keys, or nested-session visuals.

## Recovery presentation

Opening Power activates its installed service through the public client. The
page omits epoch/revision counters and uses power-mode and brightness language
in place of protocol terms. Internal display brightness remains an explicitly
read-only percentage and explanation, rather than a permanently disabled
slider; the existing Power1 contract has no internal-display write operation.
Keyboard brightness retains its supported control.
