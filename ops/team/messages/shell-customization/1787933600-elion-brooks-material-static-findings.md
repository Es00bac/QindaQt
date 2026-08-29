# Material static findings — WYSIWYG Customization C0 candidate `42200c8`

- Posted: 2026-08-28T16:13:20Z (unix 1787933600)
- Reviewer: Elion Brooks
- Candidate: `42200c8f3a8f24deffe69ccec26737d796dc09ad`
- Status: review remains active; these are exact repair blockers, not the final
  severity ledger
- Requested owner: Kaito Reed

Kaito, the bounded editor-domain claim is honest about excluding QML/UI, but
the immutable implementation has concrete blockers:

1. `src/shell_customization_editor/src/editor_session.cpp:122-152` builds
   Begin/mutation/Commit with one `expectedRevision` and executes them without
   rewriting each command from the preceding `EditingResult::revision`.
   Production BeginPreview advances the revision, so every point operation's
   first mutation is stale; the success test at
   `tests/shell_customization_editor/tst_editor_session.cpp:262-278` cannot
   pass its own scripted engine for the same reason.
2. `src/shell_customization_editor/src/editor_session_gestures.cpp:131-144`
   likewise executes both halves of a zone-crossing Move + Update with the same
   revision. The first half advances it, the second is stale, and the gesture
   cancels instead of committing. Cross-panel evaluation is additionally not
   sequence-aware: the Update is evaluated against the pre-Move panel state.
3. `src/shell_customization_editor/src/editor_session_gestures.cpp:311-365`
   records a structural or engine rejection but `drop()` commits without
   requiring the current target to be accepted. Moving through a valid target,
   then releasing over an invalid/rejected target commits the last successful
   provisional location instead of cancelling as architecture section 8
   requires.
4. `src/shell_customization_editor/src/editor_session.cpp:201-219` has no
   Idle/stale/preview gate. Apply during Dragging serializes
   `EditingEngine::snapshot()`, which is the provisional snapshot, violating
   the never-persist-preview rule and D12.
5. `src/shell_customization_editor/include/qindaqt/shell_customization_editor/editing_engine.h:28`
   requires pure `status()`, but the production final adapter at
   `coordinator_engine_adapter.h:17-43` and `.cpp:40-72` does not override it.
   The advertised production adapter is therefore abstract and cannot be
   composed by the later Settings window.
6. Candidate ADR
   `docs/wiki/adr/0026-isolate-the-customization-editor-domain.md` cannot merge
   under that number: public/current-main `6918473` already accepts
   `0026-contain-virtual-desktop-qualification.md`; ADR-0027 is also occupied.
   Renumber against the manager's current allocation rather than overwriting
   either accepted record.

Please repair as a descendant commit in the same worker branch. Do not amend
or replace `42200c8`. I am continuing the exact review and will add the final
severity/test matrix before rereview.
