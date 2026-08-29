# Nia Hart consolidated source-rereview: Controls S2 review-repair diff

- **Timestamp:** 2026-08-28T04:07:40Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Scope:** read-only audit of your dirty repair diff on parent
  `10996f146ff78f69a6f1019933d812d1475faf85`, against Tessa Rowan's verdict
  (`1787888441`: P2×2, P3×3) and your midpoint (`1787889185`). Diff = 12
  modified files (+238/−113) plus new `tests/controls/
  state_card_accessibility_test.{cpp,h}`. Nothing edited, built, run, or
  generated; no runtime qualification claimed.

## Verdict

**P2-2 (announcements) and all three P3 closures: source-closed, clean. P2-1
(deployment/tooling): implementation correct, but the expanded consumer breaks
the gate as authored** — one HIGH defect, already posted separately
(`1787889920`): `tst_installed_controls.qml:23,24,47,49` assign nonexistent
`labelText`/`helperText`/`title`/`selected` to FormRow/ThemeCard and will fail
both the qmllint and qmltestrunner steps the moment the lane runs. Fix those
four lines (`label`/`description`/`themeName`/`checked`) before releasing the
compiler lane.

## P2-2 — StateCard announcement tuples: CLOSED, no new risks

`src/controls/qml/StateCard.qml`: `accessibilityReady`/`accessibilityRevision`
removed from the public surface and replaced by a private `QtObject`
announcementState (`:52-77`); a zero-interval one-shot `Timer`
(`:79-83`) coalesces status/title/message mutations so one event turn emits
exactly one complete latest tuple; `Connections` schedules on all three
changes (`:84-88`); `publishLatest()` reads status/title/message at fire time,
so a status-before-content turn publishes the final content with politeness
derived from the settled status (`:59-73`); construction stays silent via the
`ready` gate set in `Component.onCompleted` (`:90`). The real
`Accessible.announce` + paired-signal contract is preserved verbatim.

No binding loops (timer/connections carry no geometry-reading bindings),
destruction-safe (timer dies with the component), and publication is now
asynchronous to the mutating turn — documented in the wiki StateCard row
(`controls.md` diff: "coalesce through the next event turn… exactly one
complete latest status/title/message tuple… Construction does not announce,
and readiness/revision bookkeeping is private"), which closes Tessa's
documentation requirement.

Test closure is strong (`state_card_accessibility_test.cpp`): rejects
synchronous publication (`:33`), duplicates via an explicit extra event drain
with an accurate AGENT-GUARD (`:35-38`), and stale tuples via exact full-string
tuple equality (`:41`) — stronger than the old substring checks. Covers all
five statuses plus return (`:61-66`), same-status Warning and Error message
updates (`:68-84`), and status-then-new-content (`:86-97`); asserts the public
bookkeeping properties are gone (`:23-24`) and polite≠assertive (`:21-22`).
The 29-CTest-row registry is unchanged; the helper moved into the behavior
executable, and no other test depended on synchronous announcement timing or
the removed properties (grep-verified).

## P2-1 — deployment/tooling: implementation CLOSED, gate broken by the consumer defect

`src/controls/CMakeLists.txt:31-61`: `qt_query_qml_module` supplies
`QML_FILES` + `QML_FILES_DEPLOY_PATHS`, a one-to-one count guard fails fast, a
target property carries the paths for the test, and per-file `install(... RENAME ...)`
preserves Qt's generated relative layout — no hand-maintained second
inventory, exactly Tessa's requested mechanism. The 14 QML documents will
land beside the unchanged library/qmldir/qmltypes rules; sibling-Tokens
RUNPATH rules are untouched, as required.

`run_installed_controls_consumer.cmake`: requires the exact-14 inventory
(`:49-70`: count check, recursive GLOB of the staged root, sorted-list
equality — catches missing and extra files), copies the consumer into the
staged `tooling-consumer/` directory outside all source paths (`:72-76`),
runs strict qmllint (`--ignore-settings --bare --max-warnings 0`) with
`QML_IMPORT_PATH`/`QML2_IMPORT_PATH` unset and only staged + system-Qt `-I`
roots from the staged working directory (`:78-97`), then the compiled runtime
import from the same staged copy with ambient paths still unset
(`:99-120`). New registry inputs are wired correctly
(`tests/controls/CMakeLists.txt:144-148`, including the
`$<TARGET_PROPERTY:…QINDAQT_QML_DEPLOY_PATHS>` join). This witnesses what the
old gate could not — installed qmllint/type inspection — while preserving the
runtime and RUNPATH proofs.

The only thing standing between this and closure is the consumer defect above.

## P3 closures — all three CLEAN

1. **Isolation comment:** `run_controls_visual_row.cmake:38-42` AGENT-GUARD
   rewritten to the accepted evidence — one process per row for
   selector/renderer/window-state hygiene, with pixel settlement explicitly
   attributed to the QST motion boundary and the superseded causal claim
   removed. Matches ADR-0021 as Tessa required.
2. **Font wording:** `control_test_support.cpp:107-116` comment now says
   "deterministic environment substitution, not byte-pinned font artifact
   evidence"; `tst_controls_visual.cpp:65-71` now requires BOTH `Noto Sans`
   and `Noto Sans Mono`; the wiki fixture prose states the named-substitution
   truth and that changing either family requires baseline review. Option B
   of Tessa's alternative, fully executed.
3. **Import allowlist:** `check_control_source_policy.cmake:13-35` enforces
   the positive allowlist {QtQuick, QtQuick.Controls, QtQuick.Layouts,
   QindaQt.Tokens}. Regex extraction takes the module token only (correct with
   `as` aliases and `1.0` versions), exact `list(FIND)` matching rejects any
   suffixed/unlisted module, and the current 14 files import only allowed
   modules (grep-verified). Wiki prose updated to match.

## Contract/install/tooling/accessibility sweep — no other defects

- **Contract note (not a defect):** removing `accessibilityReady`/
  `accessibilityRevision` shrinks the public surface relative to the parent
  commit, but `10996f1` is unreleased and this repair is its pre-integration
  non-amended descendant, so module revision 1.0 never shipped them — no
  revision bump is required. The wiki no longer mentions them.
- **Install/tooling:** no duplicate inventories (single Qt query, target
  property, one JOIN consumer); staged prefix guard and `REMOVE_RECURSE`
  confinement unchanged; consumer copy + qmllint + runtime all operate on the
  staged copy only.
- **Accessibility:** the private QtObject/Timer/Connections create no
  accessible nodes; role/name/description and the announce tuple contract are
  unchanged; tests still assert roles and the exact tuple.
- **Test false-greens:** the only broken assertion set found is the consumer
  defect (false-RED). The synchronous-publication rejection (`QCOMPARE(size,0)`
  immediately after mutate) is genuinely unsatisfiable by the Timer design, so
  it is a real assertion, not a tautology. All other previously verified
  surfaces (FormRow/RTL/Tab/hostile previews/five-theme geometry) are outside
  this diff and untouched by it.

## Remaining gates — yours, not claimed

Fix the four consumer lines, then (after lane release) serial configure/build,
focused behavior/source-policy/qmllint, installed
inventory/tooling/runtime/RUNPATH proof, both font witnesses, exact Controls
29/29 Debug and Release, docs/source/whitespace, and the non-amended
descendant commit for Tessa's exact rereview. All runtime, visual, and
qualification claims remain withheld — this is a source-only rereview.
