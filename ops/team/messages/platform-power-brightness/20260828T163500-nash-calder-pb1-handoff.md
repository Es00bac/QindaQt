# Nash Calder — Power PB-1 candidate handoff

- Time: 2026-08-28T16:35:00-06:00 (2026-08-28T22:35:00Z)
- Worker: Nash Calder, Power PB-1 resident service/client implementer (Z.AI coding plan, `glm-5.3`, reasoning high)
- Exact base: `f783f8389a563423e6e6bf2d98bd276748657a1e`
- **Exact candidate commit: `cb34a122c85e0d22208b7dc51e12d14f7226a3bd`**
- Exact tree: `b6180727e6c1bbaae88264d2bf34cc2a20446caf`
- Sole parent: `f783f8389a563423e6e6bf2d98bd276748657a1e` (non-amended single descendant)
- Branch: `worker/power-pb1-nash`; worktree `/mnt/d/QindaQt/worktrees/power-pb1-nash` (clean at handoff)
- Build root: `/mnt/d/QindaQt/builds/power-pb1-nash` (debug/ and release/)

## Changed paths (46 files, +6123/−38)

- New `src/services/power_service/**` (16 files): collaborator seams
  (`power_collaborators.h`), coordinator (publication + operations + assembly
  split under the source limits), resident ownership, D-Bus object with exact
  fixed introspection, deterministic unavailable collaborators, `app/main.cpp`,
  activation descriptor + systemd user unit + introspection XML, CMake.
- New `src/services/power_client/**` (8 files): transport seam, QtDBus
  transport, async exact-owner client (lifecycle / operations / queued
  completion split), CMake.
- New `tests/services/power_service/**` (6 files) and
  `tests/services/power_client/**` (8 files): fakes, focused QtTest binaries,
  boundary poison-negative script, installed package/consumer gate.
- Smallest additive rows: `src/CMakeLists.txt`, `tests/CMakeLists.txt` (2
  `add_subdirectory` lines each).
- Power docs: `power-service.md`, `power1-v1.md`, `module-boundaries.md`,
  `testing-harness.md`, `index.md`. No ADR created (PB-1 needed no new durable
  decision; ADR-0023/0024/0025 remain governing).

## Tests (exit 0 unless stated)

- Dependency-light Debug full suite: **245/245 passed**
  (`ctest` in `/mnt/d/QindaQt/builds/power-pb1-nash/debug`).
- Power+brightness selection `-R '(power|brightness)'`: **14/14 Debug** and
  **14/14 Release** (`--no-tests=error --output-on-failure`).
- Per-binary assertion totals (Debug): publication 12/0, operations 15/0,
  residency 7/0, client 15/0, qt-transport 3/0, activation 4/0 (passed/failed).
- Static gates: `tools/check-source-shape --warnings-as-errors` (1390 files, 0
  warnings), `tools/validate-docs` (90 documents PASS), `mkdocs build
  --strict` (PASS, venv `/home/cabewse/venv/bin/mkdocs`), `git diff --check`
  (clean).

## Bounded caveats

- PB-1's process publishes honest `Unavailable/upstream-not-integrated`; no
  live UPower/PPD/logind adapter, backlight, idle, Wayland, or UI behavior is
  claimed (PB-2+).
- Pre-existing, unrelated Release-only GCC 16.1.1 `-Wmaybe-uninitialized`
  failure in `src/shell_customization_editor/src/keyboard_navigation.cpp`
  blocks a full light-config Release `--build all`; Release evidence above
  uses the focused power/brightness target set. Reported on the board
  (`20260828T161200`).
- `qindaqt.power-installed-package` stages into its own build-tree prefix; it
  does not install onto the host.

## Requested next action

**Non-GLM exact-commit review** of `cb34a12` by a reviewer with Claude,
GPT, Gemini, or Codex provenance, please: verify base/descendant/tree,
rebuild the focused Debug+Release set, rerun the 14-row selection plus the
boundary/docs/shape gates, and check the AGENT-GUARD/AGENT-CONTRACT markers
stay truthful. I remain available in this worktree for blocking repairs; on
PASS please route to manager integration. Nash Calder is not live after this
handoff; profile updated accordingly.
