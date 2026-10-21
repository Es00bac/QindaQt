# ADR-0427: Share programmable input actions and controller layers

- Status: Accepted
- Date: 2026-10-20
- Owners: Input actions, controller platform and Settings Input
- Amends: [ADR-0347](0347-controller-desktop-integration.md) and [ADR-0426](0426-dualsense-desktop-layout-and-held-actions.md)

## Context

A fixed action menu cannot express Meta plus a click, text blocks, temporary
button modifiers, named LED pages or reusable input macros. Controller-only
navigation also needs touch contact independently of the touchpad switch.
Fullscreen browser video must retain desktop control. Keyboard and mouse macros
must reuse the same action execution and native shortcut authority.

## Decision

`src/input_actions` owns generic binding values and validation, bounded macro
steps, the atomic user macro library and a timer-driven sequencer over a borrowed
GUI-thread output. Controller policy aliases its public binding value. The
controller plugin remains the one compositor input-device integration; it also
composes the independent InputActions1 library endpoint and shared macro runner.
No shell execution, second virtual input device or new daemon is introduced.

Controllers1 snapshot/config version 2 stores 1–12 named pages with unique IDs,
RGB colors, bindings, a temporary-layer flag and one persisted active page.
Motion preferences remain at controller scope. Version-1 flat maps migrate into
one Desktop page without changing their values. Existing binding patches edit
the current page. Page replacement, references and global save budgets validate
atomically. Persistent page changes release owned input and require neutral
controls; held layers preserve their activating chord, release other old-map
holds and require those buttons to be released before changing their meaning.
Temporary layers are skipped by profile cycling. LED writes follow the effective
page only while desktop control owns the pad; failure is reported independently
of valid bindings. Missing physical hardware is not a configuration gate.

Touchpad contact is a separate `touchpad-touch` control. `hold-gyro` adds sensor
motion while held without changing the saved gyro preference. DualSense defaults
use contact for gyro and the physical switch for a left click. R2 holds Meta and
left click plus a temporary movement layer. Repeating directional pointer nudges
move the same drag through existing window/container pointer routing. Navigation
and editing pages provide arrows, Shift selection and Ctrl editing combinations.

InputActions1 stores up to 64 named macros in `qindaqt/input-macros.json`, with
owner-only atomic writes. Up to 64 typed steps can tap or hold input, release a
numbered hold, insert bounded Unicode text, wait, move or scroll. Holds must pair;
recursive macros, page mutations and executable commands are not steps. One
runner owns generated tokens and releases them on cancellation, focus loss,
lock, inactive session or teardown. Playback checks authority at least every
16 ms during waits; it never sleeps the compositor. Deliberate window-navigation
steps may adopt their resulting focus. Text delivery uses enabled Wayland
text-input, falling back to KWin's public keysym/keymap path in bounded batches;
it neither changes the clipboard nor replays text after cancellation.

The existing Shortcuts1 client gains a nonblocking transient lease. Register
preserves native assignments; explicit trigger edits use SetShortcuts, and
native readback supplies conflict and actual-key truth. Captured unique owners
and generations fence replies. Mouse triggers use the public KWin input filter
and ignore generated controller-device events. Monotonic playback intervals
reject native shortcut signals caused by playback, including delayed signals.
Physical Escape cancels macros. Steam and actual device consumers retain
controller priority; fullscreen status alone no longer removes desktop control.

Settings uses borrowed asynchronous JSON ports, authoritative readback and a
shared combination editor and macro step builder. It never opens controller or
macro files, SDL or compositor-private objects. Recording captures taps only in
an explicit editor pad, not global desktop input.

## Consequences

Older controller plugins cannot consume version-2 files; deploy the updated
plugin before saving them. Profile and macro documents have independent bounded
budgets and revisions. A missing macro reference refuses execution rather than
running a partial sequence. Native shortcut conflicts do not disable controller
or mouse macro triggers. The focused suite covers migration, holds, layers,
contact gyro, LED callbacks, Unicode/cancellation, macro validation/playback,
native leases and real QML editor journeys without requiring hardware or a model
review.

See [Controller input](../architecture/controller-input.md) and
[Input actions](../architecture/input-actions.md).
