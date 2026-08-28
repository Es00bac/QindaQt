# Customization editor domain

The customization editor domain is the presentation-independent half of the
Customize editor. It turns pointer and keyboard gestures into commands for the
existing `shell_customization` transaction engine and persists applied layouts
as user profiles. It contains no QML, no window, and no shell-surface code;
the Settings window owns the repository, the process, and the presentation.

- Module: `src/shell_customization_editor` (public headers under
  `include/qindaqt/shell_customization_editor`)
- Focused tests: `tests/shell_customization_editor`
- Decision record: [ADR-0026](../adr/0026-isolate-the-customization-editor-domain.md)

## Boundary

| Allowed inward dependencies | Never |
| --- | --- |
| Public `shell_customization` commands and results, public `profiles` values and loader, Qt Core | Placement/manifest policy (the engine's `evaluate()` is the only acceptance authority), applet execution, shell surfaces, LayerShellQt, D-Bus, QML, Settings1 schema keys |

The module reuses the fourteen existing `EditingCommand` kinds; it adds no
command kind and no persisted field. Persisted user profiles are exact strict
schema-v1 documents (see [Profile schema v1](../reference/profile-schema-v1.md)).

## Components

- **Intent values** (`editor_intent.h`): drag payload (palette plugin or
  applet instance), resolved drop target identity `(panelId, zone, beforeAppletId)`,
  the six customization intents, and structural validation. Validation only
  checks shape (blank identities, zone vocabulary `start`/`center`/`end`,
  self-anchoring moves, configuration bounds); existence, manifest
  compatibility, and layout acceptance stay with the engine.
- **Intent translator** (`intent_translator.h`): a pure function from intent +
  target + context to ordered `EditingCommand` sequences. Identical inputs
  always produce identical sequences, which is the invariant behind
  pointer/keyboard parity. `expectedRevision` must be supplied by the caller;
  the translator never reads engine state.
- **Gesture state machine** (`gesture_state_machine.h`): the one
  `Idle → Arming → Dragging → Committing/Cancelling` machine that both input
  paths drive. It decides when the engine is called; the session decides what
  is sent. A drop is one preview bracket, so a zone-crossing move
  (`MoveApplet` + `UpdateAppletSettings`) commits as exactly one durable undo
  step.
- **Editing engine seam** (`editing_engine.h`,
  `coordinator_engine_adapter.h`): the session's typed view over the
  in-process `LayoutEditingRepository` lease. While another coordinator owns
  the session, every call fails with `RepositoryNotReady` and the editor
  presents read-only.
- **Editor session** (`editor_session.h`): binds machine, translator, engine
  seam, and persistence. Owns the dirty flag, the applied-profile identity,
  the per-target acceptance highlight, and the deterministic rollback paths.
- **User profile store** (`user_profile_store.h`): writes applied profiles
  atomically as `<user-directory>/<profile-id>.json` documents through
  `QSaveFile`; either the previous or the complete new bytes survive a crash.
  Layered catalog precedence (user wins on id collision) belongs to the
  profiles module's future catalog work and is deliberately not improvised
  here.
- **Keyboard navigation** (`keyboard_navigation.h`): pure slot/zone/panel/edge
  stepping over the outline so the keyboard path can produce the same targets
  as the pointer path.
- **Accessibility identity** (`accessibility_identity.h`): deterministic
  panel/applet/zone naming, position-in-set values, and the announcement
  wording ("Move clock to Top panel, end zone, position 3 of 4 — accepted /
  rejected: reason"). Acceptance is announced politely, rejection assertively;
  announcements coalesce to one latest value per politeness kind per event
  turn.

## Gesture rules

1. The visual drag appears only after `BeginPreview` succeeded.
2. `evaluate()` runs per hover-target change; commands execute only when the
   resolved target identity changes, and only if the evaluation accepts.
3. Inside a preview, each accepted target change converges the dragged
   instance toward the hovered target; the second move's source is where the
   instance now lives in the preview, not the gesture-start panel.
4. Acceptance highlights are discarded after any revision change.
5. Undo/redo are enabled only while the machine is idle.
6. An output-generation change closes any open gesture and marks the session
   stale until the host rebuilds it.

## Apply, revert, and persistence

**Apply** writes the edited snapshot through the user profile store and
clears the dirty flag; a failed write changes nothing and reports a typed
`ApplyFailed` reason. **Revert** only discards in-memory edits and reports to
the host, which rebuilds its repository from the last applied profile. The
editor never auto-saves, never writes `panels.configuration`, and does not
own profile selection; committing `panels.layoutProfile` through the public
Settings1 client stays with the Settings window. Applying a profile takes
effect at the next shell start until the live-binding slice lands.

## Testing

`tests/shell_customization_editor` registers five deterministic QtTest
suites: intent translation (`qindaqt.customize-editor-intent`), the gesture
machine invariants (`qindaqt.customize-editor-gesture-machine`), session
rollback and gating with a scripted engine (`qindaqt.customize-editor-session`),
atomic persistence round-tripped through `ProfileLoader`
(`qindaqt.customize-editor-persistence`), and keyboard stepping plus
accessibility identity (`qindaqt.customize-editor-accessibility`). All suites
use in-memory or temporary-directory fixtures: no GUI, compositor, session
bus, or user configuration is touched.

Presentation, canvas rendering, an offscreen UI matrix, and live session
behavior are not provided by this module and remain future slices of the
customization architecture.
