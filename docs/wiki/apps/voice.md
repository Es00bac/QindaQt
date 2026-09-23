# Voice

**Voice** (`qindaqt-voice`) is the dictation console: a QindaTK window that
shows what you have dictated in this session, what the speech provider is
doing, and every voice action in one place.

It consumes [org.qindaqt.Voice1](../architecture/voice-input.md), as do the
[panel applet](../shell/voice-applet.md) and Voice Settings route. Its only
provider connection is
Voice1; it reads services.voiceInput through the shared desktop preference
gate before starting that connection. Opening the console while desktop
Voice is Off never activates the provider, and Try again cannot bypass Off.
A confirmed On connects live without reopening.

## The window

**Tool bar** — Dictate (which becomes Finish while recording), Command, Cancel,
a state badge, a capture-level bar while recording, and the armed switch.

**Live band** — while dictating, the partial text in the accent colour across
the top, so the words are readable from a distance.

**Session list** — every dictation delivered since the window opened, newest
first, each with its time, the route that placed it, and whether it was a
spoken command. Clicking selects; double-clicking copies. The find bar filters
by text.

**Details panel** — the selected dictation in full with a Copy button, the
provider row (a combo when the provider supports switching), the shortcuts and
microphone, and what this provider can do.

**Status bar** — provider, language, microphone, the last delivery route, and
how many dictations this session holds.

## The history is session-only

The list lives in memory, is capped at 200 entries, and is erased when the
window closes. Nothing is written to disk.

That is a decision, not an omission. A persistent list of everything a person
has ever dictated is a different product with different consent: it would need
a retention policy, an export path, an encryption story and a way to forget a
single entry. The console answers "what did I just say", which needs none of
those.

Copying an entry uses the console's own clipboard rather than the provider's
`CopyLast`, so it works on text the user is already looking at even after the
provider has gone away.

## What it is not

The console does not edit vocabulary, corrections, command patterns or app
profiles. Those belong to the speech provider and are reached through the
provider's own interface — for Gabbee, its Command Studio. Mirroring them here
would create a second authority for values QindaQt does not own.

## Theming

`QindaQtTheme` feeds the desktop's QST-1 semantic tokens into QindaTK's
`Theme`, so the console follows the session theme and its accessibility
settings the same way the [Viewer](viewer.md)
does. See [ADR-0218](../adr/0218-use-qindatk-and-poppler-for-the-viewer.md) for
the QindaTK precedent.

## Without a provider

With desktop Voice Off, the window reports that preference and leaves
provider actions dimmed. With Voice On but no provider installed,
the window opens and says so: a warning notice with a Try again control and an
install hint, an empty state in the list, and every action dimmed. Nothing
throws and nothing hangs — `QtVoiceTransport` reports an empty owner when D-Bus
activation is refused, so "not installed" resolves rather than waiting forever.

## Installation

Installed by the `VoiceConsole` component as `qindaqt-voice`, with
`org.qindaqt.Voice.desktop` and a scalable icon. It is built under the same
`QINDAQT_BUILD_VIEWER` switch as the Viewer, because both are first-party
QindaTK applications and share that dependency.

## See also

- [Voice input](../architecture/voice-input.md)
- [Voice applet](../shell/voice-applet.md)
