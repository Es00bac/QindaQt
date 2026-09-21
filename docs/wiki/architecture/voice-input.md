# Voice input

QindaQt's voice input is a contract the desktop owns and does not implement.
The interface is `org.qindaqt.Voice1`; a separate **provider** process
implements it and does the actual listening, transcribing and inserting.
[Gabbee](https://github.com/cabewse/gabbee) is the provider packaged with the
desktop today, and nothing in QindaQt names it outside packaging.

[ADR-0233](../adr/0233-own-voice-input-as-a-contract-not-an-implementation.md)
records why the split exists and what was rejected.

## What the user gets

Hold the dictation shortcut in any window — a terminal, a browser, a chat box,
an editor — and what you say is inserted there. The provider owns that delivery
and picks its route; the desktop reports which route was used and never selects
one.

Three surfaces consume the contract:

| Surface | Purpose |
| --- | --- |
| [Voice applet](../shell/voice-applet.md) | Panel chip: capture state, level, the live partial, and the last-dictation actions. |
| Voice Settings route | The desktop's two preferences, the provider's live state and capabilities, provider choice, and a "try it" control. |
| [Voice console](../apps/voice.md) | A QindaTK window: the session's dictation history, provider detail, and every intent in one place. |

## Why it reaches every window

The provider owns delivery, but the desktop owns the environment delivery
happens in. A Qt or GTK application only has an input-method context when
`QT_IM_MODULE`, `GTK_IM_MODULE` and `XMODIFIERS` are set before it starts, and
that is a session decision, not something a provider can fix later.

`SessionEnvironment` therefore points all three at IBus when an
`ibus-daemon` is installed and the user has not chosen an input method of
their own. One user-set variable speaks for the whole decision: a session that
half-answers with somebody else's input method would disagree with itself.

With the context present, a provider can commit text into the focused field
the way a keyboard does, which is what makes dictation work in a terminal, a
browser and a chat box alike rather than only where an accessibility node
happens to exist. Without it, a provider falls back to its own recovery
routes; nothing breaks, but the most reliable path is gone.

## Module boundaries

| Module | Owns | Must never |
| --- | --- | --- |
| `QindaQt::VoiceProtocol` | The wire contract: enums, bounds, the `a{sv}` codec, and validation. | Link a transport, a client, a provider, QML, audio, or an input method. |
| `QindaQt::VoiceClient` | Owner tracking, snapshot ordering, one in-flight intent, timeouts. `QtVoiceTransport` is the only translation unit with a bus. | Link QML, the shell, or a provider. |
| `QindaQt::ShellVoiceApplet` | The pure projection: what is shown and what may be touched. | Link a transport, a client, or QML. |
| `QindaQt::ShellVoiceAppletRuntime` | The controller over the injected seam, and the compiled QML. | Reach a provider, a microphone, or the filesystem. |
| `qindaqt_settings_voice` | The Settings route's model over one Settings1 client and one Voice1 client. | Own a bus; the route composition does. |
| `qindaqt_voice_console_support` | The console's model and its session history. | Link the shell or an applet. |

## The contract

Service `org.qindaqt.Voice1`, object `/org/qindaqt/Voice1`, interface
`org.qindaqt.Voice1`. The provider is D-Bus activatable.

### Payload shape

Every payload is `a{sv}`, never a D-Bus structure, so a provider can be written
in any language. Every key is mandatory; an unexpected key, a missing key or a
key of the wrong type makes the payload invalid and it is rejected with a named
reason rather than partially adopted.

### Methods

| Member | In | Out |
| --- | --- | --- |
| `GetSnapshot` | — | `a{sv}` snapshot |
| `StartDictation` | `t requestId, t expectedRevision` | `a{sv}` result |
| `StartCommand` | `t, t` | `a{sv}` result |
| `Finish` | `t, t` | `a{sv}` result |
| `Cancel` | `t, t` | `a{sv}` result |
| `Retry` | `t, t` | `a{sv}` result |
| `Undo` | `t, t` | `a{sv}` result |
| `CopyLast` | `t, t` | `a{sv}` result |
| `SetProvider` | `t, t, s providerId` | `a{sv}` result |
| `SetEnabled` | `t, t, b enabled` | `a{sv}` result |

### Signals

| Signal | Args | Meaning |
| --- | --- | --- |
| `Changed` | `t revision` | A caller-visible field changed. The revision is carried so a consumer already holding it can skip the round trip. |
| `Level` | `u percent` | Unrevisioned, lossy capture telemetry for the meter. Never gates anything, and is honoured only while the snapshot says capture is live. |

### Snapshot keys

| Key | Type | Bound |
| --- | --- | --- |
| `schemaVersion` | `u` | Exactly 1. |
| `revision` | `t` | Non-zero, monotonic within one owner. |
| `state` | `u` | `Idle`(1) `Arming`(2) `Listening`(3) `Transcribing`(4) `Delivering`(5) `Error`(6). |
| `mode` | `u` | `Dictation`(0) `Command`(1). |
| `enabled` | `b` | Whether the provider's shortcuts are armed. |
| `capabilities` | `u` | Bitmask; unknown bits are rejected. |
| `lastRoute` | `u` | `None`(0) `InputMethod`(1) `Accessibility`(2) `Clipboard`(3) `KeySynthesis`(4). |
| `providerId`, `languageCode` | `s` | `[a-z0-9._-]`, 1–64 UTF-8 bytes. |
| `providerLabel`, `microphoneLabel` | `s` | Display text, ≤128 UTF-8 bytes, no control characters. |
| `dictationShortcut`, `commandShortcut` | `s` | Display text, ≤64 UTF-8 bytes. |
| `partialText`, `lastText` | `s` | ≤512 UTF-8 bytes; `\n` and `\t` admitted, every other control character refused. |
| `reasonCode` | `s` | `[a-z0-9-]`, 1–64 bytes. `"ok"` when nothing is wrong. |
| `providerIds`, `providerLabels` | `as` | Equal length, ≤12 entries. |
| `providerAvailableMask` | `u` | Bit *i* is `providerIds[i]`'s availability. A bit past the list is rejected. |

### Result keys

`kind` `u`, `status` `u` (`Succeeded`(0) `Rejected`(1) `Failed`(2)
`Uncertain`(3) `Busy`(4)), `requestId` `t`, `initiatingRevision` `t`,
`observedRevision` `t`, `reasonCode` `s`.

## Invariants

- **Revision is the only ordering authority.** It advances when, and only when,
  a caller-visible field changed. A regression, or a repeat that carries
  different values, revokes the client's projection.
- **One intent at a time.** A second intent is answered `Busy` locally and
  never reaches the provider.
- **Nothing is replayed.** `StartDictation` is not idempotent: a silent retry
  opens a second microphone session and can deliver the same utterance twice.
  An unanswered intent is reported `Uncertain` and surfaced, everywhere, in
  those words.
- **A stale partial is a defect.** A snapshot reporting `partialText` outside
  `Arming`/`Listening`/`Transcribing` is rejected: it would show the user text
  they have already replaced or discarded.
- **Capability before offer.** A control whose capability bit is clear is not
  offered. A control that is offered and enabled is dispatchable.
- **Read gates control.** Without `voice.read` the applet has no projection, so
  `voice.control` grants nothing.

## Capability policy

`data/applet-policy/default.json` grants `voice.read` and `voice.control` to
the audited built-in `voice` package and denies both to every third-party
package. Dictation text is the most privileged thing on the desktop at the
moment it is spoken; observing it stays with the audited applet, and opening
the microphone is never delegated.

## Settings

Two keys, both in the `services` domain of `data/settings/schema-v2.json`:

| Key | Default | Meaning |
| --- | --- | --- |
| `services.voiceInput` | `false` | Whether QindaQt uses a speech provider at all. |
| `services.voicePanelTranscript` | `true` | Whether the panel chip shows the live partial while dictating. |

Everything else — vocabulary, corrections, command patterns, app profiles —
belongs to the provider and is reached through its own interface. Mirroring it
into Settings1 would create a second authority for values the provider owns.

## Writing a provider

A provider owns the name `org.qindaqt.Voice1`, answers `GetSnapshot` with a
payload that satisfies every bound above, emits `Changed` when a caller-visible
field moves, and answers every method with a result for that exact
`requestId`. It must:

- include its own `providerId` in `providerIds` whenever it advertises a list;
- clear `partialText` the moment capture ends;
- report a structured `reasonCode`, never an English sentence;
- refuse an intent whose capability bit it did not set;
- never let `revision` go backwards while it owns the name.

Gabbee's implementation is `src/gabbee/qindaqt_voice.py` plus
`src/gabbee/qindaqt_voice_wire.py` in its own repository, and its D-Bus
activation file is `share/dbus-1/services/org.qindaqt.Voice1.service`.

## Verification

| Row | Covers |
| --- | --- |
| `qindaqt.voice-protocol` | Bounds, the codec, validation, and payloads captured verbatim from a live provider. |
| `qindaqt.voice-client` | Owner attribution, revision ordering, single-intent accounting, uncertainty. |
| `qindaqt.voice-applet` | The projection, request admission, and the controller. |
| `session.voice-interop` | The production client against a real provider on a private bus. Skips (77) with no provider checkout present. |

The interop row is the cross-implementation gate: neither half is stubbed, so a
key rename or a broken admission rule fails there rather than in a live
session.

## See also

- [Voice applet](../shell/voice-applet.md)
- [Voice console](../apps/voice.md)
- [Gabbee interop evidence](../development/gabbee-interop-evidence.md)
- [Module boundaries](module-boundaries.md)
