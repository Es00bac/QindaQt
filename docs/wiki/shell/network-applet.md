# Network applet

`src/shell/network_applet` owns the panel Network applet,
`qindaqt.applets.network`. It is the desktop's first panel view of the network:
the current wired, Wi-Fi, or mobile connection, a Wi-Fi on/off switch, the
visible Wi-Fi networks, Rescan, and a link to the Settings Network page. The
durable decision is
[ADR-0258](../adr/0258-network-panel-applet-over-public-network1.md).

The applet is a consumer of the public Network1 client, exactly like the
[Settings Network route](../apps/network-settings.md) (ADR-0055). It adds no service,
adapter, or NetworkManager work, and it has no credential surface: a secured
network that needs a password is answered in the separate network secret
agent's own prompt ([ADR-0069](../adr/0069-confine-network-credential-entry.md)).

Current maturity: **built-in composition with deterministic fake-transport,
offscreen keyboard, manifest, registry, and profile evidence**. A live check on
real hardware (switch Wi-Fi off and on, move between saved networks, join a
new secured network, unplug a wired link) has not been recorded yet.

## Modules

| Part | Path | Responsibility |
| --- | --- | --- |
| Pure values | `include/qindaqt/shell/network_applet/*`, `src/network_applet_presentation.cpp`, `src/network_request_state.cpp` | Project one `NetworkModel` into bounded rows; the request state machine. Links only the Network1 model and protocol. |
| Runtime controller | `src/network_applet_controller.{h,cpp}`, `src/network_applet_actions.cpp` | Borrow the shell's `NetworkClient`, re-validate each intent, keep one request, publish QML values. |
| QML | `qml/NetworkApplet.qml`, `NetworkConnectionRow.qml`, `NetworkAccessPointRow.qml` | Compiled `QindaQt.Shell.NetworkApplet 1.0` on the shared [controls](controls.md) and [panel popup](panel-popup-placement.md). |
| Composition | `src/shell/runtime/networkappletcomposition.{h,cpp}` | Evaluate the audited manifest, registry, and policy once; own the Qt D-Bus transport and client; inject the Settings route. |

The runtime target links `QindaQt::NetworkClient` only. The Qt D-Bus transport
is owned by shell composition, and the boundary test
(`qindaqt.network-applet-boundary`) rejects D-Bus, the service, the
NetworkManager adapter, the secret agent, process starts, and any text-entry
control in the applet sources.

## What the panel shows

The glyph is a closed vocabulary (`indicatorIconName`), each value also spelled
out in the accessible name so no state depends on the icon alone:

| State | Glyph | Accessible name |
| --- | --- | --- |
| Service loading, absent, or not current | `network-offline` | "Network information is loading" / "Network is unavailable" |
| Wired connection active | `network-wired` | "Network: connected by wire" |
| Wi-Fi connected, strength known | `network-wireless-signal-{excellent,good,ok,weak,none}` (75/50/25/1 %) | "Network: connected to *name* over Wi-Fi, signal *n*%" |
| Wi-Fi connected, strength unknown | `network-wireless` | "Network: connected to *name* over Wi-Fi" |
| Mobile broadband active | `network-wireless` | "Network: connected by mobile broadband" |
| No connection, Wi-Fi radio off | `network-wireless-off` | "Network: Wi-Fi is off" |
| No connection | `network-wireless-disconnected`, or `network-offline` without a Wi-Fi device | "Network: not connected" |

Wired wins over Wi-Fi, which wins over mobile, when several are active. Every
name exists in the QindaQt icon theme and in the pinned Breeze fixture; the
`qindaqt.shell-icon-coverage` row enforces both. Connectivity (internet
available, sign-in required, limited, no internet access) is added to the
accessible description only while a connection exists; unknown connectivity
is omitted rather than guessed.

## The popup

- **Radios.** One switch per radio Network1 reports as present. Only Wi-Fi and
  mobile broadband exist as Network1 radio kinds (ADR-0251); airplane mode is
  a session key controller, not a Network1 radio, and is not offered here. A
  switch is disabled when the radio is hardware-blocked, the capability is
  absent, the client is refreshing, or any request is in flight.
- **Connected.** Each active connection with a Disconnect button.
- **Wi-Fi networks.** One row per visible (SSID, security) pair, the strongest
  access point of that network first; hidden networks are omitted because
  Network1 cannot join them from a visible row. Each row states its signal,
  whether it is saved, and whether it is secured, in words. Connect uses
  `connectKnownNetwork` when a saved profile matches the SSID and security,
  otherwise `connectVisibleNetwork` with only the opaque access-point id.
  Enterprise and WEP networks stay visible but are not joinable here; that is
  Network Settings' job.
- **Rescan** (`requestScan`, 30 s deadline) beside the list heading.
- **Network Settings…** opens the Settings `network` route through the
  shell's `SettingsRouteLauncher`. It appears only when the shell injected
  that launcher.

The popup closes on Escape, and every control is reachable and operable with
the keyboard alone. It works on horizontal and vertical panels and at the
compact panel size; placement belongs to `QindaQt.Controls.PanelPopup`.

