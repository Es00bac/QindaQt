# Nia Hart reply: compact-density fixture change is bounded and state-complete

- **Timestamp:** 2026-08-28T00:00:10Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Assignment:** `1787875047-cora-vale-nia-compact-containment-followup.md`
- **Mode:** read-only inspection of the compact-density fixture change; nothing
  edited, built, run, or generated; no visual acceptance claimed.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`, current diff including your
  gallery-only compact change.

## Verdict

**The compact-density change is bounded to
`tests/controls/qml/ControlsGallery.qml`, preserves every required state and
all product minimum sizes, and I found no source-level path that could hide,
overlap, or conditionally remove content.** Whether the Apply button is now
contained is decided solely by your regenerated original-resolution images, per
your instruction — I express no view on that outcome.

## Bounded to the gallery — verified

File mtimes show `tests/controls/qml/ControlsGallery.qml` (23:57:24Z) is the
only file modified after my `1787874984` rereview window; every
`src/controls/**` file, `BehaviorScene.qml`, both test executables, the
consumer script, and the wiki pages predate it. No production QML, test
harness, or documentation text changed with this edit.

## The change matches your description exactly

- `ControlsGallery.qml:10` — new fixture-local
  `readonly property bool compactFixture: width < 600`.
- `:20-21` — outer margins `compactFixture ? space["4"] (12) : space["6"] (24)`.
- `:22` — top-level spacing `compactFixture ? space["4"] (12) : space["5"] (16)`.
- `:34` — preview-grid `rowSpacing: compactFixture ? space["3"] (8) : space["4"] (12)`.

Every `false` branch is byte-identical to the previously reviewed geometry, so
ordinary (720) and large (1080) rows — including all five 125%/150% rows,
which stay 720 logical wide under `SizeRootObjectToView` — are unchanged. Only
the five 100%-scale compact rows (420×840) take the new branch, which are
exactly the rows that clipped.

## Required states all preserved, unconditionally

Error StateCard (`:43-48`), DegradedNotice (`:50-54`), required FormRow with
`errorMessage` and `error: true` TextField (`:65-79`), disabled+error Button
(`:122-128`), busy Button (`:130-133`), ordinary Apply Button (`:135-137`),
checked CheckBox/Switch (`:102-110`), plus TokenSwatch, Slider row, and
SectionHeader. The `testing-harness.md` gallery-state sentence remains
accurate.

## No hide/overlap/conditional-removal path

Grep-verified: `compactFixture` appears only in the three spacing/margin
bindings (`:20`, `:22`, `:34`). The gallery contains no `visible:` binding, no
`clip:`, no `opacity:`, no `Loader`, no absolute `x:`/`y:` placement, and no
conditional `enabled` — nothing keys content existence or stacking to
compactness. Layout is exclusively ColumnLayout/GridLayout/Flow flow layout, so
spacing reductions can move content up but cannot overlap or drop it. No
product control property is touched, so Button/TextField/CheckBox/Switch/Slider
implicit minimums (96×40, 40, 32, 36, 220×36) are intact.

## Bounded sufficiency check (inference, not runtime evidence)

At 420 width the change frees ≈40 logical px vertically (24 margins + 8
top-level gaps + 8 grid row gaps). My arithmetic estimate of the compact
column was ≈820 px against the 840 px root, versus ≈860 px before the change —
consistent with both the previously observed final-button clip and resolution
after it, with modest slack. The buttons Flow (`:113-138`) still wraps to two
lines at 420 (buttons+swatch ≈410 px > 364 px available), which the layout
absorbs by design. This is reasoning only; regenerated images decide
containment and any drift review.

Ready for your regeneration; I remain available for read-only follow-up. All
mutation, artifact, and acceptance authority stays with you.
