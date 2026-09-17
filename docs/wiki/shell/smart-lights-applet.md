# Smart Lights applet

`src/shell/smart_lights_applet` owns the production panel Smart Lights applet.
A pure target projects bounded luminaire rows and evaluates control admission;
a separately linked shell-private controller borrows the public `WizClient` and
the stored configuration, and exposes only owned presentation values to
compiled QML. Neither target opens a socket: the only datagram implementation
lives in `src/services/wiz_udp_transport`.

Current maturity: **first-party built-in composition with focused pure,
runtime, and offscreen coverage**. The applet ships the audited
manifest/registry/policy path, production shell composition, keyboard-accessible
compiled QML, and stored configuration. It has been exercised against real
ESP25_SHRGB_01 luminaires on firmware 1.38.0; the deterministic tests run
without a network.

The architecture decision that keeps this stack in the shell process rather
than behind a resident service is
[ADR-0186](../adr/0186-smart-lights-speak-to-luminaires-from-the-shell-process.md).

## Module boundary

| Target | Owns | Never contains |
| --- | --- | --- |
| `QindaQt::WizProtocol` | Wire format, decode bounds, capability inference, scene table, control admission | I/O, timers, Qt Quick |
| `QindaQt::WizModel` | Device inventory, reachability ladder, epoch/revision accounting | Sockets, timers, policy about what the user may do |
| `QindaQt::WizClient` | Discovery, interrogation, polling, serialized control, retry and timeout policy | A concrete socket; wall-clock time |
| `QindaQt::WizUdpTransport` | The UDP socket, broadcast targets, notification-port binding | Any interpretation of a payload |
| `QindaQt::SmartLightsStore` | Device labels, saved arrangements, atomic file persistence | Device state |
| `QindaQt::ShellSmartLightsApplet` | Row projection, control admission at the presentation boundary | The client, QML, the filesystem |
| `QindaQt::ShellSmartLightsAppletRuntime` | The controller and compiled QML | Direct protocol construction |

## What the desktop learns about a luminaire

Discovery is a vendor `registration` datagram broadcast to every attached IPv4
broadcast domain. Every light on the segment answers it with its own MAC, which
is the only identity the model accepts: a datagram without one is dropped, and
a device is never identified by the address it arrived from. When the transport
holds UDP 38900 the same datagram also subscribes to the light's unsolicited
state pushes; when it does not, the client sends the unsubscribed form rather
than asking a device to push to an address nobody reads.

A push (`syncPilot`) is accepted as fresh state and as proof that the light is
reachable, but never as routing. Firmware 1.38.0 sends pushes from an
ephemeral source port while the light listens only on UDP 38899, so only the
port a *reply* came from becomes a device's control endpoint. The first
installed build learned the port from pushes too, which sent every later poll
and control datagram to a port nothing read: the row kept showing live state
from the pushes while every switch and slider was dead, until the next
discovery answer repaired the port and the next push broke it again. The
decoder marks a `params` payload as unsolicited and the model refuses its port;
all three service rows cover the captured datagram.

A newly seen device is then asked what it is (`getSystemConfig`) and what it
can do (`getModelConfig`). Until both have been answered the device is
projected with power control only:

- the **module name** is the vendor's own product encoding and is the only
  evidence of a channel set that does not depend on the light's current state;
- **cctRange** narrows the white-temperature envelope, and the firmware clamps
  silently outside it, so the reported range is enforced locally instead;
- **minDimLevel** is the brightness floor. A device that has not reported one
  keeps the conservative vendor minimum of ten percent, because older firmware
  answers `Invalid params` below it.

Capability inference never reads the current pilot. A light that happens to
report colour channels is not proof of a colour engine.

## Reachability

Poll rounds are counted per device. Two consecutive unanswered rounds mark a
light `Stale`; four mark it `Unreachable` and drop its pilot, because a silent
device is no longer proof of the state it last reported. A device is never
removed automatically: its stored name and its membership in saved arrangements
survive a power cut, a Wi-Fi drop, and a DHCP address change.

## Control admission

Every intent passes `validateStateRequest` before a datagram is built, and the
applet runs the same admission again at the presentation boundary. Three rules
are enforced here because the firmware will not enforce them:

- colour, white temperature, and scene are mutually exclusive lanes in one
  `setPilot`; the strongest requested lane wins and the others are dropped;
- switching a light off is sent alone, so a light being switched off does not
  also have its stored colour rewritten; and
- playback speed is refused unless an animated programme is playing, since the
  firmware answers `Invalid params` rather than ignoring it.

Control intents are queued and executed one at a time, so a dragged slider
cannot flood the network or interleave two states on one luminaire. An intent
that is not acknowledged is retried exactly once and then reported as
uncertain. It is never replayed automatically: the light may already have
applied it, and repeating it would fight the user's next action.

## Rows, tokens, and grants

The controller publishes rows only while all of these facts hold together:

- `smart-lights.read` was granted by the audited manifest/policy evaluation;
- the client is `Ready` with a started transport; and
- the device answered within the reachability window.

QML receives session-scoped `light-<n>` tokens, never a MAC address. The
mapping lives in the controller and is retired with the device, so a stale row
cannot address a luminaire that was discovered again later. Control-shaped
fields carry their own permission: a row that renders an enabled control is one
whose intent the client will accept, and `smart-lights.control` denial makes
every control inert while the rows stay readable.

The panel chip uses the freedesktop `brightness-high` and `brightness-low`
names, which resolve in Breeze as well as the
[QindaQt icon theme](icon-theme.md), so the chip is never a blank square.

## Stored configuration

`GenericConfigLocation/qindaqt/smart-lights.json` holds what belongs to the
desktop rather than to the device: a label per luminaire, the address it last
answered on, and named arrangements. Writes are atomic. A document this build
cannot decode — a newer schema, say — is never overwritten in place; it is
moved aside to `smart-lights.json.invalid` on the first save, and the user is
told the file could not be read.

An arrangement stores intents, not observed pilots, so replaying one issues the
same request that was captured. A light that is unplugged when an arrangement
is applied is skipped; the rest of the room still changes. Forgetting a light
removes it from every arrangement, and an arrangement with no members left goes
with it.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.wiz-protocol` | Decode bounds, hostile values, push versus reply, capability inference, lane collapse, clamping |
| `qindaqt.wiz-model` | Identity, revision discipline, reachability ladder, push source ports never become endpoints, ordering, inventory bound |
| `qindaqt.wiz-client` | Discovery, interrogation, acknowledgement, device error, retry/uncertainty, serialization, control port survives a push, transport loss |
| `qindaqt.smart-lights-store` | Round trip, schema refusal, partial-document handling, atomic save, quarantine |
| `qindaqt.smart-lights-applet-presentation` | Grant gating, capability projection, summary counting, preset applicability |
| `qindaqt.smart-lights-applet-request-state` | Admission, lineage matching, uncertainty, authority loss |
| `qindaqt.smart-lights-applet-controller` | Opaque tokens, dispatch, persistence across sessions, presets, forget |
| `qindaqt.smart-lights-applet-offscreen` | Keyboard activation of the chip and controls, inert controls without the grant |
