# Priya Nair — PB-0 aggregation candidate audit handoff

- Timestamp: 2026-08-28T13:04:15Z (2026-08-28 07:04 MDT)
- Worker: Priya Nair, QindaQt Power and Brightness Platform Architect
  (GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning variant high)
- Exact audited candidate: `54a19ffc010b1d9ca328d6f93870d7ad7fb54462`
- Tree: `5ec923e5a329b481b4fd28fc7ca6a431f9530769`
- Parent: `3ca676cebc6bb22588b46682be7d90d3a264af5b`
- Worktree: read-only detached
  `/home/cabewse/work_SPaC3/container-wm-workers/power-aggregation-review`,
  HEAD/tree/parent re-verified by `git rev-parse` and clean `git status` this
  session
- Status: finished — audit complete; not final PB-0 qualification (brightness
  descendant open)

## Scope and evidence boundary

Static adversarial source/architecture audit only. I inspected every changed
path of the exact candidate plus the unchanged protocol surfaces it touches.
I configured, compiled, executed, and connected to nothing: no build, no test
run, no D-Bus, UPower, logind, Wayland, sysfs, or host state. Devika's
reported build/static evidence (17-step serial Debug build, 3/3 CTest,
35/35 QtTest assertions, source-shape 998, docs 64, strict MkDocs) is carried
as her claim, not independently reproduced. All line references below are at
the exact candidate tree.

## Inspected paths

Changed (all 15): `src/services/power_protocol/` CMakeLists.txt, headers
`power_aggregation.h`, `power_limits.h`, `power_types.h`, sources
`power_aggregation.cpp`, `power_codec_fields.cpp`, `power_dbus.cpp`,
`power_validation.cpp`; `tests/services/power_protocol/` CMakeLists.txt,
`power_protocol_test_data.h`, `tst_power_aggregation.cpp`,
`tst_power_protocol_values.cpp`; docs `power-service.md`, `testing-harness.md`,
`power1-v1.md`. Unchanged supporting: `power_codec.cpp`, `power_codec_p.h`,
`power_validation.h`, `power_codec.h`, `power_dbus.h`,
`tst_power_protocol_codec.cpp`, parent commit message and wiki context.

## Verdict

**No P0 or P1 blocking defect found.** The candidate conforms to the accepted
PB-0 architecture and fails closed under every hostile class I could reach by
analysis. P2 below is a review-quality gate, not a correctness defect.

### Conformance evidence (independently reasoned)

- Deterministic ordering: all three numeric aggregations sort before
  long-double addition (`power_aggregation.cpp:41-51`, AGENT-GUARD present);
  every remaining reduction (`any_of`/`all_of`/max-rank/unanimous-equality)
  is order-independent, so the permutation guarantee holds structurally, and
  ±0.0 sums are bit-stable (opposite zeros add to +0.0). Zero-NaN reachability:
  all inputs are validation-bounded finite doubles before entering `long
  double`.
- Identity/epoch: per-supply validation against its own handle epoch, then
  single-epoch unification (`:156-159`), makes opaqueId-only dedup
  (`:161-165`) complete; zero epoch, mixed epoch, duplicate, and over-cap all
  return distinct typed errors with the canonical empty composite. Absent
  supplies cannot smuggle truth (`power_validation.cpp:179-186`); absent
  aggregate is exactly `CompositeBattery{}`.
- Bounded arithmetic: percentage computed in `long double` and range-checked
  before publication (`:196-200`); aggregate rate bounded to ±8,000,000 W =
  8 × 1,000,000 (`power_limits.h:38-39`), consistent with the wiki limit table
  added by this commit; energy weighting cannot divide by zero (positive-full
  precondition `:179-182`).
- Conservatism: state precedence `Discharging > Charging > PendingDischarge >
  PendingCharge > all-full > all-empty > Unknown` (`:71-102`) and warning
  ranks Action > Critical > Low > Discharging > None > Unknown (`:23-39`)
  match the wiki verbatim; unknown rate or state poisons the aggregate rate
  (`:104-122,221-228`); estimates are unanimous-passthrough only, never
  derived (`:124-137,254-274`).
- Codec symmetry: the new `BatteryLevel` u32 sits at the identical
  percentage→level→state position in canonical write/read
  (`power_codec_fields.cpp:44-45,69-70; 93,111,120`) and DBus write/read
  (`power_dbus.cpp:91,104-116; 126,137-147`), with new signature pins
  `((ts)ussbbduubddbdbxbxu)` and `(bubduubdbxbxu)`
  (`tst_power_protocol_values.cpp:39-44`). All 12 composite metatypes
  registered (`power_dbus.cpp:41-54`). Canonical layout change is confined to
  this candidate, so no stale golden bytes exist.
- Parent-commit repairs verified in tree: trailing bytes now return typed
  `InvalidValue` (`power_codec.cpp:92-95,166-169`); handle uniqueness is one
  global insertion across supplies/holds/keyboard/internal
  (`power_validation.cpp:248-333`).
