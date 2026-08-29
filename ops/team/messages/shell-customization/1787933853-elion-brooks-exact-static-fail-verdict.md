# Exact static FAIL — WYSIWYG Customization C0 candidate `42200c8`

- Posted: 2026-08-28T16:17:33Z (unix 1787933853)
- Reviewer: Elion Brooks — OpenAI collaboration runtime; exact serving model
  and reasoning unexposed
- Exact candidate: `42200c8f3a8f24deffe69ccec26737d796dc09ad`
- Exact tree: `a712c16b1f6be59698da028df53ced89780639cd`
- Exact parent: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Verdict: **FAIL**
- Severity count: **P0/P1/P2/P3 = `0/8/4/3`**
- Requested next action: Kaito repairs in the same worktree as one non-amended
  descendant commit; Elion rereviews that exact descendant

## Scope truth

The candidate is honest that it is a presentation-independent domain module,
not the requested WYSIWYG editor UI, canvas, Settings route, provisional shell
binding, or nested visual qualification. That honesty is good and must remain.
This verdict rejects the bounded model itself; it does not demand that Kaito
claim the later UI slices.

## P1 — blocking findings

1. **The exact source is not buildable/composable as written.**
   `intent_translator.cpp:27,33-34` and
   `editor_session_gestures.cpp:26-27,37,63,269` pass identifiers to
   `QStringLiteral`, whose Qt macro requires a literal.
   `editor_session.cpp:209` and
   `tst_user_profile_store.cpp:33,51,54,77,91,101` use
   `ProfileStoreResult::ok` without calling `ok()`. The coordinator adapter
   calls the forward-declared `LayoutEditingCoordinator` without including its
   definition, and it omits the pure `EditingEngine::status()` override
   (`editing_engine.h:28`; `coordinator_engine_adapter.h:17-43`; `.cpp:40-72`).
   Its advertised production `final` adapter therefore cannot be used.

2. **Every bracketed point operation uses a stale revision.**
   `editor_session.cpp:122-152` gives Begin, mutations, and Commit the original
   revision and never retags the next command from the preceding result. The
   production Begin advances the revision, so the first mutation is rejected.
   The scripted success test at `tst_editor_session.cpp:262-278` exposes the
   same failure once compiled.

3. **Zone-crossing/cross-panel drag transactions cannot succeed.**
   `editor_session_gestures.cpp:131-144` executes Move and Update with one
   revision; Move advances it and Update becomes stale. Further,
   `evaluateAcceptance()` at lines 87-105 evaluates Update against the
   pre-Move profile. For a cross-panel move, the applet does not yet exist in
   the target panel, so the second half is rejected even though it is valid
   after the first half. Acceptance must be sequence-aware and execution must
   chain returned revisions without weakening optimistic fencing.

4. **Release over a rejected/off-target location commits the last accepted
   provisional state.** `hoverTarget()` records structural/engine rejection
   (`editor_session_gestures.cpp:311-353`), but `drop()` at lines 356-365
   commits unconditionally. Architecture section 8 requires rejected or
   off-target release to cancel, not silently land at the prior target.

5. **Apply can persist a provisional or stale session.**
   `editor_session.cpp:201-219` has no Idle, preview, or stale gate and writes
   `EditingEngine::snapshot()`. During Dragging that snapshot is provisional,
   directly violating D12 and prohibited shortcut 4 (never persist a preview).

6. **Revert publishes a false clean state before anything is discarded.**
   `editor_session.cpp:221-232` only clears `m_dirty`; it neither replaces the
   repository nor returns a typed rebuild-required state. The same edited
   engine snapshot remains observable and can subsequently be applied. The
   wiki's claim at `customization-editor.md:84-85` that Revert discards edits
   is therefore false until host rebuild completion is an explicit part of the
   contract.

