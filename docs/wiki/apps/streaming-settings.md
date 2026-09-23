# Settings Streaming route

## 2026-09-19 source diagnosis

The route shares the applet's discarded OBS authentication close-code path
and also presents output commands as locally toggled switches. It admits
repeat output/scene commands without tracking an outstanding request and
shows a connection-state token without the client's reason. This inspection
precedes the route correction: named compact switches that restore OBS's
authoritative checked state, one outstanding output/scene command, and a
readable connection diagnosis. The shared client
also needs to keep paused recordings and reconnecting streams active, as
recorded in the [OBS applet diagnosis](../shell/obs-applet.md).

The coordinated production build and existing
`qindaqt.settings-streaming-model` row subsequently passed. Live authenticated
connection and output-state behavior remain focused session observations;
the model row alone does not qualify every corrected state transition.

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

## The first run, and the Screen source

Setting OBS up does not require restarting the shell. The applet re-checks for
OBS's control configuration on a slow watch throughout the session. It
connects after setup only when a confirmed Settings1 baseline enables
auto-connect and the selected port matches OBS's configured active port.
The ordinary order of events is that the desktop is already running when
the user presses **Set up OBS** here. Until OBS's own config says the
control server is enabled, nothing asks the keyring for the password at all, so
a desktop without OBS never touches the Secret Service on this path.

The provisioned scene collection includes a **Screen** source. On Wayland,
opening it for the first time raises the portal's "Share screen with" picker,
and OBS shows nothing until a screen is chosen there. That is the portal doing
its job — screen capture is a permission the compositor grants, not something
an application can take — and it is expected on first run, once per source.

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
- `src/services/streaming_preferences` holds the purpose-scoped Settings1
  preference policy and the bounded OBS login helper.
- `src/apps/settings/streaming` holds the route model and the
  `StreamingRouteComposition` QML singleton that
  builds the production adapters, and the QML pages. The model writes no OBS
  file itself.
- Settings Center registers `streaming` as the last route, so no existing
  route index or `Ctrl`+digit shortcut moves (ADR-0128).

## Preferences

[ADR-0248](../adr/0248-confirm-streaming-preferences-before-obs-consumption.md)
makes these values authoritative only after a Settings1 snapshot confirms
them. While one save is pending, the controls wait; a rejection leaves the
previous value on screen with the server message, and an uncertain timeout or
owner change asks for a fresh read instead of replaying. A same-lineage
stale snapshot cannot settle an accepted save: the client waits for at least
the commit revision, then reports uncertainty if fresh readback never arrives. The page waits for
the first baseline before connecting, setting OBS up, or saving. A selected
new port changes the address QindaQt will use after confirmation, but OBS's
active port changes only after **Repair OBS setup** and an OBS restart.

The Start OBS at login switch saves the preference; one system XDG entry
runs `qindaqt-obs-login` through the session's normal autostart runner. The
helper checks a confirmed Settings1 baseline and replaces itself with OBS
only if enabled. The page reports whether that entry is installed, whether
Startup masks it, and whether OBS is executable, so an On switch alone does
not claim that OBS will start. No OBS password enters the login helper.


Three Settings1 keys in the `services` domain:
`services.obsWebSocketPort` (1–65535, default 4455),
`services.obsAutoConnect` (default true) and
`services.obsStartAtLogin` (default false). Settings1 rejects a whole
snapshot on one unknown key (ADR-0126), so the route uses a client scoped to
exactly these three and the resident settings service must know them before
the route can save. The installed login entry is system-owned and may be
masked by a user override in Startup; the Streaming page shows that effective
status. A custom XDG_CONFIG_DIRS that excludes /etc/xdg must include the
installed entry elsewhere.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.services-obs-protocol` | Frame decode and every refusal, the documented authentication digest, identify/request/vendor encoding, the bridge's console-mapping payload |
| `qindaqt.services-obs-client` | Identify then one read of everything, events keeping the snapshot current, unknown replies ignored, refused auth and unsupported RPC named, malformed frames degrading and closing, a lost connection clearing live state and backing off, timeouts answering the caller, statistics polled only while an output runs |
| `qindaqt.services-obs-transport` | A real QWebSocket round trip against a miniature obs-websocket server on a private loopback port, and the loopback-only guard |
| `qindaqt.services-obs-provisioning` | The generated password, obs-websocket's own key names, the three files and nothing else, refusals that would leave the server unusable, what a check names as missing, and that reading settings never returns the password |
| `qindaqt.services-obs-secret-store` | The scoped attributes as `a{ss}`, replace-on-store, an absent secret as the first-run state, and an unreachable keyring as an error rather than an empty password |
| `qindaqt.services-streaming-preferences` | Rejected writes retaining confirmed values, serialized pending save, revision-ordered readback, permanently stale timeout, owner replacement without replay |
| `qindaqt.services-obs-login-helper` | Private Settings1 and fake OBS, true/false fresh login, external edit with Settings closed, cancellation and no-owner timeout |
| `qindaqt.shell-obs-applet-connection` | No early connection, confirmed auto-connect and active-port gate, one start per stable state, disable/port replacement |
| `qindaqt.settings-streaming-model` | The loopback address, connecting without a stored password, an unreadable keyring, set-up generating and storing once, elapsed text, the dropped-frame threshold, the three bridge sentences, toggles while disconnected, delayed baseline/selected port, the effective login mask, and the bus table from the vendor payload |
| `qindaqt.settings-streaming-page` | Mouse toggles restoring the confirmed state and showing a rejection instead of a false saved value |
