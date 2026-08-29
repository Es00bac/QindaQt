# Nadia Park — exact WYSIWYG Customization C0 repair handoff

- **Time:** 2026-08-28T10:55:44-06:00
- **Status:** review requested; no longer working
- **Exact commit:** `0bffed9c43701aebd7d39c9d31c98319573d6e8c`
- **Exact tree:** `75bed4c52faa41694a5c76d806a1bfa7a63780ee`
- **Exact parent:** `42200c8f3a8f24deffe69ccec26737d796dc09ad`
- **Branch:** `worker/wysiwyg-customization-c0-repair-nadia`
- **Worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/wysiwyg-customization-c0-repair-nadia`
- **Requested next action:** Elion Brooks independently rereviews this exact
  descendant against every finding in exact verdict `1787933853`

## Outcome

This one non-amended descendant preserves the inherited repair and closes the
exact `0/8/4/3` verdict at the bounded editor-domain layer:

- The production adapter compiles, supplies the full engine seam, enforces
  owner-thread calls, retries a lost lease, and evaluates ordered mutations on
  a disposable real repository with the same outputs and manifest catalog.
- Begin/mutations/Commit and every drag mutation are revision-chained. A
  cross-panel zone move now evaluates and executes as one valid sequence and
  becomes exactly one durable undo step.
- Rejected/off-target release cancels the entire preview. Returning from a
  rejected target to an already-applied provisional target remains droppable
  without replaying a no-op command.
- Apply refuses stale, rebuild-required, open-machine, or open-engine previews.
  Revert keeps dirty truth, blocks subsequent edits/persistence, and returns a
  typed host-rebuild requirement instead of claiming the edited snapshot was
  discarded.
- `src/profiles` now owns the sole schema-v1 writer. It rejects empty/unsafe
  destinations and IDs, validates the typed profile, proves strict-loader
  round-trip identity, and atomically replaces prior bytes with direct-write
  fallback disabled. The editor retains only a narrow adapter.
- Profile-v1 bounds and enum validity are enforced; `HideMode::Always` remains
  unavailable until reveal lands. Keyboard slots and accessibility positions
  are zone-local, and announcements publish only the latest tuple per event
  turn.
- Pointer and keyboard event streams are independently constructed; intent
  parity compares full payloads; production tests prove exact cancel restore,
  one undo boundary, revision chaining, rejected release, Apply/Revert truth,
  lease retry, and cross-thread refusal.
- ADR-0026 was moved to unique ADR-0043, and module/wiki/root registrations are
  included.

## Exact changed paths

- `docs/wiki/adr/0026-isolate-the-customization-editor-domain.md` (removed)
- `docs/wiki/adr/0043-isolate-the-customization-editor-domain.md` (added)
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/index.md`
- `docs/wiki/shell/customization-editor.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/profiles/CMakeLists.txt`
- `src/profiles/include/qindaqt/profiles/user_profile_store.h` (added)
- `src/profiles/src/user_profile_store.cpp` (added)
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/accessibility_identity.h`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/coordinator_engine_adapter.h`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/editing_engine.h`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/editor_session.h`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/user_profile_store.h`
- `src/shell_customization_editor/src/accessibility_identity.cpp`
- `src/shell_customization_editor/src/coordinator_engine_adapter.cpp`
- `src/shell_customization_editor/src/editing_command_sequence_p.h` (added)
- `src/shell_customization_editor/src/editor_intent.cpp`
- `src/shell_customization_editor/src/editor_session.cpp`
- `src/shell_customization_editor/src/editor_session_gestures.cpp`
- `src/shell_customization_editor/src/gesture_state_machine.cpp`
- `src/shell_customization_editor/src/intent_translator.cpp`
- `src/shell_customization_editor/src/keyboard_navigation.cpp`
- `src/shell_customization_editor/src/user_profile_store.cpp`
- `tests/CMakeLists.txt`
- `tests/shell_customization_editor/editor_test_fixtures.h`
- `tests/shell_customization_editor/tst_accessibility_navigation.cpp`
- `tests/shell_customization_editor/tst_editor_session.cpp`
- `tests/shell_customization_editor/tst_gesture_state_machine.cpp`
- `tests/shell_customization_editor/tst_intent_translation.cpp`
- `tests/shell_customization_editor/tst_user_profile_store.cpp`

## Verification on the exact commit

- Root dependency-light strict-warning build of all five editor targets and
  eight adjacent profiles/transaction-engine targets: exit `0`.
- `ctest --test-dir build/nadia-root --output-on-failure -j1 -R
  '^qindaqt\.(customize-editor-|profile-|shell-customization-)'`: **13/13
  passed**, exit `0`.
- `./tools/validate-docs`: **65 Markdown documents plus navigation passed**,
  exit `0`.
- `./tools/check-source-shape`: **1,031 source files passed**, exit `0`; the
  largest changed hand-written file is the session test at 493 non-blank
  lines, below the decomposition-review threshold.
- `git diff HEAD^..HEAD --check`: exit `0`.
- `build/mkdocs-venv/bin/mkdocs build --strict`: exit `0`, documentation built
  successfully.
- `git status --porcelain`: empty; exact worktree is clean.

## Bounded caveat

This remains honestly presentation-independent. It does not claim the later
QML editor, canvas, Settings route, provisional shell binding, or nested visual
qualification. No host GUI, compositor, session bus, input device, user
configuration, or host desktop was touched.

— Nadia Park, 2026-08-28T10:55:44-06:00. Exact repair handed off; not live.
