# QindaQt Settings — Bluetooth route

`qindaqt-settings --page bluetooth` is the first-party Bluetooth inventory and
connection surface. It composes only the public Bluetooth1 client and keeps
BlueZ, the resident service implementation, D-Bus transport, and hardware out
of the route boundary. The route offers power, discovery, and connection
intents only when its current exact public snapshot admits them.

## Truth shown by the route

The page distinguishes loading, ready, degraded, unavailable, and pending
states. Authoritative ready truth is presented in two inventories:

| Group | Public truth | Interaction |
| --- | --- | --- |
| Adapters | Bounded presentation name, powered state, discovery state, and whether this route owns the discovery request | Power on/off and acquire/release this route's discovery lease when admitted |
| Devices | Bounded presentation name, device class and semantic icon, paired/connected state, and RSSI when known | Connect or disconnect only an already-paired device when admitted |

Rows use route-local opaque identifiers derived from public handles. Bluetooth
hardware addresses do not cross the QML boundary; an address-shaped platform
alias is replaced by a class or ordinal fallback. The page never renders the
public broker's technical owner identifier even though the model retains it
for fencing and focused diagnostics.

## Owner, epoch, revision, and pending fences

One process-lifetime route model projects one same-thread public
`BluetoothClient`. Ready state requires one validated snapshot tied to the
client's exact current owner, nonzero epoch, and nonzero revision. Owner loss,
degraded truth, malformed truth, or a replacement awaiting its first accepted
snapshot clears actionable rows and closes admission.

Every displayed action availability and the final dispatch use the same route
admission predicate. Admission checks route activity, exact ready truth,
current handle epoch, public capabilities, adapter power, paired/connected
state, discovery ownership, and the absence of another pending operation. A
submitted request pins its owner, epoch, and initiating revision. Its success
does not optimistically edit rows: controls remain fenced until a matching
authoritative snapshot reaches the result's minimum revision. Late or
mismatched completions cannot reopen availability. Failures remain visible and
uncertain work is never automatically replayed.

## Discovery lease lifetime

The route owns at most one caller-scoped discovery lease. Start Discovery
acquires it through the public client; Stop Discovery releases only the exact
lease held by this route. Navigating away, closing the Settings window, or
destroying the model requests the same serialized release. Window close waits
while an admitted acquire/release is outstanding. If departure occurs
while acquisition is pending, release follows only after that exact acquisition
completes successfully. Owner replacement retires the old lease locally
because no request may be sent to a different owner. A failed or uncertain
release is surfaced and not replayed automatically. When public mutation
authority is already unavailable, closing the process relies on Bluetooth1's
caller-disappearance cleanup instead of trapping the window on an inadmissible
request.

This is reference-counted discovery intent, not ownership of the adapter's
aggregate `discovering` value. Another caller may keep discovery active after
this page releases its own reference.

## Pairing and trust boundary

Pairing, trust, untrust, and removal stay in BlueZ as established by
[ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md). The page states
that limit visibly and has no corresponding model invokable. Unpaired devices
remain inventory-only. Neither QML nor the model imports private Bluetooth
service headers, Qt D-Bus, BlueZ APIs, or another application's internals. See
[Bluetooth service architecture](../architecture/bluetooth-service.md) and the
[Bluetooth1 reference](../reference/bluetooth1-v1.md) for the public contract.

## Responsive interaction and accessibility

The route uses QST-1 tokens and QindaQt.Controls in both wide and compact
Settings hosts. Its ordered content is vertically scrollable; Page Up/Page
Down and Ctrl+Home/Ctrl+End move through it, and keyboard focus is revealed.
Adapter switches and discovery buttons expose accessible names, descriptions,
roles, and checked/disabled state. Device actions announce class, pairing,
connection, and signal truth. Busy, unavailable, degraded, error, and authority
states are visible text rather than color-only cues.

The declared host-entry focus target is the Close action. It is always enabled
and admitted, including empty, unavailable, degraded, and busy states, so Tab
from the active route tab never lands on a disabled domain action. Escape
returns focus to the active wide or compact Bluetooth tab. Ctrl+5 selects the
route, while the platform Quit shortcut closes the window after requesting
discovery release.

## Composition and package boundary

The closed Settings registry maps only canonical `bluetooth` to the compiled
Bluetooth component. Unknown or path-like startup values are rejected before
constructing the transport, client, or model. The executable owns one public
Qt Bluetooth transport on the session bus, one client, and one route model for
its process lifetime; only the QObject projection crosses into QML.

The existing `SettingsAppearanceRuntime` component installs the Bluetooth QML
module and linked public client/transport libraries with relative runtime
paths. The dedicated package fixture stages that component, proves the
developer-tree module cannot mask a withheld installed Bluetooth module, then
restores it and launches the relocated route with private buses unavailable.

## Verification and stopping point

Focused Bluetooth selection:

```sh
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir build/dev --output-on-failure --no-tests=error \
  -R '^qindaqt\.settings-bluetooth-'
```

- the model row uses the injected fake Bluetooth transport to prove bounded,
  address-free projection, class/icon/RSSI truth, shared admission, paired-only
  connections, pending convergence, exact-owner replacement, and departure
  release;
- the warning-fatal page rows render wide and compact offscreen/software scenes
  and proves accessible controls, disabled truth, shortcut-independent route
  focus entry, action wiring, authority disclosure, and window-close waiting
  for a discovery release;
- boundary and negative-control rows enforce an allow-list-only include scan
  and reject sibling app internals, parent escapes, private service headers,
  and pairing authority; and
- the installed row proves real relocation, deliberate missing-module failure,
  and bounded construction with neither host D-Bus nor radios.

Settings Center's selector additionally proves route order, canonical startup,
Ctrl+5, PageTab accessibility, Escape/Tab focus, responsive Loader exclusivity,
and the common relocated package. Both selectors run in strict Debug and
Release profiles.

This slice does not claim pairing, trust changes, device removal, persistent
preferences, live BlueZ or radio behavior, host-bus integration, live AT-SPI,
screen-reader traversal, or nested-session screenshots.
