# ADR-0183: a macro button is a list of console operations

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (builds on [ADR-0182](0182-a-preset-is-the-console-under-a-name.md))
- **Superseded by:** None

## Context

The reference console ships a macro-button companion: user-defined buttons
whose scripts drive the console — mute this, recall that, route the other. The
parity page listed macro buttons as a gap, and the Settings Audio route's
intent surface is closed: it has no text entry, by a gate that refuses any
text field, so a macro *editor* inside Settings was not on the table.

## Decision

A macro is a **name and an ordered list of console operations**, written by
the user in `$XDG_CONFIG_HOME/qindaqt/audio-macros.json` — the format is
documented in [Audio macro buttons](../reference/audio-macro-buttons.md). The
service reads the document fail-closed: an action that is not one of the
documented forms drops its whole macro, because a button that runs the first
half of what its author wrote is worse than a button that is not there. At
most 32 macros of at most 16 actions.

Only console operations can be named — strip mute, solo, mono, gain, send;
bus mute, mono, gain; preset load. Nothing a macro can say reaches the device
slice, a virtual device, or a stream; that is the same closed surface Settings
has, expressed in a file.

`RunMacro` (kind 24) runs the actions in order as ordinary console
operations, each admitted like one. The first refusal stops the macro and is
reported as the operation's reason; the actions before it stay applied, which
is what a stopped sequence means. The macro names are published in the
snapshot (`Console::macros`, schema 9), and the Settings console shows one
button per macro beside the presets.

## Consequences

- One-click console changes, scripted by the user in a file they own and can
  share.
- The document is re-read on every publication, so an edit takes effect
  without restarting anything.
- No macro can do what Settings cannot.

## Revisit when

- Global shortcuts for macros (the reference console binds them to keys); the
  shell's shortcut service would call `RunMacro`.
- A macro editor with a proper naming surface, outside the Audio route's
  closed gate.