## Exact-owner projection

Rows exist only while all of these hold together: `network.read` was granted;
the client has a current snapshot (`snapshotCurrent()`); and the client is
Ready or Degraded. Owner loss or replacement, a timed-out refresh, or read
denial clears every row and shows the unavailable glyph. A Degraded service
keeps current rows and shows its diagnostic.

Every `can*` flag is the Network1 model's own intent verdict for that exact
row, gated by `network.control` and by `NetworkClient::operationAdmissionReady()`
and by the applet having no request in flight. The controller re-finds each
row in the current model when QML invokes it; it never trusts row state sent
back from QML.

## Requests: pending, confirming, and outcomes

The applet holds at most one request, because `operationFinished` carries no
request id. Each request records the owner, epoch, and revision it was
dispatched against.

| Phase | Meaning | Shown as |
| --- | --- | --- |
| Pending | Dispatched; no reply yet | "Connecting to *name*…", "Turning Wi-Fi off…", … |
| Confirming | Network1 accepted it; waiting for truth | the same pending text; controls stay fenced |
| Succeeded | A newer same-owner, same-epoch snapshot shows the requested state | "Connected to *name*.", "Wi-Fi is off." |
| Failed | Refused locally or by Network1, or a radio snapshot contradicts the request | "Could not connect to *name*: *reason*." |
| Uncertain | Mismatched or unreadable reply, transport failure or timeout, owner change, or no confirmation within the window | "… Check the current state before trying again." |

An accepted reply is not success. While confirming, the controller asks the
client for a fresh snapshot once a second (a read) and gives up as uncertain
after 15 s, or 90 s for a connection, which can wait on the user typing a
password into the secret agent's prompt. A Scan is the exception: its accepted
reply is the outcome, because the list itself updates as results arrive.

Nothing is replayed. Failure and uncertainty retire the request; the user must
act again. A settled message clears when the popup closes, and a Succeeded or
Failed message clears when the Network1 owner changes, so it never reads as a
fact about the new owner.

## System Status

[System Status](desktop-controls.md) borrows the same controller for its
network lane: the lane's glyph, summary, and accessible text are the facade's
own values, and its quick control is the Wi-Fi switch, which calls the same
`requestRadio`. A failed or uncertain request marks the lane for attention.

## Production seams

- Manifest `data/applets/network.json` requests `network.read` and
  `network.control`, both new capability tokens in schema v1
  ([manifest schema](../reference/applet-manifest-schema-v1.md)).
- `BuiltinAppletRegistry::firstParty()` lists `qindaqt.applets.network`.
- `ShellRuntimeApplication::initializeServiceAppletCompositions` builds the
  composition; `RuntimePanelWindowFactory::setNetworkAppletAccess` hands the
  controller to panel QML as `networkAppletAccess`, and
  `BuiltinAppletContent.qml` renders it.
- The composition attaches the Settings route where the Voice applet does, and
  is released with the other applet compositions after panel windows and
  desktop controls.
- The default `qindaqt` profile places the applet in the center zone next to
  Bluetooth and Power. The gnome-, macos-, and unity-inspired profiles place no
  standalone Bluetooth or Power applet either; they show the network through
  their System Status instance ([layout profiles](layout-profiles.md)).

## Focused verification

| Row | Covers |
| --- | --- |
| `qindaqt.network-applet-presentation` | rows, ordering, collapse of duplicate BSSIDs, glyphs, wired/Wi-Fi/radio-off/no-device indicators, currency loss, grant and admission gates |
| `qindaqt.network-applet-request-state` | confirm-from-newer-truth, visible-network recognition, radio conflict, stale or foreign replies, refusal text, owner loss, deadline, no late input |
| `qindaqt.network-applet-controller` | fake Network1: connect saved and new, refusal, uncertain transport failure without replay, owner replacement, owner loss, radio confirm and hardware refusal, no Wi-Fi device, service absent, deadline, rescan, disconnect, control denial, Settings launch |
| `qindaqt.network-applet-offscreen` | compiled QML under `QT_FATAL_WARNINGS`: keyboard join, switch shows only confirmed truth, Escape, compact vertical panel, Rescan, Settings link, accessible roles and names |
| `qindaqt.network-applet-boundary` | no D-Bus, service, adapter, secret-agent, or process reach; no text-entry control; typed intents only |
| `qindaqt.desktop-controls-system-status-network` | the System Status network lane mirrors the facade and honors its own grants |
| `qindaqt.applet-catalog`, `qindaqt.applet-runtime-resolution` | manifest, capabilities, registry, and stock and vertical placements |

## Non-claims

- No hidden-network, enterprise, WEP, VPN, hotspot, or wired-profile editing;
  those stay in Network Settings.
- The applet does not check whether the secret agent is running before joining
  a secured network (the Settings route does). If no agent answers, the
  connection is reported uncertain after the confirmation window rather than
  as a password error.
- A connection that NetworkManager abandons (for example a cancelled password
  prompt) is reported uncertain at the deadline, not as a specific failure.
- No live-hardware or nested-session evidence is recorded yet.
