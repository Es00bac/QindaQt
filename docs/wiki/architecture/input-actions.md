# Programmable input actions

**Settings → Input → Macros** creates reusable sequences for controller buttons,
keyboard shortcuts and mouse combinations. **Custom keys + mouse** in a controller
binding combines Ctrl, Alt, Shift, Meta, up to eight keys and five mouse buttons.
It also supports modifier-only holds. All held keys/buttons release with their
trigger; text and macros run once per press.

## Macro editor and execution

Named macros have a visual ordered step list with add, edit, duplicate, move and
delete controls. Steps tap an action/chord, begin/release one of eight numbered
holds, insert text, wait, move the pointer or scroll. The focused recording pad
adds key taps/clicks without recording other desktop applications. Copy/paste
and window-drag presets are editable starting points. Test starts after three
seconds to allow selecting the target app; physical Escape and Stop playback
cancel. Controller bindings can select or create a library macro.

A macro has 1–64 steps, paired holds, at most one minute of explicit delays,
waits at most 10 seconds and taps at most 2 seconds. Each text step allows 4,096
UTF-8 bytes. Movement/scroll components are finite and bounded to ±4,000.
Recursive macro calls and executable commands are rejected before saving or
running. Cancellation releases owned keys, clicks, nudges and queued text.
Unrelated window focus changes cancel playback; explicit window-navigation
actions can continue in their resulting focused window. A new run replaces the
previous owned run. Lock/inactive context is checked during waits, not just at
the next action. Text preserves Unicode, tabs and line breaks and never changes
the clipboard. Large text is emitted in bounded event-loop batches.

## Boundaries and persistence

`src/input_actions` owns generic values, codecs, `MacroStore` and `MacroRunner`.
The runner borrows a GUI-thread `MacroOutput` that outlives it. Controller policy
uses the public generic Binding; Settings uses public codecs and an injected
JSON port. The existing controller KWin plugin composes one shared input sink,
text emitter, native shortcut lease and public mouse filter. It is also usable
without a connected controller.

InputActions1 on `/org/qindaqt/InputActions1` exports the following same-user
session-bus interface:

| Member | Contract |
| --- | --- |
| `GetSnapshot() -> s` | Version-1 JSON, decimal revision, ready/error/running and macro rows with config and native shortcut status/readback |
| `Apply(s id, s config, t revision) -> s` | Atomic full named macro replacement; exact revision and ≤128 KiB request wrapping config (≤120 KiB) and explicit assignShortcut intent |
| `Reset(s id, t revision) -> s` | Delete the named macro; exact revision |
| `Run(s id, t revision) -> s` | Test a saved macro after three seconds; exact revision |
| `Cancel()` | Cancel the currently owned run and release its input |
| `Changed(t revision)` | Rediscover snapshot; never trust an old owner or mutation reply alone |

The library is `qindaqt/input-macros.json` under XDG config, schemaVersion 1,
with at most 64 macros and a 1 MiB document budget. Saves use owner-only QSaveFile
replacement. Invalid files remain untouched and disable library mutation.
Macros are referenced by stable ID; deleting one leaves its controller references
inactive until reassigned. Text contents are never diagnostics.

Keyboard triggers register through the existing native Shortcuts1 authority.
The asynchronous public `ShortcutLease` reads native saved assignments before
reclaiming them on startup and uses SetShortcuts only for an explicit trigger edit. Readback shows
actual assignments and conflict/unavailable status. The lease deactivates owned persistent actions on teardown, preserving native
assignments, and unregisters only deleted macros. Native v1 has no release-only
method, so this one release uses the existing same-owner `/kglobalaccel`
`setInactive` wire; registration, mutation, readback and dispatch use Shortcuts1.
All replies are fenced by captured unique owner and generation. Mouse triggers match
one button and optional modifiers and pause in Settings to keep its editor usable. Playback cannot recursively activate keyboard
or mouse macro triggers through its own generated events.

See [Input settings](../apps/input-settings.md), [Controller input](controller-input.md)
and [ADR-0427](../adr/0427-share-programmable-input-actions-and-controller-layers.md).
