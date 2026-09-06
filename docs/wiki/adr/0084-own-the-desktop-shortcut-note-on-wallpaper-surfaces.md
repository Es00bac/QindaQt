# ADR-0084: Own the desktop shortcut note on wallpaper surfaces

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Shell runtime
- **Supersedes:** None
- **Superseded by:** None

## Context

The desktop ships container-combination and docking gestures whose keys are
invisible to a first-time user, and the upcoming chrome toggle has no stable
binding yet. A pinned, dismissible cheat sheet is the smallest honest remedy,
but any new shell surface risks task-list contamination, focus stealing, and a
second preference authority. [ADR-0078](0078-own-wallpaper-surfaces-in-the-shell.md)
already owns one noninteractive desktop-scoped background surface per output
whose contract keeps windows out of the task list and visibility facts.

## Decision

The shortcut note is a card rendered inside the existing wallpaper background
surfaces — one card instance per output, shown only on the primary output,
positioned below and beside the profile's top- and right-edge panel bands.
No new layer-shell surface, window, or compositor role is created, and the
hosting surfaces stay keyboard-inactive; every interactive element in the card
uses `Qt::NoFocus`, so dismissal is clickable and can never capture keyboard
focus. The private `ShortcutNoteController` owns note state only: it never
grows the wallpaper controller beyond a lending attach hook.

Visibility persists through Settings1 as one schema-v2 boolean,
`shell.shortcutNoteDismissed` (default `false`, so the note is visible
initially). The controller consumes and writes it through a purpose-scoped
Settings1 client holding exactly that key, following the notification-quieting
pattern so an absent optional key can never poison another preference scope.
Unreachable or malformed settings fail visible: dismissal then lasts for the
session only. Reopening is the global shortcut registered through the shared
`GlobalShortcutRegistrar` seam with default `Meta+F1` and stable action id
`qindaqt_toggle_shortcut_note`; a reopen persists `false` so the next session
matches what is shown.

The card advertises only shipped, accurate mappings (note toggle,
Meta+Shift left-drag combine, Meta+Shift+D docking mode with arrow-key edge
choice, Escape cancel, Enter confirm) and labels the key text as defaults,
because the registrar seam cannot report the user's current mapped sequences.
The chrome-toggle binding is deliberately absent until its exact shortcut is
integrated and confirmed by root.

## Consequences

Desktop-surface ownership stays in ADR-0078's single exception; the note adds
no compositor or panel-planning responsibility. Settings schema v2 gains the
`shell` domain's first key, coordinated with the Settings owners. Tests cover
visibility/dismiss/reopen round trips over a real schema-backed service,
shortcut override tracking through the seam, and no-focus card interaction.
When the chrome toggle's binding lands, its exact shortcut may be added to the
card in the same default-labeled form.

## Revisit when

The registrar seam gains the ability to report currently mapped sequences
(replace the defaults labeling), the note needs per-output placement beyond
the primary screen, or Settings1 gains a shell-domain consumer that wants the
key composed with the shell preference scope instead of a purpose-scoped
client.
