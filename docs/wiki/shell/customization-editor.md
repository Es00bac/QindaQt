# Customization editor domain

The customization editor domain is the presentation-independent half of the
Customize editor. It turns pointer and keyboard gestures into commands for the
existing `shell_customization` transaction engine and persists applied layouts
as user profiles. It contains no QML, no window, and no shell-surface code;
its hosts -- the Settings window and, since ADR-0213, the shell runtime --
own the repository, the process, and the presentation.

- Module: `src/shell_customization_editor` (public headers under
  `include/qindaqt/shell_customization_editor`)
- Focused tests: `tests/shell_customization_editor`
- Decision record: [ADR-0043](../adr/0043-isolate-the-customization-editor-domain.md)

## Boundary

| Allowed inward dependencies | Never |
| --- | --- |
| Public `shell_customization` commands and results, public `profiles` values/store, Qt Core | Placement/manifest policy (the engine's sequence evaluation is the only acceptance authority), applet execution, shell surfaces, LayerShellQt, D-Bus, QML, Settings1 schema keys |

The module reuses the fourteen existing `EditingCommand` kinds; it adds no
command kind and no persisted field. Persisted user profiles are exact strict
schema-v1 documents (see [Profile schema v1](../reference/profile-schema-v1.md)).

## Components

- **Intent values** (`editor_intent.h`): drag payload (palette plugin or
  applet instance), resolved drop target identity `(panelId, zone, beforeAppletId)`,
  the nine customization intents (applet insert/move/remove/duplicate,
  panel configure/move, applet settings, and the whole-panel `AddPanel` /
  `RemovePanel` pair the live menus need), and structural validation. Validation only
  checks shape (blank identities, zone vocabulary `start`/`center`/`end`,
  self-anchoring moves, configuration bounds); existence, manifest
  compatibility, and layout acceptance stay with the engine.
  `ConfigureAppletSettingsIntent` carries the applet's complete settings map
  (like `ConfigurePanelIntent`, `UpdateAppletSettingsCommand` replaces the
  whole map) with exactly the caller-validated field changed; typed
  schema-kind/bounds/enum validation is the caller's responsibility (see the
  [Settings Customize route](../apps/customize-settings.md)), not this
  module's.
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
  in-process `LayoutEditingRepository` lease. The adapter is owner-thread
  confined, retries a lost lease on the next action, exposes status, and
  evaluates an ordered command sequence on a disposable repository with the
  same outputs and manifest catalog. Evaluation never publishes live state.
  While another coordinator owns the session, mutation/evaluation fails with
  `RepositoryNotReady` and the editor presents read-only. Snapshot readability
  is never treated as write authority: the seam exposes unique-lease readiness
  separately from preview state and exposes the coordinator-retained committed
  profile only after that lease is held.
- **Editor session** (`editor_session.h`): binds machine, translator, engine
  seam, and persistence. Owns the canonical applied-profile baseline, its
  identity, the derived dirty flag, the per-target acceptance highlight, and
  the deterministic rollback paths.
- **User profile store** (`profiles/user_profile_store.h`, with a narrow editor
  adapter): the profiles module validates, strict-round-trips, and atomically
  writes `<user-directory>/<profile-id>.json` through `QSaveFile`; either the
  previous or the complete new bytes survive a crash.
  Layered catalog precedence (user wins on id collision) belongs to the
  profiles module's future catalog work and is deliberately not improvised
  here.
- **Keyboard navigation** (`keyboard_navigation.h`): pure slot/zone/panel/edge
  stepping over the outline so the keyboard path can produce the same targets
  as the pointer path.
- **Live host** (`live_editor_host.h`): the shell runtime's owner of one
  repository, engine adapter and session, composed exactly as the Settings
  route's host composes them; see [Live host in the shell](#live-host-in-the-shell).
