# Priya Nair — PB-0 repaired-commit exact rereview: PASS

- Timestamp: 2026-08-28T13:24:34Z (2026-08-28 07:24 MDT)
- Worker: Priya Nair, QindaQt Power and Brightness Platform Architect
  (GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning variant high)
- Exact reviewed commit: `30783867d7f2f49c9ad740c90f1c824614510b72`
- Tree: `0fb14c92301dd374a8b9d39859ec20f1bbf37aff`
- Parent: `cea3fb9a5b3d1a1aa8d0bc23570218ed86722f05`
- Full PB-0 lineage inspected: `3ca676c` (protocol) → `54a19ffc` (aggregation,
  my first audit) → `cea3fb9a` (brightness) → `3078386` (repair); no rebase,
  no history rewrite; repair diff is exactly Devika's declared seven files
- Verdict: **PASS** — no P0/P1/P2 findings; two residual P3 notes below

## Scope and evidence boundary

Static exact-commit review only. I verified HEAD/tree/parent by `git
rev-parse` with a clean worktree, inspected the full repair diff
`cea3fb9a..30783867`, re-inspected the aggregation/brightness surfaces in the
final tree, and audited the brightness boundary (`cea3fb9a`) fresh this run.
I compiled nothing, ran no test, connected to no bus, touched no host state,
and edited no product file. Devika's recorded evidence (serial build 13/13;
power selector CTest 3/3; QtTest 39/39 = values 14 + codec 11 + aggregation
14; brightness CTest 3/3, QtTest 15/15 = math 6 + composition 9; source-shape
1,011; docs 65; strict MkDocs; isolated install with five public headers) was
traced structurally and reconciles exactly with the registered test functions
and data rows (QtTest totals include init/cleanup: values 12+2, codec 6+3+2,
aggregation 12+2; math 4+2, composition 7+2), the CMake test names, and the
documented selectors. It is carried as her claim, not reproduced.

## Reviewer finding closure (every item from handoff `1787922255`)

1. **P2 charging estimate — CLOSED.** Aggregation: mismatched `timeToFull`
   stays unknown and unanimous publishes only for a Charging aggregate with
   `timeToEmptyKnown` staying false (`tst_power_aggregation.cpp:242-260`).
   Canonical: positive round trip of `timeToFullKnown=true` on composite and
   supply (`tst_power_protocol_codec.cpp:39-61`). QtDBus: validated input
   marshalled through the fixed structure with signature re-pinned
   (`tst_power_protocol_values.cpp:60-83`). My counterexample defect class
   (swapped estimate pairs or inverted guard) is now caught green.
2. **P2 negative rate limit — CLOSED.** Eight discharging supplies at
   1,000,000 W each publish exactly −8,000,000 W
   (`tst_power_aggregation.cpp:323-329`); the + side is retained
   (`:311-321`).
3. **P3 exact-full arithmetic — CLOSED.** Implementation now divides stable
   summed energy by stable summed full energy before scaling, with an
   AGENT-GUARD stating the false-rejection mechanism
   (`power_aggregation.cpp:199-203`); `power1-v1.md:123-126` documents it.
   The exponent-spread row spans 2⁰…2⁻⁶³ across eight exactly-full sources
   and requires exactly 100% (`tst_power_aggregation.cpp:287-313`); I verified
   this row genuinely discriminates — under the old `energy * 100.0L / full`
   the quotient rounds one ulp above 100 and fails `ArithmeticOverflow`,
   while equal bit-identical sums now ratio to exactly 1 → exactly 100.
4. **P3 precedence pins — CLOSED.** Adjacent pairs across the complete coarse
   order incl. Unknown and the complete warning order, each in both
   enumeration orders (`tst_power_aggregation.cpp:161-206`); any adjacent
   transposition of either order array now fails.
5. **P3 lineage guard — CLOSED.** In-source AGENT-GUARD fixes epoch
   unification before opaque-ID-only dedup and names the cross-generation
   fail-open risk (`power_aggregation.cpp:161-164`); `power1-v1.md:142-145`
   records the ordering as contract.
6. **P3 PB-1 hostile-bus row — honestly deferred**, unchanged and unclaimed
   by the testing-harness text; remains required PB-1 scope.

No regression: the repair diff touches only the percentage arithmetic, the
guard comment, tests, and docs; epoch/dedup logic, signed rates, absent truth,
state/warning policy, codec layout, and signatures are byte-for-byte carried
from my audited `54a19ffc`.

## Brightness boundary audit (fresh this run)

`cea3fb9a` conforms to the accepted architecture and module boundaries:
`brightness_model` depends only on public power protocol + Qt Core
(`src/services/brightness_model/CMakeLists.txt:24`); the boundary policy gate
rejects Display headers, DBus/QML/Quick, files, timers, clocks, and QObject
(`check_boundary.cmake`); registry edits are one-line additions
(`src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`, module
boundaries, index) with reciprocal links. Composition is pure, owned,
enumeration-independent (roots/members/keyboards sorted), fail-closed with no
partial projection (`brightness_composition.cpp:56-200`; atomicity pinned at
`tst_brightness_composition.cpp:290-296`); the stale-epoch mapping returns
typed `LineageMismatch` with no current value (`:125-126`, pinned at
`tst_brightness_composition.cpp:247-254`) — Devika's lineage finding
`1787920621` is closed. Owner-loss fences, mirror collapse, ambiguity
persistence suppression, keyboard isolation, and hotplug rebuild are all
pinned. Integer math uses checked 64-bit arithmetic with monotonicity across
all 10,001 normalized values and a consistent quantization bound
(`brightness_math.cpp`; `tst_brightness_math.cpp:42-75`). All files are far
under decomposition limits (largest 318 lines). Install exports match the
power protocol's `QindaQtTargets` convention with all five public headers.
Docs (`brightness-model.md`, power-service, power1-v1, testing-harness) match
the implementation exactly and make no service/runtime claims.

## Residual findings (non-blocking)

- **P3-A:** the QtDBus demarshal path (`operator>>`,
  `power_dbus.cpp:135-151` for CompositeBattery) is never executed by any
  test — coverage is signature + `marshall()` write-direction only. The read
  operators are inspector-verified mechanical mirrors and the canonical codec
  round-trips the `timeToFullKnown=true` bytes, so nothing is at risk today;
  one demarshal-equality row (or the PB-1 private-bus rows) would close it.
- **P3-B:** PB-1 hostile private-bus array bounded-termination row remains
  later scope, as documented; carry it into PB-1's test matrix.

## Verdict

**PASS.** Exact commit `30783867d7f2f49c9ad740c90f1c824614510b72` (tree
`0fb14c92301dd374a8b9d39859ec20f1bbf37aff`) is accepted for
current-public-base merge rehearsal, subject to the manager's combined-tree
verification gate. This rereview does not by itself mark PB-0 complete;
executable maturity and integration claims remain the manager's gates, and
the two P3 notes ride forward as PB-1 scope.

## TERMINAL SUMMARY

PB-0 rereview of `30783867` (tree `0fb14c92`, parent `cea3fb9a`): PASS, NO
P0/P1/P2; all six surviving P2/P3 items from my first audit verified closed
without regression (charging time-to-full evidence at all three wire layers,
−8 MW boundary, ratio-before-scale with a discriminating exact-full row,
complete precedence pins, dedup/epoch AGENT-GUARD, honest PB-1 deferral);
brightness boundary independently reviewed and conformant; two residual P3
notes (DBus demarshal row, PB-1 hostile-array row) carried forward. Accepted
for merge rehearsal pending manager combined-tree verification. Record closed
finished; not live after this message.
