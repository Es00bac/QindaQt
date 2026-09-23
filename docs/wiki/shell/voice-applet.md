# Voice applet

The Voice applet is the panel's microphone: a chip that says whether voice
input is armed, shows that the microphone is open and what it is hearing, and
opens a popup with the last dictation and what can be done about it.

It consumes [`org.qindaqt.Voice1`](../architecture/voice-input.md) only after Settings1 confirms the desktop's voice-input preference On.
It never speaks to a speech provider, touches a microphone, or learns the
provider's process identity.

## The chip

| State | Chip |
| --- | --- |
| No provider | Muted microphone, dimmed, "Voice off". |
| Connecting | Muted microphone, "Voice…". |
| Armed and idle | Plain microphone, "Voice". |
| Disarmed | Muted microphone, "Voice off". |
| Arming / listening | Microphone with sound arcs in the accent colour, a segmented level meter, and the capture state as text. |
| Transcribing / inserting | The same lit microphone, "Transcribing" / "Inserting". |
| Failed | Muted microphone tinted with the danger colour, "Voice error". |

The microphone pulses while capture is live and stops the moment it ends, so a
stuck animation is itself a bug report rather than ambient decoration. The
meter is drawn from plain `Item`s, never `Canvas`, because Canvas content does
not paint under the offscreen capture this surface is reviewed with.

The level is only honoured while the snapshot says capture is live. A meter
that keeps moving after capture ends reads as a hot microphone, so both the
client and the projection zero it.

## The popup

- **Status line** — what the provider is doing, and the provider and language
  beneath it.
- **Armed switch** — arms or disarms push-to-talk across the whole desktop. It
  reflects the provider, not the click: it is re-bound from the projection on
  every change, so a refused request snaps back instead of lying.
- **Transcript block** — the live partial while dictating, in the accent
  colour, and the last delivered text otherwise. Four lines, then elided.
- **Provenance** — which route placed the last text, when capture is over.
- **Actions** — Dictate, Command, Finish, Cancel, Retry, Undo, Copy. An action
  the provider cannot do is absent; an action it can do but that is not
  currently valid is present and dimmed.
- **Shortcut hint** — "Hold F5 to dictate, F6 for a command."
- **Microphone** — which input device the provider is using.
- **Status/feedback line** with a Dismiss control.
- **Open Voice** and **Voice settings** — the console and the Settings route.
  Each is available only when the composing shell supplied a launcher, so no
  dead affordance is ever shown.

## Composition

`VoiceAppletComposition` (shell-private) owns the `QtVoiceTransport`, the
`VoiceClient`, the `VoiceServiceClientAdapter` seam and the controller, in that
destruction order. It evaluates the manifest, the built-in registry and the
capability policy once, independently of panel-window reconstruction.

**The client is started only when voice.read is granted and Settings1 has
confirmed services.voiceInput On for its current owner.** Default Off, a
missing baseline, and owner loss leave the provider untouched and the panel's
actions unavailable. A confirmed Off stops the client and clears the projected
transcript. The separate panel-transcript preference still controls whether a
live partial is shown while Voice input is On.

The two route launchers are attached later, from `ShellRuntimeApplication`,
because the Settings route launcher is built after the service applets. A
composition with no launcher reports both routes unavailable.

## Projection contract

`projectVoiceApplet()` is pure and is the single place that decides what the
user sees and what they may touch. An action it enables must be dispatchable;
one it disables must be inert. It never consults the clock, the filesystem or a
session bus, so the whole projection is testable from a snapshot literal.

`VoiceAppletController::invokeAction()` refuses an action id the projection did
not offer, even a real one, because QML is handed ids and must not be trusted
to have kept them current.

## Placement

The stock `qindaqt` profile places the chip in the top bar's end zone between
audio and smart lights. The manifest is `data/applets/voice.json`; its
entry point is `qindaqt.applets.voice`, registered in the first-party built-in
registry.

## Capabilities

`voice.read` and `voice.control`, both granted to the audited built-in `voice`
package and both denied to every third-party package. Read gates control: with
no projection to observe, there is nothing a control could act on.

## Verification

`qindaqt.voice-applet` covers the projection (states, capability gating, level
handling, accessibility text), request admission, and the controller's
dispatch, busy, authority-loss, uncertainty and route behaviour.

## See also

- [Voice input](../architecture/voice-input.md)
- [Voice console](../apps/voice.md)
