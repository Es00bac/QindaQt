---
from: curie-the-4th
to: nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: replay-handoff
created_at: 2026-08-28T16:22:51-06:00
---

# Curie the 4th — clean current-manager Power PB-1 replay handoff

- Exact replay commit: `ee488e8aed897ff062ddb850c70c8a5368918f7e`
- Exact tree: `3d42ad991ab6eed3aa0b918d0050ace50363972e`
- Sole parent: `31ba149e5e2abe263cef87764acb4e6487d29c8b`
- Source candidate: `cb34a122c85e0d22208b7dc51e12d14f7226a3bd`
- Branch/worktree: `worker/power-pb1-manager-replay-curie4` at
  `/mnt/d/QindaQt/worktrees/power-pb1-manager-replay-curie4`
- State: clean; no staged or unstaged product changes; no surviving replay
  `qindaqt-power-service` or private `dbus-daemon` process.

## Replay/provenance proof

- Exact 46 candidate paths replayed.
- All 39 non-shared Power production/test blobs match `cb34a122` exactly.
- Seven shared paths are the source candidate plus 127 current-manager
  additions and zero candidate-relative deletions. The current Network,
  Display Color, Settings, Terminal, and adjacent registrations remain.
- Connectivity/fsck, whitespace, mode, tree cleanliness, and one-parent
  provenance checks pass.

## Executable evidence

- Fresh focused strict-warning Debug build: 86/86 actions.
- Fresh focused strict-warning Release build: 86/86 actions.
- Exact Power/Brightness selector: **14/14 Debug** and **14/14 Release**.
- Those rows include private-D-Bus transport, activation, residency, atomic
  service/client behavior, staged install, relocated C++ consumer execution,
  and production boundary policy.
- An external build-confined `QtWayland` poison source is rejected by the same
  boundary checker.
- `tools/validate-docs`: 103 documents PASS.
- strict MkDocs: PASS.
- source shape: PASS over 1,544 files, with the inherited unrelated warning for
  `tests/services/display_color_model/tst_color_model.cpp` at 539 nonblank
  lines. Warnings-as-errors therefore returns 1 for that pre-existing file;
  no Power-owned source/test file reaches 450 lines.
- `git diff --check`, clean exact tree, and residue checks: PASS.

The current manager additionally required the already-built private
`qtermwidget6` prefix to configure because Terminal is now integrated; no
Terminal source was changed.

## Requested next action — gated

Do **not** start replay review until Elan Frost issues the terminal exact verdict
for source `cb34a122`. If Elan passes it, assign a different worker to review
this exact immutable replay `ee488e8`; if Elan finds a blocker, route that
reproduction to Nash and keep this replay preserved but ineligible. This work
contributes zero accepted product evidence in the meantime.
