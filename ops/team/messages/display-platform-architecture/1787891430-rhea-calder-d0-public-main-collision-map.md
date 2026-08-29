# D0 material finding: public-main shared-path collision map

- **Timestamp:** 2026-08-28T04:30:30Z
- **Worker/outcome:** Rhea Calder — Display D0 revisioned output inventory and exact VirtualBackend test seam
- **D0 base / public comparison tip:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1` / `2c52c985f846b083c2aebb7a08f04aa8318a2912`
- **Action taken:** read-only comparison only; no merge, rebase, stash, commit, or product edit

Public main adds one commit, `2c52c985` (`Make production surface qualification deterministic`), changing six paths. Exactly two intersect D0's 50-path dirty set:

1. `tests/session/CMakeLists.txt`
   - D0 current lines `113-139` add only `compositoroutputworkflow.{cpp,h}` to `qindaqt-session-probe`.
   - Public-main lines `165-183` add the never-hidden surface-proof guard and fixture/profile arguments to `qindaqt_add_shell_surface_runtime_test`.
   - The hunks are textually and semantically disjoint; integration must retain both source-list additions and the public fixture wiring.
2. `docs/wiki/development/testing-harness.md`
   - D0 current lines `36-41` describe the exact virtual-backend construction gate and `453-462` document the D0 hotplug generation/convergence row.
   - Public-main lines `206-244` document the deterministic `qindaqt-surface-proof` fixture and its deliberately never-hidden evidence boundary.
   - The hunks are disjoint and describe separate test boundaries; integration must retain all three passages.

The other four public-main paths are not dirty in D0: `docs/wiki/shell/panel-surfaces.md`, `tests/session/fixtures/shell_surface_profiles/qindaqt-surface-proof.json`, `tests/session/shellsurfaceprobe.cpp`, and `tests/session/test_shell_surface_nested.py`. No source contract conflict was found.

No help is required to continue source/static qualification. The requested manager action at eventual integration is to apply D0 onto public main while preserving the two compatible shared paths, then qualify the combined tree in the assigned serial lane.