- Module boundary: aggregation includes only Qt Core and own protocol headers;
  no DBus/transport types in `power_aggregation.cpp`. Wiki updated in-commit
  and matches implementation exactly (worst-level order, both precedence
  lists, 8 MW and 315,360,000 s limits, module-table row). Largest new file
  is 281 lines.

### Findings

**P2-1 (test gap; repair before manager acceptance of PB-0).** The aggregate
charging estimate branch (`power_aggregation.cpp:264-274`) has zero
executable coverage: every fixture and helper hard-codes
`timeToFullKnown=false` (`power_protocol_test_data.h:37-38,57-58`;
`tst_power_aggregation.cpp:31-32`), and the only estimate row exercises
timeToEmpty (`:172-191`). A field swap between the timeToEmpty/timeToFull
pairs, or an inverted `state == ChargeState::Charging` guard, ships green
through all 35 assertions — and the composite's `timeToFullKnown=true` bytes
are never round-tripped by the codec or DBus rows either. Counterexample
defect it would miss: exchange `timeToEmptySeconds`/`timeToFullSeconds` in
the two `commonEstimate` calls. Required rows: unanimous timeToFull
pass-through on a Charging aggregate; non-unanimous timeToFull stays unknown;
one canonical+DBus round trip with `timeToFullKnown=true`; the
all-discharging −8×1,000,000 W boundary (only the + side is pinned at
`:217-226`).

**P3-1 (reasoned counterexample; unverified by execution).** `energy *
100.0L / full` (`power_aggregation.cpp:195`) can false-reject an
exactly-100% aggregate as `ArithmeticOverflow`. Mechanism: for ≥3 supplies
whose exact energy sums span ≥65 significant bits (requires ~≥12 binary
exponent spread, e.g. a 1.0 Wh supply with 2⁻⁶- and 2⁻⁶³-scale companions,
all energy == energyFull), Σ·100 needs up to 71 significant bits and rounds;
when the product rounds up within the top ~36% of its half-ulp band, the
quotient rounds to the next `long double` above 100.0 and the `:196` check
rejects. Unreachable for ≤2 supplies (their sums stay ≤61 bits through ×100,
exact). Direction is fail-closed (canonical empty, prior snapshot retained)
and physically implausible for UPower truth, so I rate it non-blocking.
Decision for Devika: either switch to `energy / full * 100.0L` (exact 100 at
equal sums) or document the conservative rejection; a runtime row is the way
to confirm, when the compiler lane permits.

**P3-2 (partial precedence pins).** `aggregateCoarseLevel`
(`power_aggregation.cpp:57-69`) is pinned only for Critical vs Low; the
Normal/High/Full/None ranks and None-vs-Unknown are unpinned, and warning
mid-ranks (None vs Discharging) are unpinned (`:135-155` pins Low/Action
only). A transposed order array passes the current suite. Cheap table-driven
rows would close this.

**P3-3 (guard-comment nit).** Dedup by opaqueId alone is sound only because
epoch unification precedes it (`:156-165`). A future editor switching dedup
to `(epoch, id)` pairs without preserving unification would admit same-ID
cross-epoch duplicates as unique — a fail-open hazard. One AGENT-GUARD line
stating that ordering invariant would pin it.

**P3-4 (required PB-1 row, not a PB-0 defect).** `readBoundedArray`
(`power_dbus.cpp:21-37`) keeps draining an over-cap DBus array after setting
`wireValid=false`; bounded termination on a malformed element depends on
QDBusArgument's error-state `atEnd()` behavior, which PB-0's deliberately
bus-free suite cannot exercise. Require a PB-1 private-bus row: hostile
oversized/malformed array must terminate boundedly and the snapshot must be
rejected via `wireValid` before publication.

## Limitations

Static reasoning only; no executable confirmation of any finding, including
P3-1. Devika's test/build claims were not reobserved. The parent protocol
commit was reviewed only through this candidate's tree plus its manager
checkpoint (`1787919451`), not as a separate full audit. Brightness
composition (boundary 3) is out of scope and still open; nothing here is
final PB-0 qualification.

## Requested next action

To Devika Shah: add the P2-1 rows (and, at your discretion, P2-1's negative
boundary and P3-2 pins) in the brightness worktree's test surface or a
superseding aggregation commit — the audited candidate stays immutable — and
decide P3-1 (reorder arithmetic vs document) with a runtime check when the
lane permits. To the manager: I found no blocker; after P2-1 closes, this
candidate is fit to proceed toward integration acceptance alongside the
brightness boundary, with my evidence treated as advisory review, not the
required independent acceptance verdict itself.

## TERMINAL SUMMARY

Candidate `54a19ffc` (tree `5ec923e5`, parent `3ca676c`): PB-0 aggregation
AUDITED, NO P0/P1 BLOCKER, CONFORMANT AND FAIL-CLOSED by static adversarial
analysis; 1 P2 test gap (charging-estimate branch + timeToFull wire bytes
zero coverage), 4 P3 notes (100% long-double false-rejection counterexample,
partial precedence pins, dedup/epoch guard comment, PB-1 hostile-DBus-array
row). Repair to Devika; integration decision to manager. Audit closed
finished; not live after this message.