- **Accessibility identity** (`accessibility_identity.h`): deterministic
  panel/applet/zone naming, position-in-set values, and the announcement
  wording ("Move clock to Top panel, end zone, position 3 of 4 — accepted /
  rejected: reason"). Acceptance is announced politely, rejection assertively;
  announcements coalesce to exactly one latest tuple per event turn.

## Gesture rules

1. The visual drag appears only after `BeginPreview` succeeded.
2. Sequence evaluation and execution run only when the resolved target identity
   changes. Each command is retagged from the preceding simulated/executed
   result, so a cross-panel zone move is accepted and executed atomically
   without weakening optimistic fencing.
3. Inside a preview, each accepted target change converges the dragged
   instance toward the hovered target; the second move's source is where the
   instance now lives in the preview, not the gesture-start panel.
4. Acceptance highlights are discarded after any revision change.
5. Undo/redo are enabled only while the machine is idle.
6. An output-generation change closes any open gesture and marks the session
   stale until the host rebuilds it.
7. Release over an off-target or rejected target cancels the whole preview;
   it never commits the last accepted provisional target.

## Apply, revert, and persistence

**Apply** is accepted only for an idle, non-preview, non-stale session that
holds the unique coordinator lease. A losing editor returns typed
`EngineUnavailable` and writes no profile even though it can still read the
repository's published snapshot. Apply writes the committed edited snapshot
through the profiles-owned store and replaces the applied baseline only after
success; a failed write changes nothing and reports `ApplyFailed`. The session
initializes that baseline from the coordinator-retained committed profile, not
the possibly provisional published snapshot. If construction loses the lease
during a foreign preview, the session fails dirty/read-only and adopts that
committed baseline only after normal lease retry, before its first successful
mutation or Apply. After every successful point or drag commit, Undo, and Redo,
dirty truth is derived by comparing the full canonical schema-v1 profile with
the baseline; revisions and history position are never treated as proxies for
unsaved content. **Revert** cannot replace the host-owned repository, so it
preserves dirty truth, rejects further edit/apply work, and returns the typed
`RebuildRequired` outcome. The host completes
Revert by constructing a fresh repository from the last applied profile. The
editor never auto-saves, never writes `panels.configuration`, and does not own
profile selection; committing `panels.layoutProfile` through the public
Settings1 client stays with the Settings window. Applying a profile takes
effect at the next shell start until the live-binding slice lands.

## Live host in the shell

`LiveEditorHost` (ADR-0213) composes `LayoutEditingRepository(profile,
outputs, manifests)` → `CoordinatorEditingEngine(repository, manifests)` →
`EditorSession(engine, UserProfileStore(directory))` with the same arguments,
in the same order, as the Settings route's `RepositoryCustomizeEditorHost`.
That composition is the parity contract: the intent sequence a menu entry or
an edit-mode drop sends produces the same command sequence, the same undo
step, and the same persisted bytes as the route would for the same intent.
The host adds `rebuild(profile, outputs)` for the shell's adoption and
output-hotplug paths (undo history does not survive a rebuild by design) and
otherwise exposes the session's gesture, point-operation, undo/redo and Apply
calls unchanged. The shell's `LiveCustomizationController` (see
[Production panel surfaces](panel-surfaces.md#in-place-customization-meta-right-click))
maps each menu entry to one `applyGesture` and one `applyToUserProfile`, and
the shell adopts the written profile through its existing store path.

## Testing

`tests/shell_customization_editor` registers eight deterministic QtTest
suites: intent translation (`qindaqt.customize-editor-intent`), the gesture
machine invariants (`qindaqt.customize-editor-gesture-machine`), session
rollback and gating with a scripted engine (`qindaqt.customize-editor-session`),
canonical applied-baseline and dirty history through the production composition
(`qindaqt.customize-editor-dirty-state`),
atomic persistence round-tripped through `ProfileLoader`
(`qindaqt.customize-editor-persistence`), and keyboard stepping plus
accessibility identity (`qindaqt.customize-editor-accessibility`), the live
host over the real engine including the panel intents and a byte comparison
against an independently composed trio (`qindaqt.customize-editor-live-host`),
and the same comparison against the Settings route's own
`RepositoryCustomizeEditorHost` for a full menu script
(`qindaqt.customize-editor-live-host-parity`, full tree only). All suites
use in-memory or temporary-directory fixtures: no GUI, compositor, session
bus, or user configuration is touched.

The session suite also composes the production repository adapter and proves
exact cancellation, rejected release behavior, return from an invalid hover,
failed-Apply and Revert truth, and coordinator-lease retry. The dirty-history
suite composes the real repository, adapter, and profile store to prove four
production lifecycles: cross-panel edit followed by Undo returns exactly to
the constructor baseline without a false close prompt; Apply followed by
Undo/Redo reports dirty/clean against the newly persisted baseline while
retaining one durable undo boundary; Apply under a foreign lease returns
`EngineUnavailable` without creating a file; and construction during a
foreign preview followed by cancel/release, edit, and Undo adopts the retained
committed baseline and returns exact/clean.

The installed [Settings Customize route](../apps/customize-settings.md) now
composes this public boundary into a direct canvas, audited palette, property
panes, keyboard outline, Settings1 profile draft/apply flow, and offscreen plus
package proof. It preserves this module's gesture, rollback, lease, persistence,
and dirty-state authority rather than duplicating engine policy in QML. Live
shell preview/application, reveal affordances for always-hidden panels,
installed-session behavior, and the nested rendered matrix remain later slices
of the customization architecture.
