# ADR-0182: a preset is the console under a name

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (builds on [ADR-0176](0176-the-console-remembers-itself.md))
- **Superseded by:** None

## Context

The reference console saves and recalls whole configurations. QindaQt's
console remembered itself ([ADR-0176](0176-the-console-remembers-itself.md))
but had exactly one memory; a streamer who mixes differently for a podcast
and a game had to rebuild the console by hand each time.

## Decision

A preset is the console document — the same `ConsoleModel::toJson`, nothing
live — written under a name into `$XDG_CONFIG_HOME/qindaqt/audio-presets/`,
one file per preset, at most 64. The file name is a slug of the display name
(lower-case, `[a-z0-9-]`), and the display name travels inside the document,
so the slug never has to be reversible and nothing a name contains can leave
the directory.

Three operations, kinds 21–23, carry the name: `SavePreset`, `LoadPreset`,
`DeletePreset`. They are applied by the coordinator, not the console model —
they are whole documents through the preset store — and complete
synchronously like every console operation. A load goes into a scratch model
first and is refused whole if the result fails the console gate, so a
malformed preset can never half-apply onto the live console; a successful
load rebinds against the retained graph at once, since bindings are live state
and not in the document.

The names are published in the snapshot (`Console::presets`, schema 8), so
every surface offers the same list without a second round trip.

The Settings bar offers load, delete, and *save as new*, which takes the
first free "Preset N" name. It offers no text entry: the Audio route's intent
surface is closed and its boundary gate refuses any text field. Renaming a
preset is a later concern.

## Consequences

- Whole console configurations can be saved and recalled.
- A preset directory is a plain set of JSON files a user can copy or share.
- Test coordinators take the preset directory as a constructor argument, so a
  test never touches the user's presets.

## Revisit when

- Renaming and a proper naming surface are wanted; the store already keeps
  the display name apart from the file name for that.
- Macro buttons land; a macro that recalls a preset is the obvious first one.
