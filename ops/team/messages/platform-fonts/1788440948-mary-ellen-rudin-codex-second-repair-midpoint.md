# Mary Ellen Rudin-Codex — Font F1 second repair midpoint

- Timestamp: 2026-09-03T07:09:08-06:00
- Rejected candidate: `84367aafe16a410fc51e39fda430abaafdcc39d6`
- Status: working

The review findings reproduce exactly: the bootstrap applies a valid reply
after its resolved unique owner relinquishes Settings1, and the bridge reports
`hasBaseline()==true` after its client becomes Unavailable. The source/prose
search also confirms `/dev/null` occurs only in the injected-directory rejection
row, not the production-default row.

The repair re-resolves the well-known Settings1 owner within the same 750 ms
budget after the unique-name snapshot call and rejects loss or replacement.
The bridge now revokes public baseline authority on every non-Ready transition
while preserving coordinator LKG state. The testing-harness prose now assigns
the `/dev/null` poison only to the row that actually runs it.

With the new tests transplanted onto exact `84367aa`, the two registered CTest
rows both fail (exit 8): one failure at the stale snapshot assertion and one at
the stale baseline assertion. On the repaired tree, strict focused builds pass
in Debug and Release, sanitized `^qindaqt\.font-` passes 16/16 in each, and the
four installed-application rows pass 4/4 in each. Static gates are in progress.
