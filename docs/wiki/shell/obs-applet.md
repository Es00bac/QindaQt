# OBS applet

## 2026-09-19 source diagnosis

Inspection of base `01e919f6` found three remaining control defects. This
record precedes the corrections; compilation and runtime checks are deferred
until the coordinated desktop code is ready.

- `QtObsTransport` forwards the socket's close text but discards its numeric
  close code; `ObsClient::handleDisconnected` then ignores that text. The
  declared `obs-auth-rejected` reason is never produced, so OBS rejecting a
  password looks like OBS being absent. The
  [obs-websocket protocol](https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md#websocketclosecodeauthenticationfailed)
  specifies authentication failure as close code 4009.
- The popup's unlabeled switches change their checked state locally before
  OBS answers. A refused operation need not change the authoritative active
  value, so the switch can keep showing the requested state. The controller
  also discards a zero request id and admits repeated presses while a request
  is outstanding. Explicit Start/Stop actions, tracked request completion,
  and a visible pending message are the bounded correction.
- The chip computes a summary such as Streaming but never renders it; its
  two most consequential states share the same icon and colour. The popup
  has no direct route to the Streaming settings it tells users to open.
  The action status must be readable beside the icon and setup must be one
  action away.

The profile placement and first-setup watch were already fixed by `2c6137e6`.
The PulseAudio monitor names used by the bridge match upstream OBS's capture
implementation; they are not a newly discovered device-name defect.

The OBS applet is a top-bar chip that says what OBS is doing and a popup with
the controls a user reaches for mid-session. Everything else about OBS stays
in OBS, and everything about setting OBS up stays in
[Settings → Streaming](../apps/streaming-settings.md).

## The chip

The glyph states the most consequential thing OBS is doing: **streaming**
outranks **recording**, which outranks the **virtual camera**. A user who is
live needs to see that first, whatever else OBS is also doing. The chip takes
the accent colour exactly while OBS is recording or streaming.

When OBS is not reachable the chip stays, and its accessible description says
why — "OBS is not running", "OBS did not accept QindaQt's password", or that
OBS speaks a control protocol QindaQt does not.

## The popup

| Control | What it does |
| --- | --- |
| Recording | Start or stop recording, with elapsed time while it runs |
| Streaming | Start or stop streaming, with elapsed time while it runs |
| Virtual camera | Start or stop the virtual camera |
| Scene list | Make one of OBS's scenes live |
| Open OBS | Open the OBS window, for everything this popup deliberately does not do |

A control here is enabled exactly when it is dispatchable. Pressing one while
OBS is unreachable reports why rather than doing nothing silently. A refused
request is shown in OBS's own words.

A dropped-frame warning appears only while a stream is running and only when
OBS reported frame counts showing at least 1% dropped.

## Where the chip lives

The chip ships on the panel of the **QindaQt** profile only, in the end zone
beside the other hardware chips — audio, bluetooth, power, smart lights. The
desktop-imitation profiles (GNOME, macOS, Unity, XFCE, MATE, minimal) carry none
of those by design, and a chip reading "OBS is not running" on a GNOME-imitation
top bar for someone who never installed OBS is clutter rather than discovery.

An existing layout — one the user has already customized, or one taken from
another profile — does not gain the chip automatically. Add it in place with
**Meta+right-click → Add applet → OBS**.

`qindaqt.profile-formats` fails if the flagship profile stops placing it. That
row exists because the applet first shipped with its manifest, capabilities,
policy grant and controller all correct and no profile putting it anywhere, so
there was nothing on screen to connect and nothing to notice was missing.

## Capabilities and composition

The applet is an audited built-in declaring `streaming.read` and
`streaming.control`. Both are granted to the built-in package and denied to
every third-party package in `data/applet-policy/default.json`: what a user is
recording, and the scene names they chose, are private, and starting a
capture on their behalf is not something an unaudited applet may do.

`ObsAppletComposition` owns the obs-websocket client. **It starts the client
only when the read capability is granted and a password is already in the
keyring** — connecting without one would make OBS's refusal look like a wrong
password the user chose, and would retry against it forever. Without the
control grant the controller is still built, so the panel the user configured
keeps its chip and reports honestly that OBS cannot be driven.

The password reaches the client and nothing else: it is never held by the
controller, never published into QML, and never logged
([ADR-0201](../adr/0201-one-obs-websocket-client-for-the-desktop.md)).

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.shell-obs-applet-presentation` | The glyph precedence, the four unavailable sentences, elapsed text, the dropped-frame threshold, and the accessible description naming every running output |
| `qindaqt.shell-obs-applet-controller` | Every control reporting why it did nothing without a client, toggles dispatching the opposite of what OBS reports, a refused request in OBS's words, and scene selection skipping the live scene |