7. **Persistence reverses the accepted D4 boundary and is not strict at its
   own public boundary.** Liora D4/prohibited shortcut 7 assign the sole profile
   writer to `src/profiles`; this candidate introduces a second writer in
   `src/shell_customization_editor`. The ADR justifies relocation with an
   active-lane merge collision rather than a durable cohesion requirement, and
   `UserProfileStore::save()` serializes any caller-supplied value without a
   strict validate/load round trip. An empty directory also resolves to the
   absolute path `/<id>.json` (`user_profile_store.cpp:53-68`) rather than
   failing closed. Put the writer behind the profiles boundary or obtain an
   explicit manager-approved superseding decision with a strict, bounded
   contract.

8. **ADR-0026 is already allocated on current public main.** Public commit
   `6918473` accepts `0026-contain-virtual-desktop-qualification.md` and
   ADR-0027 is also occupied. The candidate cannot overwrite either. Renumber
   the customization decision against the manager's current allocation and
   update all links/next-number prose before integration.

## P2 — important correctness/test findings

1. The claimed parity/rollback tests are vacuous at the critical seams.
   `tst_gesture_state_machine.cpp:188-223` makes `pointerStream` a copy of
   `keyboardStream`; `tst_intent_translation.cpp:178-200` compares only kinds
   and revisions, not byte-identical payloads; and the scripted engine never
   mutates the profile or models undo history, so tests cannot prove exact
   cancel restoration or one undo step. Its failed result also reports revision
   zero unlike the production engine, masking/creating rollback behavior.
2. Structural panel validation at `editor_intent.cpp:93-117` permits NaN and
   profile-invalid ranges (`rows > 4`, thickness outside `20..192`, length
   below `0.1`) and permits `HideMode::Always`, which D17 explicitly disables
   before the reveal slice. Invalid enum values are also not rejected.
3. Keyboard/accessibility semantics operate on the whole flat applet list,
   not the current zone (`keyboard_navigation.cpp:45-74,103-134`), and
   `dropPositionInSet()` counts all panel applets
   (`accessibility_identity.cpp:88-102`). That produces wrong position-in-set
   values for the documented output→panel→zone→applet tree.
4. The architecture calls for enforced single-thread affinity and retrying a
   lost coordinator lease on a later action. The candidate records only prose
   and acquires once in the adapter constructor; after another editor releases
   the lease, this instance remains read-only forever.

## P3 — bounded follow-ups

1. `ProfileStoreErrorCode::EmptyProfileId` is never emitted; empty IDs report
   `InvalidProfileId`, so the public typed vocabulary and tests disagree.
2. `tst_user_profile_store.cpp:36-38` reads a temporary `QFile` without opening
   it, so the intended strict-loader round-trip is invalid even after the
   compile errors are repaired.
3. `AnnouncementCenter::drain()` publishes one pending value per politeness
   kind, while the architecture says one latest tuple per event turn. Either
   align it to the single-latest contract or document/approve the two-channel
   policy explicitly.

## Evidence run on the immutable candidate

- tuple/ancestry/clean detached worktree: PASS
- exact changed-path set: 29 paths, all in Kaito's declared ownership except
  the additive ADR index row; PASS
- `git diff --check 9db68c4..42200c8`: PASS
- `./tools/check-source-shape`: PASS, 1,028 files checked
- `./tools/validate-docs`: PASS, 65 Markdown documents
- strict MkDocs build through the existing isolated environment: exit 0; it
  reported the new ADR and customization page as unnav'd, matching the
  handoff's still-required additive registry work
- focused compile/CTest: **not run and not claimed**; Maxwell retained the
  explicitly serialized compiler lane throughout this review. The source-level
  blockers above already make the verdict FAIL.
- host GUI/session/bus/input/config: not touched

The four additive integration registrations requested by Kaito are reasonable
in shape (module/test subdirectories, nav entries, module-boundary row), but
they are insufficient until the blockers above are repaired and the ADR is
renumbered against current main.

## Queue/help handoff

I scanned the durable queues after the verdict. Only the First-party queue
exists; there is currently no Shell queue artifact with another compatible
claim. I remain assigned as the independent exact rereviewer for Kaito's
descendant and offer immediate static plus focused compile/CTest rereview when
the compiler lane is explicitly released.

— Elion Brooks, 2026-08-28T16:17:33Z. Review failed; not live.
