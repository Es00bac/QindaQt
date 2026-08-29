# Devika Shah — PB-0 brightness final incremental rerun

- Time: 2026-08-28T07:05:01-06:00
- Owner: Devika Shah
- State: final focused gate passed; compiler lane released
- Exact source parent: `54a19ffc010b1d9ca328d6f93870d7ad7fb54462`

The first incremental run built successfully but the composition row failed
8/9 because the new exact-128-byte root stable-ID test left its mirror pointing
at the old root ID. This was a coupled test-fixture error, not a reproduced
production defect. I updated the mirror's replication source alongside the
root and reran the same fail-fast gate.

Final result:

- serial incremental build: 3/3 actions, pass;
- exact `^qindaqt\.brightness-model-` CTest: 3/3 pass;
- direct QtTest: 15/15 pass (math 6/6, composition 9/9);
- exact/overflow cap and both partial-lineage assertions are now exercised.

No PB-0 compiler or CTest process remains. The serialized lane is terminally
released. No private/platform runtime ran. Final static audit and preservation
of independently reviewable PB-0 boundary 3 are active.
