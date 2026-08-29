# Handoff — C0 customization editor domain candidate (Kaito Reed)

- Posted: 2026-08-28T14:10:45Z (unix 1787926245)
- Worker: Kaito Reed — GLM `zai-coding-plan/glm-5.3-flash`, reasoning high
- Status: **handoff — review requested; this seat is not live**
- Exact candidate commit: **`42200c8`** (`Add the customization editor domain
  module`), single commit on branch `worker/wysiwyg-customization-c0` in
  `/home/cabewse/work_SPaC3/container-wm-workers/wysiwyg-customization-c0`,
  parent exactly `9db68c4` (public `main`), working tree clean after commit.

## What landed

29 files, +3933 lines, all inside my claimed ownership:

- `src/shell_customization_editor/**` — new static library
  `qindaqt_shell_customization_editor` / alias `QindaQt::ShellCustomizationEditor`
  (own CMakeLists; deps: public `QindaQt::ShellCustomization` + Qt6::Core only):
  - `editor_intent.h/.cpp`: drag payload, resolved drop-target identity,
    six intents, zone vocabulary `start/center/end`, structural validation
    (no placement policy — `evaluate()` stays the only acceptance authority).
  - `intent_translator.h/.cpp`: pure intent→command translation; zone-crossing
    moves emit `MoveApplet` + `UpdateAppletSettings`; `ConfigurePanel` always
    sends the complete five-field tuple; caller-supplied `expectedRevision`
    only (prohibited-shortcut 13 respected); `gestureSequence()` canonical
    bracket form.
  - `gesture_state_machine.h/.cpp`: Idle/Arming/Dragging/Committing/Cancelling
    with all six architecture invariants encoded; executes per resolved-target
    identity change, evaluates per hover change (D3).
  - `editing_engine.h` + `coordinator_engine_adapter.h/.cpp`: typed seam over
    the move-only coordinator lease; `RepositoryNotReady` degradation to
    read-only when the lease is lost.
  - `editor_session.h` + `editor_session.cpp` + `editor_session_gestures.cpp`:
    deterministic rollback paths (failed in-drag command, failed commit, failed
    apply), converging in-drag execution for palette inserts and instance
    drags, idle-only undo/redo gating, apply/revert (D12), output-generation
    staling (D16).
  - `user_profile_store.h/.cpp`: atomic `QSaveFile` user profiles
    (`<dir>/<id>.json`), typed failure codes, file-name-safe id validation;
    layered catalog precedence deliberately NOT improvised here (profiles-module work, D4).
  - `keyboard_navigation.h/.cpp`: pure slot/zone/panel/edge stepping for the
    keyboard path; `accessibility_identity.h/.cpp`: deterministic naming,
    position-in-set, polite/assertive announcements with per-kind coalescing.
  - No new `EditingCommand` kind, no schema field, no QML, no LayerShellQt,
    no D-Bus, no `panels.configuration` writes.
- `tests/shell_customization_editor/**` — five focused QtTest suites
  (`qindaqt.customize-editor-intent`, `-gesture-machine`, `-session`,
  `-persistence`, `-accessibility`) plus fixtures and a standalone-capable
  CMakeLists mirroring `tests/shell_customization` conventions.
- `docs/wiki/shell/customization-editor.md` — module's primary wiki page.
- `docs/wiki/adr/0026-isolate-the-customization-editor-domain.md` +
  one index row in `docs/wiki/adr/index.md` (next free number was 0026; the
  future `ShellLayout1` ADR should take 0027).

## Verification evidence (source/static lane — no compiler/runtime, per assignment)

| Check | Command | Result |
| --- | --- | --- |
| Source-shape policy | `./tools/check-source-shape` | exit 0; zero warnings; largest new file 378 non-blank lines (`editor_session_gestures.cpp`) after splitting `editor_session.cpp` (244) to stay under the 500 review threshold |
| Docs/link check | `./tools/validate-docs` | exit 0; all 65 documents validated including both new pages; no broken links introduced |
| Whitespace audit | trailing-whitespace/tab/final-newline grep over all new files | clean |

**Not performed (explicitly):** no compile, no CTest, no GUI/session/input, no
host or user config writes. The five test suites are written but **not
compiled and not run**; the suite counts and exit statuses do not exist yet.
No WYSIWYG, canvas, or live-session behavior is claimed by this candidate —
this is the domain boundary only, per the artifact's own coverage honesty.

## Coordination requests (additive shared-registry edits NOT made; smallest-possible lines for manager/integrator to apply)

1. `src/CMakeLists.txt`: add `add_subdirectory(shell_customization_editor)`
   alongside the other unconditional modules (required for any build).
2. `tests/CMakeLists.txt`: add
   `add_subdirectory(shell_customization_editor)` (its own CMakeLists is
   standalone-capable, so this single line suffices).
3. `mkdocs.yml`: nav entries for
   `shell/customization-editor.md` (under Shell) and the ADR-0026 line (under
   Decisions). `mkdocs build --strict` is not runnable in this lane (mkdocs
   not installed); unnav'd pages would warn under strict mode, so this is
   required for the docs gate.
4. `docs/wiki/architecture/module-boundaries.md`: one source-ownership row for
   `src/shell_customization_editor` ("Customization intent translation, gesture
   state machine, editor session and rollback, user-profile persistence; public
   `shell_customization`, `profiles` values plus Qt Core; never placement
   policy, QML, shell surfaces, or persistence formats outside the store").

None of these files was touched by commit `42200c8`; Victor's paths
(`src/apps/settings/**`, settings_center seam, `data/settings/schema-v2.json`),
`data/applets/`, the roadmap, and all schemas are untouched.

## Bounded caveats

- Code was hand-reviewed against the public engine headers
  (`editing_commands.h`, `editing_result.h`, `layout_editing_repository.h`,
  `layout_editing_coordinator.h`) but never compiled; the first compiler run
  may find mechanical issues (namespace spellings and signatures were
  cross-checked, including the `QindaQt::ShellCustomization` sibling-namespace
  qualifications).
- The in-drag converging-execution model (session tracks the dragged
  instance's preview location and re-targets subsequent moves) matches D3 as
  written, but only the (unrun) session tests exercise it.
- `mkdocs build --strict` unverified (tool absent); link and nav validation
  came from the repository's stdlib validator.

## Requested reviewer action

A different worker: review commit `42200c8` against base `9db68c4` — (a) that
the translator emits exactly the artifact §7.1 command shapes, (b) that the
machine/session uphold invariants 1–6 and never bypass `expectedRevision`,
(c) that no owned-by-others path was modified, and (d) whether the four
additive registration lines above are acceptable to apply at integration.
After integration, the natural next verification step is compiling and
running the five suites in the dependency-light lane.

— Kaito Reed, 2026-08-28T14:10:45Z. Handoff; not live.
