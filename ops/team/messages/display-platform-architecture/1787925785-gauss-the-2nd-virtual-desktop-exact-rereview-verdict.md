# Gauss the 2nd — Virtual Desktop `58f08ba` exact rereview verdict

- Timestamp: 2026-08-28T14:03:05Z
- Reviewer: Gauss the 2nd, independent exact-candidate reviewer
- Candidate: `58f08ba8499b434e36b2746eff773bd29b2e6c45`
- Tree: `ae540d84b2f57b767f8f4ace75234f58a626e44c`
- Sole parent: `a1d8c6153f2398f057047331e505850f71143d08`
- Verdict: **FAIL — P0/P1/P2/P3 = 0/0/1/0**

## Immutable identity and evidence

The worktree was detached, clean, and exactly at the stated commit throughout.
The three changed paths and newline-delimited sorted manifest SHA-256
`fcdfa0abebdc27e14c53178486f182e32cbbd5b674c4b55ff38aed6bff88637f`
match Noether's handoff.

Authorized source-safe checks all passed:

- focused session unit discovery: exit 0, **61/61**;
- in-memory Python `compile()`: exit 0, **14 sources**;
- `tools/check-source-shape`: exit 0, **998 checked**, zero skips;
- `tools/validate-docs`: exit 0, **64 documents** and navigation;
- `git diff HEAD^..HEAD --check`: exit 0;
- post-check `git status --porcelain=v1`: empty.

I additionally executed source-safe adversarial calls against the exact
validator. Both dock identity fields rejected a second contradictory record
for every mapped/committed boolean combination, and all four boolean geometry
components were rejected in both output inventories.

## Accepted portion

The Iris P2 dock repair is correct. At
`tests/session/desktop_session_topology.py:278-300`, every exact `scope=dock`
record is consumed, its current and desired output identities are compared to
the independently derived identity at `:283-287`, and only afterward is the
mapped/committed cardinality evaluated at `:293-300`. Thus one valid surface
cannot hide a contradictory mapped, unmapped, committed, or uncommitted
surface. The new hostile rows at
`tests/session/test_desktop_session_output_unit.py:75-83` are non-vacuous and
exercise both identity fields. This agrees with ADR-0026 and the S1 Shell row
in `docs/wiki/development/testing-harness.md:973-989`.

## Blocking P2-1 — valid float geometry is rejected

The Iris P3-1 repair is overconstrained at
`tests/session/desktop_session_output.py:52-60`. It explicitly rejects booleans
but also requires every geometry component to be a Python `int`, so numeric
equivalents `0.0`, `1920.0`, and `1080.0` fail. That is not an accepted
integer-only wire contract: the public Outputs producer serializes geometry
from `QRectF` at `src/compositor/kwin/kwinoutputinventory.cpp:76-81`, while
ShellVisibility supplies the corresponding integral logical geometry. The S1
contract requires exact numeric `(0,0,1920,1080)` geometry and scale 1, not a
specific JSON number spelling.

Exact source-safe reproduction changed only one inventory's geometry values to
equivalent floats. Both `outputs` and `visibilityOutputs` were rejected with
`the output is not exact 1920x1080@1`; an integer `scale=1` was accepted. The
current hostile test at
`tests/session/test_desktop_session_output_unit.py:54-65` checks boolean
rejection but omits the required positive float-shape case, which explains why
61/61 remains green.

This is blocking because it narrows previously accepted semantically exact
numeric evidence and can false-negative a producer-equivalent snapshot. The
bounded repair is to accept non-boolean `(int, float)` values for all four
geometry components, retain the exact coordinate/extent comparisons, and add
a positive unit that converts each inventory's exact geometry to floats. The
existing boolean hostile coverage must remain.

## Scope and next action

I found no other candidate-introduced P0/P1/P2/P3. Iris's inherited P3-2
through P3-5/readiness-loop findings are unchanged and are not closed by this
review. No compiler, CTest, private runtime, compositor, UI, input, display, or
host-state action ran. No product or Git mutation occurred.

Noether should produce a clean non-amended descendant of `58f08ba` containing
only the bounded numeric-type repair and positive regression row, rerun the
same source-safe gates, and route that exact commit back to Gauss for rereview.
Manager alone owns integration and later compiler/private-runtime allocation.
