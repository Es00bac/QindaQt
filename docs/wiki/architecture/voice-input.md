# Voice input

QindaQt's voice input is a contract the desktop owns and does not implement.
The interface is `org.qindaqt.Voice1`; a separate **provider** process
implements it and does the actual listening, transcribing and inserting.
[Gabbee](https://github.com/cabewse/gabbee) is the provider packaged with the
desktop today, and nothing in QindaQt names it outside packaging.

[ADR-0233](../adr/0233-own-voice-input-as-a-contract-not-an-implementation.md)
records why the split exists and what was rejected.
[ADR-0244](../adr/0244-gate-desktop-voice-use-on-confirmed-settings.md)
records the desktop's default-off activation policy.

## What the user gets

After switching desktop Voice input On and configuring a provider, hold its
dictation shortcut in any window — a terminal, a browser, a chat box, an
editor — and what you say is inserted there. The provider owns that delivery
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

The session supervisor runs IBus for the lifetime of the login. On Gentoo it
names `/usr/libexec/ibus-dconf` explicitly: IBus 1.5.33 on qinda exits 255
with `Can not execute default config program` when left to find that helper
implicitly, while the same executable stays running when given its installed
path. The supervisor's child-lifetime test pins the launch arguments and
restart behavior.

Gabbee tries `dotool` first to type into the currently focused window. The
Gentoo desktop profile installs it with Gabbee. If that route cannot deliver,
Gabbee tries its IBus bridge, the focused accessibility node, and then its
clipboard-paste recovery. IBus can commit into a focused input context only
when the application started with the input-method variables and its Gabbee
engine is active. The two routes cover different failure modes: `dotool`
reaches windows already open before an IBus daemon was repaired, while IBus
provides a direct text route for applications with an input context.

## Module boundaries

| Module | Owns | Must never |
| --- | --- | --- |
| `QindaQt::VoiceProtocol` | The wire contract: enums, bounds, the `a{sv}` codec, and validation. | Link a transport, a client, a provider, QML, audio, or an input method. |
| `QindaQt::VoiceClient` | Owner tracking, snapshot ordering, one in-flight intent, timeouts. `QtVoiceTransport` is the only translation unit with a bus. | Link QML, the shell, or a provider. |
| `QindaQt::VoicePreferences` | Confirmed current-owner desktop opt-in over a borrowed Settings1 client; no provider connection. | Activate Voice1, own a bus, or infer consent from a default. |
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
| `providerIds`, `providerLabels` | `as` or `av` of `s` | Equal length, ≤12 entries. |
| `providerAvailableMask` | `u` | Bit *i* is `providerIds[i]`'s availability. A bit past the list is rejected. |

#### Why the provider arrays accept two signatures

A list of strings nested inside `a{sv}` does not have one spelling across
bindings. Qt sends `as`. PyQt6 turns a Python list into a `QVariantList` and
sends `av` whose every element is a variant holding `s` — which is what
Gabbee actually puts on the bus. Both are honest encodings of the same value,
so the decoder reads both and recovers strictness per element: a variant that
does not hold a string is refused, not coerced. Any other element signature is
refused outright.

The decoder never hand-walks these arrays. `QDBusArgument` does not advance
when an element is not the type being extracted, so the obvious
`while (!argument.atEnd())` loop never terminates and appends until the
process is killed — a replaceable provider could otherwise take the shell
down with it. The array bound is enforced in the decoder, before anything is
built from the payload, rather than by the caller afterwards.

This is the defect `session.voice-interop` was written to catch, and the one
it did catch: against the real provider the client allocated 26GB and was
killed by the kernel before it printed a single check.

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

The shell applet, Voice Settings route and Voice console start Voice1 only
after Settings1 confirms services.voiceInput On for its current owner.
Default Off, an unavailable baseline, owner loss, or degraded Settings1
authority withdraws desktop actions and provider projections. A
same-owner refresh preserves confirmed On while the separate panel
transcript preference is saved. Applying Off in Settings withdraws that
route's actions before its asynchronous commit completes and keeps them
withdrawn through conflict or uncertain readback until a fresh snapshot
confirms On. Desktop Off
does not terminate an independently running provider or change its own
shortcut-armed state; that is the separate Voice1 enabled field.

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
| `qindaqt.settings-voice` | Default-off baseline, live opt-in, owner replacement, clean external drafts, sequential two-key apply, and conflict/uncertain Off readback. |
| `qindaqt.voice-applet` | The projection, request admission, and the controller. |
| `qindaqt.voice-applet-composition` | Real shell composition stays inactive through missing or Off Settings1 baselines and owner changes; confirmed On alone starts the Voice client. |
| `session.voice-interop` | The production client against a real provider on a private bus. Skips (77) with no provider checkout present. |

The interop row is the cross-implementation gate: neither half is stubbed, so a
key rename or a broken admission rule fails there rather than in a live
session.

## See also

- [Voice applet](../shell/voice-applet.md)
- [Voice console](../apps/voice.md)
- [Gabbee interop evidence](../development/gabbee-interop-evidence.md)
- [Module boundaries](module-boundaries.md)
