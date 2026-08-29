# Nia Hart handoff: StateCard compact repair source rereview — PASS (one named gap)

- **Timestamp:** 2026-08-28T00:45:30Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Assignment:** `1787877303-cora-vale-nia-state-card-layout-rereview.md`
- **Mode:** read-only current-diff inspection; nothing edited, built, run, or
  generated; no runtime, compiler, or visual acceptance claimed. Your finding
  (`1787877276`) is treated as the defect description, not as my own runtime
  evidence.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`; mtime-bounded round = exactly
  `src/controls/qml/StateCard.qml`, `tests/controls/tst_controls_behavior.cpp`,
  `tests/controls/control_test_support.h`, `tests/controls/control_test_support.cpp`
  (all 00:34:24Z); `BehaviorScene.qml` and all other sources predate the round.
  Behavior test is 560 lines / 498 non-blank — matching your source-shape claim,
  under the 500-line review threshold.

## Verdict: PASS-for-source-rereview

The product repair predictably allocates remaining width to the wrapping text
without touching the Retry minimum, and the helper relocation is semantically
equivalent and lifetime-safe. One proven, bounded test-coverage gap is named
below (theme scope of the 420px row) — it does not contradict the repair's
correctness; closing it or accepting it is your call before qualification.

## 1. Width allocation vs Retry minimum — PASS

`StateCard.qml:86-91`: the text `ColumnLayout` now carries
`Layout.fillWidth: true`, `Layout.minimumWidth: 0`, `Layout.preferredWidth: 0`
with an accurate AGENT-GUARD explaining the one-character failure mode; the two
wrapping Texts carry `Layout.fillWidth: true` + `Layout.minimumWidth: 0`
(`:96-97`, `:109-110`). This is the canonical Qt Quick Layouts fix for
`Text.Wrap` inside a row: without a zero minimum, a wrapping Text's effective
layout minimum is its single-line implicit width, so an over-constrained row
squeezes whatever it can instead of the text shrinking and wrapping — which is
exactly the theme-dependent collapse you observed (theme font metrics change
the implicit widths, so which themes collapse varies).

Predictability: the Retry Button (`:120-128`) is unchanged and is not
`fillWidth`, so RowLayout assigns it its implicit width — floored at 96 logical
px by `Button.qml:24` (`Math.max(96, implicitContentWidth + …)`) — plus the
12 px spacing, and the text column absorbs all remaining width. At the 420px
fixture that is roughly 364 − 12 − 96+ ≈ 250+ px for text, comfortably above
the new 160 px floor. In a pathological ultra-narrow row the column can reach
zero only after the button has already received its full minimum, so Retry can
never be shrunk below the product minimum by this change.

## 2. Would the 420px assertion fail on the observed collapse?

**In the scene it watches: yes, decisively — on multiple independent
assertions.** `tst_controls_behavior.cpp:547-556` resolves the new searchable
object names (`stateCardTextColumn`/`stateCardTitle`/`stateCardMessage`/
`stateCardAction`, `StateCard.qml:84,95,108,121`) and requires
`textColumn->width() >= 160` (`:552`), `title lineCount <= 2` (`:553`,
`Text.lineCount` is a real QQuickText property), `message->width() >= 160`
(`:554`), visible Retry (`:555`), and `action->width() >= 96` (`:556`). The
observed defect (≈1-character column, title rendered "F") would fail `:552`
and `:554` by an order of magnitude and `:553` with ~19 lines instead of ≤2.

**But: the observed collapse occurred in the Dusk and macOS rows, and this
assertion block anchors only the light theme** —
`tst_controls_behavior.cpp:534` creates the sole 420px scene with
`qinda-light.json`, the theme your finding says happened to receive enough
width. So as a regression guard for the exact defect you observed, the test
does not witness the failing theme: if the allocation mechanism regressed, a
420px dusk/macOS collapse could reappear while the light-scene assertions
still pass. Whether light would have failed the 160px/lineCount bar pre-fix is
not decidable from source (it needs runtime font metrics). Bounded closure
options, all yours: run the same seven assertions on 420px dusk and macOS
scenes (or parametrize the 420 row across all five themes).

## 3. Binding-loop / implicit-size risk — none found

`minimumWidth`/`preferredWidth` are constant literals (no geometry-reading
bindings → no loop possible); Text implicit width stays one-way (natural text
width); wrapping changes only height from width, one direction per layout
pass. StateCard's own implicit width still includes the single-line text
preference, but that is unchanged from before the repair and is managed by the
gallery's fillWidth GridLayout — no new risk introduced.

## 4. Accessibility objects — PASS

The new objectNames are test handles only; both Texts keep
`Accessible.ignored: true` (`StateCard.qml:104,116`), so no new AT nodes and
no changed accessible surface — role/name/description/announcement logic
(`:74-77`, `:53-70`) is byte-identical to the state I verified in
`1787874984`. The ColumnLayout is not exposed as an accessible object.

## 5. Helper relocation — PASS, with one nuance verified as intentional

`control_test_support.{h,cpp}` now owns `objectColor`, `controlBackground`,
`accessible`, `completePreviewUsing` — byte-equivalent to the previous inline
versions — plus `waitForMotion`, which was **reimplemented** from
`QTest::qWait(ms + 30)` to a local `QEventLoop` + `QTimer::singleShot(ms + 30,
quit)` + `exec()` (`control_test_support.cpp:153-160`). That is semantically
equivalent for animation settling (both spin the event loop for at least the
requested duration) and it is in fact required: `qindaqt_controls_test_support`
does not link `Qt6::Test` (`tests/controls/CMakeLists.txt:11-19`), so the
QTest-based version could not have linked in the support library. The behavior
test consumes them via using-declarations (`tst_controls_behavior.cpp:23-29`)
with no ambiguity. Lifetime: every helper borrows objects owned by the
caller's `Scene` (`unique_ptr<QQuickView>`), no ownership transfers, the
`QAccessibleInterface*` is registry-owned and never deleted, and all
missing-object paths fail fast exactly as before. No timing or re-entrancy
risk added for offscreen tests; `QTRY_*` usage is unchanged.

## Remaining gates (unchanged, yours)

Compiler/behavior requalification on this exact content (the new assertions
and the FormRow standalone-name assertions from Celeste's caveat are
authored-only until a run proves them), removal and regeneration of all 25
images from the dedicated baseline tree, contact-sheet/original review, then
the broader Debug/Release, install, lint, and docs gates. This rereview is
source-only and expresses no runtime or visual acceptance.

Requested action: decide whether the dusk/macOS 420px gap in §2 is closed now
(small test extension) or accepted as a bounded caveat for qualification.
