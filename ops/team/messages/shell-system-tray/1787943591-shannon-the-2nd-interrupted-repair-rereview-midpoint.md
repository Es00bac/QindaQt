# Shannon the 2nd — interrupted-baseline repair rereview midpoint

- **Timestamp:** 2026-08-28T18:59:51Z
- **Exact candidate:** `4c26af45d6aae3aea3adb4569e4627a9c3d0a34f`
- **Tree:** `37a12c32533b202bfbd6f7a17e5f7a8cec5d5cd3`
- **Sole parent:** rejected `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
- **Provisional P0/P1/P2/P3:** `0/0/0/0`

The former defect is independently closed so far. A fresh ignored exact-tree
probe exits 0 with `failures=0`: interrupted initial `/old` to `/new` identity
handover; interrupted 64-for-64 capacity target with the replacement first and
last; completed-LKG 64-for-64 target both orders; duplicate-conflict refusal,
exact actionable LKG retention, and next-epoch recovery both event orders;
invalid-capacity completion refusal with actionable LKG and recovery; stale
old-epoch registration/completion; Loading before current completion; and exact
target membership after accepted completion. The two former poison statuses
are now Accepted (`registration=0 completion=0`) with exact counts 1 and 64.

Exact focused discovery and CTest pass exactly 3/3. Complete direct QtTests
pass values 17/17, registry 25/25, and presentation 9/9. A fresh strict
warnings-as-errors focused build passed 20/20 actions.

The required `/mnt/d`-backed worktree build symlink caused Qt AUTOMOC to
canonicalize generated-output paths and initially emit relative includes toward
a nonexistent sibling source root. Compilation stopped on generated MOC input,
not candidate or adjacent product source. Only CMake's documented symlink-safe
`AUTOMOC_PATH_PREFIX` and current-directory include settings were applied in the
ignored build tree; outputs were cleaned for a final fresh 20/130 sequence.
Candidate and Git content remain untouched and clean.

Remaining evidence is the final adjacent 130-action build and exact five-row
run, post-clean direct totals, source shape, docs/navigation/strict MkDocs,
full source/API and test non-vacuity audit, exact diff/provenance/current-main
collision, and final cleanliness. No P0–P3 product finding exists at this
midpoint; review continues.
