# Settings Streaming route

The Streaming route (`qindaqt-settings --page streaming`) is where OBS is set
up and where the desktop's recording, streaming and virtual-camera controls
live in full. OBS stays the authority for everything it owns;
[ADR-0201](../adr/0201-one-obs-websocket-client-for-the-desktop.md) records
the control client and
[ADR-0202](../adr/0202-qindaqt-provisions-obs-and-owns-one-secret.md) records
what QindaQt writes into OBS's configuration.

## What the route does

| Section | What it changes | Authority |
| --- | --- | --- |
| OBS | Connect or disconnect; set OBS up or repair the setup; the control port; connect automatically; start OBS at login | obs-websocket over the loopback control port; the QindaQt profile, scene collection and websocket settings on disk |
| Recording, streaming and the camera | Start and stop recording, streaming and the virtual camera; switch the live scene | OBS, through typed obs-websocket requests |
| Console buses in OBS | Nothing — it states which console buses and strips OBS can record | The QindaQt OBS bridge plugin's obs-websocket vendor request |

## Setting OBS up

**Set up OBS** writes three things and nothing else:

- `basic/profiles/QindaQt/basic.ini` — a simple-output profile with sensible
  recording defaults. It declares no streaming service and no stream key:
  QindaQt does not know the user's channel.
- `basic/scenes/QindaQt.json` — a desktop capture through the portal and the
  default microphone. The console buses are **not** listed here; the bridge
  plugin creates and removes those to match the audio console (ADR-0190).
- `plugin_config/obs-websocket/config.json` — the server enabled on the
  chosen port, with a generated 32-character password.

The password is stored in the Secret Service, under attributes scoped to this
one secret. Running the button again keeps the existing password rather than
locking the user out of an OBS that already has it. **OBS reads its
configuration at start**, so the route says to restart OBS when it is already
running.

A keyring that cannot be read is reported and stops the operation. It is
never treated as "there is no password yet", because that would overwrite a
credential OBS is already using.

## The control port

QindaQt connects to `ws://127.0.0.1:<port>` and nothing else. obs-websocket
has no transport security, so the route offers a port and states the address
rather than offering a host field; the transport itself refuses a
non-loopback address (ADR-0201).

## Honest status

- "OBS is not running", "OBS did not accept QindaQt's password" and "OBS
  speaks a control protocol QindaQt does not" are different sentences,
  because the user's next action is different.
- Elapsed time shows a dash until OBS reports one. A zero clock would read as
  "it just started".
- The dropped-frame warning appears only when OBS actually reported frame
  counts and the fraction is at least 1%. A warning the numbers cannot
  support is never shown.
- The bus table distinguishes "the bridge plugin is not loaded" from "the
  bridge is loaded and the console has no buses yet" from "the bridge cannot
  reach the audio console".

## Module shape

- `src/services/obs_client` holds the protocol, the transport seam, the
  QWebSocket transport, the client, the provisioning documents and writer,
  and the scoped keyring store. It is shared with the OBS applet.
- `src/apps/settings/streaming` holds the route model, its purpose-scoped
  Settings1 preferences, the `StreamingRouteComposition` QML singleton that
  builds the production adapters, and the QML pages. The model writes no OBS
  file itself.
- Settings Center registers `streaming` as the last route, so no existing
  route index or `Ctrl`+digit shortcut moves (ADR-0128).

## Preferences

Three Settings1 keys in the `services` domain:
`services.obsWebSocketPort` (1–65535, default 4455),
`services.obsAutoConnect` (default true) and
`services.obsStartAtLogin` (default false). Settings1 rejects a whole
snapshot on one unknown key (ADR-0126), so the route uses a client scoped to
exactly these three and the resident settings service must know them before
the route can save.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.services-obs-protocol` | Frame decode and every refusal, the documented authentication digest, identify/request/vendor encoding, the bridge's console-mapping payload |
| `qindaqt.services-obs-client` | Identify then one read of everything, events keeping the snapshot current, unknown replies ignored, refused auth and unsupported RPC named, malformed frames degrading and closing, a lost connection clearing live state and backing off, timeouts answering the caller, statistics polled only while an output runs |
| `qindaqt.services-obs-transport` | A real QWebSocket round trip against a miniature obs-websocket server on a private loopback port, and the loopback-only guard |
| `qindaqt.services-obs-provisioning` | The generated password, obs-websocket's own key names, the three files and nothing else, refusals that would leave the server unusable, what a check names as missing, and that reading settings never returns the password |
| `qindaqt.services-obs-secret-store` | The scoped attributes as `a{ss}`, replace-on-store, an absent secret as the first-run state, and an unreachable keyring as an error rather than an empty password |
| `qindaqt.settings-streaming-model` | The loopback address, connecting without a stored password, an unreadable keyring, set-up generating and storing once, elapsed text, the dropped-frame threshold, the three bridge sentences, toggles while disconnected, and the bus table from the vendor payload |
