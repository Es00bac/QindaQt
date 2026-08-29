# Celeste Ward rereview: Controls S2 API/docs repair (4 findings)

- **Timestamp:** 2026-08-28T00:02:00Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Assignment:** `1787874954-cora-vale-celeste-api-docs-triage.md`
- **Mode:** read-only rereview of the current diff; nothing edited, built, run,
  generated, or staged.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`; read the current
  `docs/wiki/shell/controls.md`, `src/controls/qml/Button.qml`,
  `src/controls/qml/ThemeCard.qml`, `src/controls/qml/FormRow.qml`,
  `src/controls/qml/DegradedNotice.qml`,
  `tests/controls/qml/BehaviorScene.qml`, and
  `tests/controls/tst_controls_behavior.cpp`, plus Nia Hart's repair rereview
  (`1787874984`) and Jules Reed's qualification plan for cross-reference.

## Verdict

**All 4 findings are source/docs-closed and the supported consumer contract
is now coherent and truthful.** One does not yet carry executable evidence
against its current exact content — see the caveat below before treating
finding 4 as fully proven, not just documented.

## Item-by-item closure

1. **`ThemeCard.available` — CLOSED.** `docs/wiki/shell/controls.md:56` now
   opens the ThemeCard row with "`available` is the caller-owned capability
   input," matching `ThemeCard.qml:15` and the `enabled: available &&
   !previewUnavailable` binding at `:67`. Discoverable and accurate.

2. **`enabled` second-authority override — CLOSED as a documented-unsupported
   decision, not a behavior change.** `controls.md:66-70` states plainly that
   Button/ThemeCard inherit writable `enabled`, that `available` is the only
   supported capability input, that direct `enabled` assignment replaces the
   internal binding and can bypass busy/invalid-preview gating, and that this
   is "a documented QML usage contract rather than a second state authority."
   Matching `AGENT-CONTRACT` comments are in source at `Button.qml:23-24`
   (`enabled: available && !busy`) and `ThemeCard.qml:65-66` (`enabled:
   available && !previewUnavailable`). This is internally consistent with
   Cora's stated rejection of a wrapper/composition redesign, and no test
   claims tamper-resistance that doesn't exist — the acceptance suite still
   only proves the supported paths. Fully closed; nothing further to ask for
   here.

3. **DegradedNotice specialization — CLOSED, one low residual naming
   ambiguity.** `controls.md:55` now states DegradedNotice "specializes
   `StateCard`," names the supported surface as `reason`/`retryText`/
   `retryRequested`, and calls overriding inherited `status`, `message`, or
   `actionText` unsupported because it "would sever the fixed warning and
   alias bindings." `DegradedNotice.qml` itself is untouched (confirmed by
   its unchanged content and an earlier mtime than every other file in this
   repair round), which matches the stated intent of a docs-only fix. **Low,
   non-blocking:** the sentence does not classify `title` (`DegradedNotice.
   qml:13`, also a plain overridable inherited-through-composition binding
   defaulting to "Feature unavailable") as supported or unsupported to
   override. Unlike `status`/`message`/`actionText`, overriding `title`
   doesn't sever any alias binding, so leaving it out may be intentional —
   but the sentence as written lets a careful reader wonder whether `title`
   was omitted by design or by oversight. A half-sentence would remove the
   ambiguity; it does not block this repair.

4. **FormRow naming takeover — docs CLOSED; new focused assertion is
   authored but not yet proven by a reported run.** `controls.md:53` now
   states plainly that "the row's required label and current helper/error
   description intentionally supersede the editor's own `accessibleName` and
   `accessibleDescription`; its native role and value interface remain
   intact" — this matches `FormRow.qml`'s two `Binding { restoreMode:
   Binding.RestoreBinding }` elements (`:35-49`, unchanged from before this
   repair) exactly. The new fixture (`BehaviorScene.qml:93-94`, `accessibleName:
   qsTr("Standalone editor name")` / `accessibleDescription: qsTr("Standalone
   editor description")` on the FormRow-wrapped TextField) and the new test
   assertions (`tst_controls_behavior.cpp:324-327`, asserting those two
   TextField properties still read back the standalone strings while
   `:328-330` asserts the *Accessible* interface exposes FormRow's
   required/error text instead) are exactly the right check for the
   documented contract, and are text-correct by inspection. **Caveat, not a
   defect:** `Button.qml`, `ThemeCard.qml`, `controls.md`,
   `BehaviorScene.qml`, and `tst_controls_behavior.cpp` all carry the same
   filesystem mtime (`2026-08-27T23:55:49Z`), which is after the last
   reported compiler/qmllint/CTest run in
   `1787874645-cora-vale-controls-focused-debug-pass.md` (`2026-08-27T23:50:45Z`,
   19/19 QtTest). No build, qmllint, or CTest result has been reported against
   this exact content, so the "Standalone editor name" assertions are
   currently authored-only, the same evidentiary status Nia flagged for the
   earlier behavior repairs before your focused pass. I did not compile or
   run anything to check this myself, per my read-only boundary.

## Not acceptance

Source/docs closure and truthfulness are confirmed for all 4 findings; the
supported consumer contract (`available` on Button/ThemeCard, the
`enabled`-override warning, DegradedNotice's specialization boundary, and
FormRow's naming takeover) reads as coherent and internally consistent
end-to-end. Executable proof of item 4's new assertion, and any baseline/
visual acceptance, remain yours to run and report. No mutation, execution, or
acceptance is expressed or implied here.
