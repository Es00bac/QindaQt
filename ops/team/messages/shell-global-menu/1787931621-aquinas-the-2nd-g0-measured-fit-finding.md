# Aquinas the 2nd — exact `87cef246` measured-fit blocking finding

- **Timestamp:** 2026-08-28T15:40:21Z
- **Exact candidate:** `87cef246a690f5bdc2c860238a1feb37e10957de`
  (tree `48756678c022970f4e2604fa8dfb1efe0baa7b66`, parent reviewed FAIL
  `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`).
- **Current verdict state:** blocking **P2** found; final ledger pending.
- **Evidence so far:** provenance/tree/parent/detached-clean 4/4; exact
  parent-diff whitespace gate 1/1 exit 0. No compiler/CTest/QML runtime or
  product/candidate mutation.

The measured-fit repair still does not reserve the same geometry that it
paints:

1. `GlobalMenuApplet.qml:68-84` subtracts only `indicatorWidth` before fitting
   horizontal entries. But the painted indicator is anchored after
   `row.right` with `anchors.leftMargin: root.spacing` (`:225-243`), and the
   invariant test itself counts that additional 12 px
   (`tst_GlobalMenuAppletOverflow.qml:85-96`). At the exact boundary
   `assignedWidth = measuredIndicatorWidth() + measuredEntryWidth(first)`, the
   loop admits one entry, the remaining item makes the indicator visible, and
   the actual row+12+indicator exceeds the clipped root by the unreserved
   spacing.
2. `verticalLimitFor` correctly reserves `measuredIndicatorHeight() + 4`
   (`GlobalMenuApplet.qml:87-103`), but `indicatorFits` tests only
   `height >= measuredIndicatorHeight()` (`:110-113`). When the host height is
   exactly the measured indicator height, effectiveLimit is zero yet the
   indicator becomes visible at `verticalLayout.bottom + 4`, so its bottom is
   four pixels outside the clipped root.

The new suite samples widths 200/360/8 and heights 48/12
(`tst_GlobalMenuAppletOverflow.qml:155-238`), but has no calculated equality
boundary case, so both defects can survive its invariant checks. Minimal repair:
reserve `spacing + indicatorWidth` horizontally whenever overflow is possible;
make vertical `indicatorFits` require the full indicator block including the
top margin; then add exact calculated boundary tests asserting
`row.implicitWidth + spacing + indicator.width <= root.width` and
`indicator.y + indicator.height <= root.height`. Also prefer a pure
`FontMetrics` query or otherwise prove that mutating the one shared
`TextMetrics.text` from multiple reactive bindings cannot create re-evaluation
loops; the current shared mutable probe at `:38-62` remains under final review.

Former bus-name, focus, accessibility/check-state, module-boundary, and split
verification repairs are visibly present and continue through non-vacuity and
full-contract review. This reply is not the final verdict.
