# Gauss the 2nd — `d08747d` float-geometry exact rereview PASS

- Timestamp: 2026-08-28T14:06:30Z
- Candidate: `d08747d11c79111302f628630c4c16e46d871b2e`
- Tree: `27237b36496484f01b097bd1715aa6f67ed49efc`
- Sole parent: `58f08ba8499b434e36b2746eff773bd29b2e6c45`
- Verdict: **PASS — P0/P1/P2/P3 = 0/0/0/0**

## Exact identity and scope

The previous review tree was clean before its detached HEAD moved from
`58f08ba` to the exact repaired commit. HEAD, tree, sole parent, and final
cleanliness all match Noether's handoff. The repair changes exactly:

- `tests/session/desktop_session_output.py`
- `tests/session/test_desktop_session_output_unit.py`

Their sorted newline-delimited manifest SHA-256 is
`af169209e4fa269ea246f773ed2a5eec8ca48cc88bf94ab069dcabed6b998a3b`.
The diff against `tests/session/desktop_session_topology.py` is empty, so the
previously accepted consumed-dock contradiction ordering and hostile behavior
are byte-unchanged.

## Repair review

At `tests/session/desktop_session_output.py:52-65`, every geometry component
now passes the type gate only when it is a non-boolean `int` or `float`, then
must still equal its exact `(0,0,1920,1080)` value. Scale retains the same
non-boolean integer/float gate and exact equality. This closes the prior
false-negative without weakening value comparison.

The positive regression at
`tests/session/test_desktop_session_output_unit.py:54-62` is non-vacuous: each
subtest converts all four fields in one selected inventory to floats while the
complete validator still consumes both inventories and full boot evidence.
The Outputs and ShellVisibility variants both pass. The existing boolean
origin/scale hostile row and both dock identity hostile rows remain intact.

Additional exact-candidate adversarial calls confirmed:

- equivalent float geometry independently passes Outputs and ShellVisibility;
- booleans in each of x/y/width/height reject for both inventories;
- integer `scale=1` passes;
- inexact float width `1920.5` rejects.

## Source-safe verification

- focused session unit discovery: exit 0, **62/62**;
- in-memory Python `compile()`: exit 0, **14 sources**;
- `tools/check-source-shape`: exit 0, **998 checked**, zero skips;
- `tools/validate-docs`: exit 0, **64 documents** and navigation;
- `git diff HEAD^..HEAD --check`: exit 0;
- topology diff assertion: empty;
- post-check candidate worktree: clean.

I found no candidate-introduced P0/P1/P2/P3. Iris's inherited unrelated P3
readiness-hardening items remain outside this bounded repair and are not
claimed closed. No compilation, CTest, private runtime, compositor, session,
UI, input, display, cursor, host configuration, or hardware action ran. No
product edit was made.

Manager may accept exactly `d08747d11c79111302f628630c4c16e46d871b2e` for
the next serialized compiler/private-runtime preparation boundary, still
subject to the already recorded Settings identity prerequisite. Manager alone
integrates.
