# ADR-0233: Own voice input as a contract, not an implementation

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** Voice platform contract, the Voice applet, the Voice route, and
  the Voice console
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt had no voice input of its own. What it had was evidence that somebody
else's worked on it: the Gabbee interop rows under `tests/session/gabbee/`
qualify Gabbee's global shortcuts through the real portal frontend, and its
text delivery into standalone and grouped terminals through a real PTY
([Gabbee interop evidence](../development/gabbee-interop-evidence.md)). That
work proved the hard part — dictated text reaches a terminal, a browser and an
ordinary editor — and stopped exactly there. Voice was a program a user could
run next to the desktop, not a thing the desktop had.

Making it first-class meant deciding what QindaQt was actually adopting.

Adopting the program would have meant the shell knowing about Gabbee: its
`io.gabbee.Controller` name, its Python state machine, its config file. That
buys a fast integration and a permanent dependency on one project's internals,
in a desktop whose every other hardware-facing feature sits behind a contract
it owns (Audio1, Clipboard1, Power1, Display1, Network1).

Rewriting the engine in C++ was the other extreme. Gabbee is ~190 passing
tests of semantic number formatting, spoken punctuation, realtime streaming
with batch recovery, exactly-once delivery across four routes, and a typed
desktop-command grammar. None of that is desktop-shell work, and none of it
would be better for being ours.

There is also a third party to consider that does not exist yet: the next
provider. Whisper running locally, a different cloud vendor, an accessibility
tool with its own opinions. A desktop that hard-codes one of them has to be
re-integrated for each.

## Decision

QindaQt owns **`org.qindaqt.Voice1`** and implements none of it.

The desktop ships the consumer half: `QindaQt::VoiceProtocol` (the wire
contract and its bounds), `QindaQt::VoiceClient` (owner tracking, snapshot
ordering, single-intent accounting), the Voice panel applet, the Voice
Settings route, and the `qindaqt-voice` console. Gabbee ships the provider half
in its own repository, in `src/gabbee/qindaqt_voice.py`, over its existing
controller.

Three consequential shapes follow from that split.

**The payload is `a{sv}`, not a D-Bus structure.** A positional structure
requires every provider to register a C++-shaped custom type, which PyQt and
most other bindings cannot reach. A provider contract that only C++ can
implement is not a provider contract. The strictness a structure would have
given is recovered on decode: an unexpected key, a missing key or a key of the
wrong type marks the payload invalid, and validation then rejects it with a
named reason.

**The provider is the authority on its own capabilities.** The snapshot carries
a capability bitmask and an inventory of providers the user could switch to.
The applet disables a control whose bit is clear rather than offering an intent
that can only fail. Starting, finishing, cancelling and arming are the contract
floor; everything else is optional and advertised.

**No intent is ever replayed.** Dictation is not idempotent: a silently retried
`StartDictation` opens a second microphone session and can deliver the same
utterance twice into the user's document. An unanswered request is reported
`Uncertain` and surfaced, and every layer — client, applet, Settings model,
console — says so in those words rather than guessing.

## Consequences

The desktop degrades honestly with no provider installed. The applet says "No
voice provider is running", the Settings page says the same with a retry, and
the console says it with an install hint. `QtVoiceTransport` reports an empty
owner when D-Bus activation is refused, so "not installed" resolves rather than
hanging in "Starting" forever.

Transcript text crosses the boundary, bounded at 512 UTF-8 bytes, because the
panel chip shows what the user is dictating exactly as Gabbee's own bar does.
That bound is a decision, not an accident: raising it turns a panel applet into
a transcript store. Spoken layout (`\n`, `\t`) is admitted; every other control
character is refused, so a transcript can never carry terminal escapes. A
snapshot that reports a partial outside capture is rejected outright as a
provider defect, because a stale partial shows text the user already replaced.

The desktop's own preferences stay small — `services.voiceInput` and
`services.voicePanelTranscript`. Everything else (vocabulary, corrections,
command patterns, app profiles) belongs to the provider and is reached through
its own interface, not mirrored into Settings1.

Gabbee gains three in-place hooks: a peak-level observer on its recorder, an
`input_enabled` flag its `start()` honours so the desktop's "shortcuts armed"
switch tells the truth no matter which shortcut backend fires, and the provider
module itself. It stays fully usable on desktops that never consume Voice1.

The session gains one responsibility it did not have: `SessionEnvironment`
points `QT_IM_MODULE`, `GTK_IM_MODULE` and `XMODIFIERS` at IBus when an
`ibus-daemon` is installed and the user has chosen no input method of their
own. That is a session-wide change, not a voice-only one, and it is here
because a Qt or GTK application only has an input-method context if those
variables were set before it started — no provider can fix that afterwards. It
is the difference between dictation committing into the focused field the way a
keyboard does and falling back to a recovery route. A single user-set variable
suppresses all three, because a session that half-answers with somebody else's
input method would disagree with itself.

The console is the second consumer, which is what justifies the
protocol/client/presentation split rather than composing everything in the
shell process as [ADR-0186](0186-write-internal-brightness-through-logind.md)'s
sibling Smart Lights work deliberately did.

## Alternatives rejected

- **Consume `io.gabbee.Controller` directly.** Fastest, and it makes the shell
  depend on one project's internal names and state machine.
- **Port the engine to C++.** Discards ~190 tests of semantic work that is not
  desktop-shell work, for no user-visible gain.
- **Model the contract as D-Bus properties.** `org.freedesktop.DBus.Properties`
  would give per-field change notification, but the revision rule needs the
  whole snapshot to move at once; per-property signals would let the panel
  render a mixture of two revisions.
- **A resident QindaQt service between the shell and the provider.** A second
  hop with nothing to decide. The client already fences owner, revision and
  request identity; a relay would only add a place for them to disagree.
