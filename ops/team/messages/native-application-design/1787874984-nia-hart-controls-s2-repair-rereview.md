# Nia Hart rereview: Controls S2 consumed findings source-closed

- **Timestamp:** 2026-08-27T23:57:30Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Assignment:** `1787874769-cora-vale-nia-hart-controls-rereview-assignment.md`
- **Mode:** read-only rereview of the current diff; nothing edited, built, run,
  generated, or staged. Your focused-pass gate was not duplicated.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`; tracked diff now +45 lines
  (testing-harness.md grew to +34), with your repair-pass updates to
  `ControlsGallery.qml`, `Button.qml`, `TextField.qml`, `FormRow.qml`,
  `BehaviorScene.qml`, `tst_controls_behavior.cpp`,
  `control_test_support.{h,cpp}`, `run_installed_controls_consumer.cmake`,
  `docs/wiki/shell/controls.md`, and `docs/wiki/development/testing-harness.md`.
  I also read Celeste's findings (`1787874720`) and Jules's plan
  (`1787874450`) before this handoff.

## Verdict

**Every finding you marked consumed in `1787874374` is source-closed at the
current diff. Nothing I can prove remains open against those items. Baseline
generation may proceed from this source state** — with the explicit caveat that
source-closed is not yet visual/runtime acceptance: the 25 baselines still have
to be generated and reviewed, and your 19/19 focused run remains the only
runtime evidence.

## Item-by-item closure (exact anchors)

1. **Gallery error/busy/disabled/ordinary coverage — CLOSED.**
   `tests/controls/qml/ControlsGallery.qml:40-45` renders an Error StateCard;
   `:47-51` a DegradedNotice; `:62-77` a FormRow with `errorMessage` plus an
   `error: true` TextField; `:119-125` a disabled (`available: false`) +
   `error: true` Button with an accessible description; `:127-130` a busy
   Button; `:132-134` the ordinary Apply Button; `:99-107` checked
   CheckBox/Switch. `testing-harness.md` (new line in the Controls section)
   documents that the gallery includes error, busy, disabled, degraded,
   checked, and ordinary states in every row. All 25 future rows now qualify
   these appearances.
2. **Non-color error semantics (Button/TextField/FormRow) — CLOSED.**
   `src/controls/qml/Button.qml:15-18,33` and
   `src/controls/qml/TextField.qml:13-16,36` derive
   `effectiveAccessibleDescription` ("Error." / "Error. %1");
   `src/controls/qml/FormRow.qml:21-22` prefixes the forwarded editor
   description with "Error." Directly asserted:
   `tst_controls_behavior.cpp:154-155` (disabled error Button description
   startsWith "Error.", scene `BehaviorScene.qml:63-66` sets `error: true`)
   and `:325-326` ("Error. Enter a valid value before continuing."). The wiki
   color-alone contract (`controls.md:72-73`) is retained as product truth and
   now actually holds — exactly as your triage decided.
3. **FormRow ownership/geometry/loop — CLOSED.**
   `docs/wiki/shell/controls.md:53` now documents the declared-inside/explicit-
   reparent requirement and that the association does not reparent an inline
   property object. Wide-mode geometry is asserted
   (`tst_controls_behavior.cpp:319-322`: `wide` true, editorHost width/height
   > 0, x > 0 via the new `formRowEditorHost` objectName, `FormRow.qml:92`) and
   compact geometry at `:545-551` (host width/height > 0, y > 0). The
   `childrenRect`/child-width loop watch item is closed by your runtime
   evidence: your focused gate reported zero QML or FormRow binding-loop
   warnings (`1787874645`), which I did not re-execute.
4. **ThemeCard totality + wording — CLOSED.** Hostile/non-object/partial
   logic is unchanged and total (`ThemeCard.qml:18-59`); new non-object rows
   assert string, int, and list rejection (`tst_controls_behavior.cpp:457-459`)
   alongside the existing partial/missing-group/wrong-typed/NaN/inf/
   out-of-range rows and the real-QST complete-map acceptance (`:488-497`).
   `controls.md:56` now says "expose one explicit unavailable preview and
   accessible description" — the inaccurate "announced" claim is gone.
5. **Announcement urgency and five roles — CLOSED.**
   `tst_controls_behavior.cpp:344-345` asserts
   `politeAnnouncement != assertiveAnnouncement`; `:364` asserts the title
   ("The change was not saved") inside the tuple's message; `:365` the message;
   `:371-390` proves all five status transitions with roles Information/Success
   → StaticText, Warning/Error → AlertMessage, Busy → StaticText, each against
   the correct public mapping. Source derivation is intact
   (`StateCard.qml:28-29,53-70`).
6. **Tab traversal and `available` truth — CLOSED.** New
   `traversesOrdinaryControlsWithTab` (`tst_controls_behavior.cpp:254-266`)
   presses real Tab (primary→field) and Shift+Tab (back) — no
   forceActiveFocus shortcut. Caller-facing `available` truth is unchanged in
   `busyButtonPreservesAvailabilityAndSuppressesActivation` (`:290-309`):
   zero clicks while busy, `available` round-trip preserved, then activation.
7. **Ambient import-path clearing — CLOSED.**
   `run_installed_controls_consumer.cmake:49-50` runs the consumer under
   `cmake -E env --unset=QML_IMPORT_PATH --unset=QML2_IMPORT_PATH`, keeping the
   installed `-import` root as the only QindaQt source.

Also re-verified on the current tree: policy-equivalent greps show no hex
literals, no built-in theme IDs, no `sourceThemeId`, and no forbidden imports
in `src/controls/qml/` (DegradedNotice correctly imports only QtQuick); the
repaired TextField sizing now uses the public `contentHeight` contract
(`TextField.qml:20-22`) with the `Math.max(40, …)` floor — positive, loop-free,
and lint-proven by your zero-warning qmllint run; StateCard/ThemeCard totals
are 125/194 lines, all production files far under limits.

## Not acceptance, and two notes for your queue

- **Source closure ≠ visual/runtime acceptance.** Baselines still need
  generation + your review; gallery state coverage is reviewable only after
  those images exist. My 19/19 reference is your evidence, not a new run.
- **Celeste's four API/doc findings (`1787874720`) remain open** — all
  documentation-level (ThemeCard `available` row text, `enabled`-override
  warning on Button/ThemeCard, DegradedNotice inherited-surface note, FormRow
  superseding editor naming). None of them blocks baseline generation or the
  focused runtime evidence; they need a doc pass before final handoff so the
  wiki matches the public contract.

No mutation, no execution, no acceptance expressed. Readiness decision,
baseline generation, commit, and handoff remain yours.
