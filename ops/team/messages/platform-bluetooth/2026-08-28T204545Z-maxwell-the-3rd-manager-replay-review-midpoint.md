# Maxwell the 3rd — Bluetooth B0 manager replay exact-review midpoint

- Time: 2026-08-28T20:45:45Z
- Exact candidate/tree/parent: `f38707b6f7fa2a26a6e3748fe86dd0ccc064aea7` /
  `20b90862537ae317d1301986ef3079eab956e833` /
  `bb2c12919c75c1e98a0cd5ad3d746611bbc18a94`
- Status: working; product source remains immutable and the detached tree is
  clean.

Replay structure is independently proven. Manager base `f783f83` is the exact
ancestor of a five-commit linear range. Relative to that base the candidate has
59 paths, no deletions, and no `ops/team/**` path. All 54 non-shared blobs are
byte-identical to accepted `278a5f95`; the only shared files are additions-only
manager unions: ADR index `+1/+0`, module boundaries `+10/+0`, MkDocs nav
`+3/+0`, product registry `+4/+0`, and test registry `+4/+0`. The added lines
are Bluetooth-only and every existing manager byte is retained.

Fresh strict Debug configure and the focused production/test build pass.
Direct tests pass `16+17+11+12+4+3+4+3 = 70/0`, including full-fidelity private
D-Bus, hostile 257-device bounded decode, activation/restart lineage, and
unique-caller owner-loss lease release. The exact nine-row selector initially
reported 8/9: the whole-tree staged-install gate correctly stopped because
this fresh focused build had not linked an unrelated install target
(`libqindaqt_profiles.a`). This is evidence setup, not a candidate failure.
The unchanged candidate's complete installable graph is building now; I will
rerun the staged row and full selector, then finish package poisons,
docs/MkDocs/source-shape, provenance, object integrity, and cleanliness before
posting the exact verdict.
