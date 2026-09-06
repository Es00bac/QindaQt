# Gabbee interoperability evidence

Bounded, executable evidence that the external dictation assistant Gabbee
(checkout at `/home/cabewse/gabbee`, read-only input to these tests)
interoperates with a QindaQt desktop: synthetic dictation into an ordinary
app and a terminal, focus capture including a grouped window member, and
GlobalShortcuts portal registration/routing.

The evidence assets live in `tests/session/gabbee/`.  Everything they do is
host-safe: no microphone capture (a fixed silent WAV replaces Gabbee's
recorder), no typing tool (PATH is stripped of `dotool`/`xdotool`), no uinput
device, and no synthetic input of any kind.  Gabbee's own mock STT provider
produces one fixed harmless transcript,
`[mock transcript from gabbee-synthetic.wav]`.

## What is real Gabbee code and what is stubbed

| Layer | Evidence path |
| --- | --- |
| STT | Gabbee's `gabbee.stt.mock.MockSpeechToText` (fixed transcript) |
| Recorder | Probe `SyntheticRecorder` writes a fixed silent WAV; never opens an audio device |
| Focus capture/activation | Gabbee's `KWinWindowBackend`/`KWinQtScriptBridge` against the QindaQt compositor's stock `org.kde.KWin` Scripting service |
| Insertion | Gabbee's `TextDeliveryRouter` recovery chain: direct typing fails closed (no tool), IBus/AT-SPI are attempted, clipboard mirror lands via `wl-copy` |
| Global shortcuts | Gabbee's `PortalPushToTalkBinding` against a real `xdg-desktop-portal` frontend |
| Portal backend | Fake KDE backend claiming the reviewed selector's bus name, implementing the installed `org.freedesktop.impl.portal.GlobalShortcuts` contract |

The QindaQt side is always real: the production portal selector conf (a
runtime-staged copy), the real portal frontend, and — in the nested lane —
the real compositor, shell, Text Editor, and Terminal.

## Deterministic host rows (no display, no lane)

`session.gabbee-interop-unit` (skip code 77 when the Gabbee checkout or
`xdg-desktop-portal` are absent) covers the probe contracts — synthetic WAV
fixity, portal-conf staging, evidence schema, host-endpoint hygiene,
injection-free sources — and executes the full GlobalShortcuts chain:

```
private dbus-daemon (config with ONE probe-owned service dir)
  -> staged portals.conf copy adding org.freedesktop.impl.portal.GlobalShortcuts=kde
  -> real /usr/libexec/xdg-desktop-portal
  -> fake backend owning org.freedesktop.impl.portal.desktop.kde
  -> Gabbee's real PortalPushToTalkBinding
```

Assertions: both F5/F6 shortcuts register ("Hold F5 anywhere to talk."), a
backend-emitted `Activated` reaches Gabbee's pressed callback through the
frontend, `Deactivated` reaches released, and the backend journal records
exactly one `CreateSession` + `BindShortcuts` with Gabbee's shortcut ids.

AGENT-GUARD: the private bus config exists because a stock session bus
D-Bus-activates the host-installed real KDE backend from
`/usr/share/dbus-1/services`, which would silently steal the reviewed
selector's bus name.  The staging helper also refuses to run passthrough if
the integrated conf ever routes GlobalShortcuts anywhere other than the
reviewed `kde`-only line (accepted portal candidate 8215a8cd; see
[ADR-0059](../adr/0059-route-unimplemented-portal-families-explicitly.md)
family policy) — the evidence can never widen
routing on its own.

The fake backend implements the installed impl contract exactly:
`CreateSession(o,o,s,a{sv})`, `BindShortcuts(o,o,a(sa{sv}),s,a{sv})`,
`ListShortcuts(o,o,a{sv})`, and `Activated`/`Deactivated` signals whose
session handle is an object path — the frontend drops string-typed handles
silently, which is easy to miss when writing a fake.

## Nested lane row (root-owned private runtime lane)

`tests/session/gabbee/run_gabbee_interop_nested.py` boots the contained
desktop exactly like the `desktop.virtual` rows — bubblewrap namespaces,
private bus, nested KWin `--virtual` with a test scenario (which also enables
the compositor's development control surface) — plus read-only mounts for the
Gabbee checkout and a privately installed `Terminal` component stage.  It
then runs:

1. the same GlobalShortcuts portal chain against the nested session bus;
2. `gabbee_interop_probe.py`: Gabbee captures and activates the focused
   Text Editor window, dictates the synthetic transcript, and the delivery
   result plus AT-SPI/clipboard readback are recorded; the same for the
   Terminal window;
3. the compositor's `DockWindows` development API groups editor + terminal
   into one window container; Gabbee must capture exactly the focused member
   (distinct `window_id`s) and deliver per member.

Typed-character insertion inside the contained lane is deliberately absent:
the compositor's development input injector has a closed key enum (no
character keys) and uinput is unreachable by sandbox design.  Gabbee's
production recovery chain therefore proves the AT-SPI EditableText and
clipboard paths there; typed insertion into a live terminal would require the
explicitly acknowledged host-uinput lane this evidence must never enable.

## Exact invocation (root, with the private-runtime lane allocated)

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
python tests/session/gabbee/run_gabbee_interop_nested.py \
  --build-root <configured-build> \
  --kwin-wayland <qindaqt-kwin-wayland-binary> \
  --plugin-relative <kwin-plugin-relative> \
  --decoration-relative <decoration-plugin-relative> \
  --gabbee-root /home/cabewse/gabbee
```

Preconditions: the desktop stage fixture
(`ctest --test-dir <build> --fixtures-setup desktop_virtual_stage`), a
configured build with the `Terminal` install component, `bwrap`, and the
Gabbee checkout with its venv.  Missing lane acknowledgement exits 77 before
touching anything.  Results land in
`<build>/tests/session/gabbee-results/<run-id>/`.
