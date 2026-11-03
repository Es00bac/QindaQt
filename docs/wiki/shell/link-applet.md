# QindaLink applet

`src/shell/link_applet` owns the panel QindaLink applet,
`qindaqt.applets.link`: the paired phone's connection and battery at a glance,
and the desktop's pairing UI. The decision is
[ADR-0437](../adr/0437-qindalink-panel-applet-and-phone-capability-tokens.md);
the service is [QindaLink](../architecture/qindalink.md) and its public API is
[Link1](../reference/link1-v1.md).

Current maturity: **built-in composition with deterministic fake-transport and
offscreen keyboard evidence.** No live check against `qindaqt-link-service` or
a phone has been recorded.

## Modules

| Part | Path | Responsibility |
| --- | --- | --- |
| Pure values | `include/qindaqt/shell/link_applet/*`, `src/link_applet_presentation.cpp` | Project `(service present, devices, pending pairing, now)` into one of five states and their text. |
| Runtime controller | `src/link_applet_controller.{h,cpp}` | Borrow the shell's `LinkClient`, hold the pending pairing, publish bounded QML values, forward the typed actions. |
| QML | `qml/LinkApplet.qml` | Compiled `QindaQt.Shell.LinkApplet 1.0` on the shared [controls](controls.md) and [panel popup](panel-popup-placement.md). |
| Composition | `src/shell/runtime/linkappletcomposition.{h,cpp}` | Evaluate the manifest, registry and policy once; own the Qt D-Bus transport and client (neither on a denied `phone.read`); wire "Open QindaLink" to the launcher. |

## States

| State | When | Tint role |
| --- | --- | --- |
| Unavailable | service absent, or `phone.read` denied | `muted` |
| No phone | service present, no paired device | `muted` |
| Pairing request | a valid six-digit code is pending | `warning`, with a fixed ring |
| Connected | paired phone connected and READY | `success` |
| Offline | paired phone not connected | `muted` |

A pending pairing outranks everything else. The primary phone is the paired
device that is READY, else connected, else first listed. A code that is not
exactly six ASCII digits is dropped; the phone's name and model are stripped of
controls and bidi overrides, capped at 64 characters, and rendered as
`Text.PlainText`. The glyph is `phone-symbolic` (shipped in the QindaQt theme)
tinted by role; a badge slot (`unreadCount`) is hidden until the SMS lane
feeds it.

## Popup

- **Pairing:** the phone's name and model, the code as `123 456`, and Reject and
  Accept. Focus lands on Reject; Accept is never the default.
- **Connected:** connection (USB or Wi-Fi), battery with a charging mark, and
  network type with signal 0-4 when known. Offline shows "Last seen N min ago".
- **Ring phone:** enabled only when the phone is connected and its device
  snapshot lists the `ring` capability. Link1 does not emit capabilities yet,
  so it is disabled today.
- **Open QindaLink:** `activate("org.qindaqt.Link.desktop")` through the shell
  launcher, hidden when the catalog has no such entry.
- **Unpair:** asks "Forget this phone?" before calling `Unpair`.

## Seams and first run

The controller reacts to `availableChanged`, `devicesChanged` and
`pairingRequested`; nothing reads the service once. A pending pairing is
dropped when the service vanishes or the phone stops being a live unpaired
connection. A request raised before the shell started is not shown (Link1 has
no pending-pairing property); the service notification still carries it.

## Verification

| Row | Covers |
| --- | --- |
| `qindaqt.link-applet-presentation` | all five states, code formatting and rejection, sanitizing, ring gate, primary selection |
| `qindaqt.link-applet-controller` | owner appears late or vanishes, pairing accept/reject/lifetime, denied read, denied control, ring gate, unpair, launcher seam |
| `qindaqt.link-applet-offscreen` | compiled QML under `QT_FATAL_WARNINGS`: chip text, pairing popup focus and keyboard accept/reject, disabled ring, two-step unpair |
| `qindaqt.link-applet-boundary` | no QDBus, transport or process reach, only typed `LinkClient` calls |
| `qindaqt.applet-manifest`, `qindaqt.applet-catalog`, `qindaqt.applet-runtime-resolution` | count guards, `phone.*` tokens, registry |
| `qindaqt.shell-panel-access-threading` | `linkAppletAccess` declared and forwarded |
