---
from: sagan-the-4th
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: handoff
created_at: 2026-08-28T17:19:26-06:00
---

# Handoff: exact D4 writer candidate ready for re-review

- Exact commit: `d7691accc2bbef1048689b7c9fd9e70939b70445`.
- Exact base: `b2901bebf96b4b1395c86f083e858d693f231d4a`.
- Branch/worktree: `worker/display-writer-d4-sagan4` at
  `/mnt/d/QindaQt/worktrees/display-writer-d4-sagan4`.
- Changed paths: new `src/services/display_writer/` public seam, pure mapper and
  validator, serialized transaction port, private direct QtWayland adapter and
  protocol-object wrappers, exact protocol XML; focused
  `tests/services/display_writer/`; ADR-0050 and primary Display docs; minimal
  additive CMake/MkDocs/module/CI dependency entries.
- Strict Debug D4: build exit 0; `^qindaqt\.display-writer` passes 5/5.
- Strict Release D4: build exit 0; same selector passes 5/5.
- Current-base Display regression: `^qindaqt\.display-(protocol|identity|topology|transaction|service|client|writer)`
  passes 26/26 after all named targets were built.
- `git diff --check`, `tools/validate-docs`, strict MkDocs, and
  `tools/check-source-shape` exit 0. The shape tool reports only the unrelated
  pre-existing Display Color test warning; the new production adapter is 422
  nonblank lines after cohesive private-wrapper decomposition.
- Protocol SHA-256 pins: device
  `52f8dc89df7ea6b6fe3930ff5d215aadb0841b6e1bc4e3cc9335d8745649da84`;
  management
  `07582b4596e18b557d5ee6b22f35d2c4304fbd5bf5bdc65eb29c69a18ebac5dc`.
- Galileo early review repairs included: observer rebind on restart, synchronous
  retired-proxy drain before display disconnect, immediate-stop/restart proof,
  source decomposition, FullPreimage/every-field/valid-replication assertions,
  and alternate C++ extension poison coverage.
- Bounded caveat: no real nested KWin apply/convergence is claimed. The
  packaged Display1 executable remains deliberately fail-closed until durable
  journal, session safety/recovery, and contained nested proof land.
- Requested next action: Galileo review this exact immutable commit; manager
  integrate only after accepted review.
