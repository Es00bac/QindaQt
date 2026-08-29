# Progress ledger audit — qualified versus partial integrated evidence

- Timestamp: 2026-08-28T14:12:04Z
- Owner: progress-ledger audit worker
- Scope: read-only reconciliation of the live board; no product, feature-state,
  roster, manager-worktree, session, display, or hardware change
- Live result: **441 / 700 evidence points = 63.00%**, with zero worker parse
  errors

## Exact decomposition

The 63.00% headline already includes partial integrated maturity. The live
implementation assigns MODELLED/WIRED/EXECUTABLE/QUALIFIED scores of
25/50/75/100 (`tools/team-board/board.mjs:8-15`), calculates each named step as
`weight * evidencePercent / 100` (`tools/team-board/board.mjs:211-235`), and
sums those step contributions into its row (`tools/team-board/board.mjs:256-262`).

Across the seven equally weighted 100-point roadmap rows, the 441 points are:

- **QUALIFIED: 355.0 points = 50.71 program percentage points.** This is 300
  points from fully qualified QQ-001/002/003 plus 55 qualified step points
  inside incomplete QQ-004 and QQ-006.
- **EXECUTABLE: 55.5 points = 7.93 program percentage points.** The underlying
  executable steps cover 74 breadth points at 75% maturity.
- **WIRED: 23.0 points = 3.29 program percentage points.** The underlying wired
  steps cover 46 breadth points at 50% maturity.
- **MODELLED: 7.5 points = 1.07 program percentage points.** The underlying
  modelled steps cover 30 breadth points at 25% maturity.
- **ABSENT/UNVERIFIED: 0 points.** These steps cover 195 breadth points.

Arithmetic: `355 + 55.5 + 23 + 7.5 = 441`; `441 / 700 = 63.00%`.
Equivalently, the partial integrated contribution above fully qualified work is
`55.5 + 23 + 7.5 = 86` points, or **12.29 program percentage points**. It is
already inside 63.00%, not something that can be added again.

The row sources are QQ-004 (`features.json:50-108`), QQ-005
(`features.json:122-180`), and QQ-006 (`features.json:196-255`). The live
contract explicitly says worker activity and candidate branches add zero
(`tools/team-board/board.mjs:325-337`; `docs/wiki/contributing/team-board.md:16-19`).

## Useful supplemental views that do not inflate product progress

1. **Integrated evidence breadth:** 505 / 700 = **72.14%** of roadmap breadth
   has at least some accepted integrated evidence. This is a coverage measure,
   not completion: it counts the nominal breadth behind qualified (355),
   executable (74), wired (46), and modelled (30) states once each. It must be
   displayed separately and never added to 63.00%.
2. **Integration queue, not progress:** list exact unmerged candidates by gate
   (`handoff`, `review blocked`, `exact review passed`, `manager verification`,
   `integrated`) and show their exact commit. The currently exact-reviewed
   virtual-desktop repair is `d08747d11c79111302f628630c4c16e46d871b2e`, but
   its own PASS says compiler/private-runtime preparation and the Settings
   identity prerequisite remain (`display-platform-architecture/1787925990-
   gauss-the-2nd-float-geometry-exact-rereview-pass.md:46-65`). It therefore
   stays an **unpriced queued candidate**, not product points.
3. A conditional `potential delta` is honest only after an exact accepted
   candidate maps to one existing canonical step and a justified next maturity:
   `step weight * (target maturity - current maturity) / 100 / 7`. If a
   candidate implements only an unweighted slice of a broad step, report it as
   unpriced until the canonical breakdown is refined. Never present the result
   as current completion and never sum mutually dependent candidates.

## Handoff

Manager can describe the board as **50.71% fully qualified evidence + 12.29%
partial integrated maturity = 63.00% authoritative progress**, alongside the
separate **72.14% integrated-evidence breadth** indicator. I found no honest
candidate-inclusive current product percentage; the honest candidate view is a
separate gate/count ledger or a clearly conditional, non-additive forecast.
