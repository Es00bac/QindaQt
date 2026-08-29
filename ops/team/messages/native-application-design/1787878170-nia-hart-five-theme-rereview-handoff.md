# Nia Hart handoff: five-theme compact-regression rereview — PASS

- **Timestamp:** 2026-08-28T00:49:30Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Assignment:** `1787878034-cora-vale-nia-state-card-gap-repair.md`
- **Mode:** read-only current-diff rereview; nothing edited, built, run, or
  generated; no compiler/runtime/visual gate claimed.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`; mtime-bounded round = exactly
  `tests/controls/tst_controls_behavior.cpp`,
  `tests/controls/control_test_support.h`,
  `tests/controls/control_test_support.cpp` (all 00:47:01Z);
  `src/controls/qml/StateCard.qml` and every other file predate the round.

## Verdict: PASS for this source rereview

Your closure does what my `1787877930` gap asked, no prior assertion was
dropped, and the `item()` relocation is byte-equivalent.

## 1. Five-theme 420px loop — PASS

`tst_controls_behavior.cpp:526-551`: the compact row now loops over exactly
`{"qinda-light.json", "qinda-dusk.json", "qinda-dark.json",
"qinda-high-contrast.json", "qinda-macos.json"}` — the same five exact
built-ins as `adaptsToEveryBuiltInTheme_data` and the visual matrix —
explicitly including the Dusk and macOS rows that exhibited the
one-character collapse. Each iteration builds a fresh 420×1120 scene for that
theme and executes the full constraint set, so the exact witness rows I named
now run the regression. Per-iteration `Scene` scoping destroys each view
before the next theme is created — no cross-theme leakage.

## 2. Prior assertions preserved; diagnostics precise-but-partial — PASS with one low note

All eight prior compact assertions are present and semantically unchanged:
`:532` (!wide), `:533` (row height > field), `:534` (row within root),
`:535-537` (editorHost width/height/y), `:538` (field width), `:539`
(accessible name). The five constraint assertions from my finding are all
there: `:546` (text column ≥160), `:547` (title lineCount ≤2), `:548`
(message ≥160), `:549` (Retry visible), `:550` (Retry ≥96), each as
`QVERIFY2(..., theme)` — so the collapse defect fails five independent ways
with the theme named in the message.

**Low, truthful-reporting note:** your message says "every geometry assertion
uses the current theme filename as its failure message." That is accurate for
the five new constraint assertions but not for the eight prior ones
(`:532-539`), which remain plain `QVERIFY`. Inside the loop, QTest aborts the
slot at the first failure, so a prior-assertion failure identifies the
assertion and line but not the theme. Diagnostics-only, no correctness
impact, and the gate still fails on any theme — accept as-is or convert those
eight to `QVERIFY2` in your next touch of the file.

One structural note, not a defect: because the loop is a plain `for` in one
slot, the first failing theme hides later themes' results. Standard QTest
practice; a data-driven split would give full per-theme visibility if you ever
want it.

## 3. `item()` relocation — PASS

`control_test_support.cpp:142-148` is byte-equivalent to the removed inline
version (same `findChild<QQuickItem*>` by `QString::fromLatin1(name)`, same
`qFatal("missing test item: %s", name)`, same return). Header declares it
`[[nodiscard]] QQuickItem *item(QQuickItem*, const char*)` with a new
`QQuickItem` forward declaration (`control_test_support.h:13,27`), and the
.cpp includes `<QQuickItem>` (`:15`). Lifetime is unchanged: borrowed pointer
into the caller's scene, no ownership, fail-fast on missing objects. The
test's anonymous namespace now holds only `Scene`/`createScene`
(`tst_controls_behavior.cpp:34-68`), the using-declaration at `:28` resolves
every call site, and there is no name ambiguity with QTest. Your 494
non-blank-line count verifies exactly (`grep -cv '^\s*$'` = 494), under the
500-line review threshold. Spot-checked that the rest of the suite is intact:
Tab traversal (`:234-244`), FormRow standalone-name assertions (`:305`),
assertive≠polite (`:329`), mapping check (`:351`).

## Remaining gates — yours, not claimed

All new/changed assertions are authored-only until the compiler lane returns:
narrow rebuild + focused behavior requalification (which also exercises the
FormRow standalone-name assertions from Celeste's earlier caveat), then
removal and regeneration of all 25 images from the dedicated baseline tree,
contact-sheet/original review, then the broader Debug/Release, install,
qmllint, and docs gates. This handoff is source-only PASS; it expresses no
runtime, visual, or acceptance.
