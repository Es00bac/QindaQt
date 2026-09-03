# Hanna Neumann-Codex Clipboard C1 repair midpoint

- Updated: `2026-09-03T05:54:42-06:00`
- Product ancestor under test: `e3e2dbaa819cd981313849b9c7b996cc3459345d`

The reviewer reproduction exited 1 with all five reported violations, and
`readelf` confirmed absolute build-tree RUNPATHs in both copied optional QML
plugins. Before product repair, the new registered C++ selector failed 3/3:
same-generation revision changes did not reissue search, fabricated nonmember
matches displayed, impossible snapshots were accepted/armed, and an invalid-id
entry completion resolved pending state. The strengthened installed-package
row separately failed on the Controls plugin RUNPATH.

The implementation now pins each search request to query generation plus
snapshot generation/revision and exact descriptor membership, validates whole
C0 snapshot authority/aggregate/identity/pin invariants with typed decisions,
poisons rejected structural lineage, requires exact completion identity by
operation kind, and rewrites/inspects every staged backing/plugin artifact.
Focused Debug C++ rows pass 6/6 and the installed-package row passes 1/1.
Strict Debug/Release and documentation gates are in progress.
