---
author: Sela North
timestamp: 2026-08-28T11:07:00-06:00
topic: platform-power-brightness
type: claim
status: working
---

# Sela North — Power applet P1 compiled-repair claim

- Implementer: Sela North, permanent Power applet P1 compiled-repair implementer
- Provider/model: Google Antigravity Vertex ADC; `gemini-3.7-flash-high` (reasoning: high)
- Branch: `worker/power-applet-p1-repair-sela`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/power-applet-p1-repair-sela`
- Base commit: `251c62065dcbc393c3d4067858bf28329f1f881d`

## Plan

1. Read AGENTS.md, project wiki index, docs/wiki/shell/power-applet.md, Mara Voss handoff, and manager preservation message.
2. Reproduce the missing generated MOC issue in all 3 focused test executables and repair the CMake/AUTOMOC configuration.
3. Wire the additive test/source subdirectories in root CMakeLists or verify how the build system is structured across the project.
4. Build in a clean build directory with strict warnings enabled; run all 3 Power applet focused tests + adjacent power/brightness tests.
5. Inspect any test failures or subtle implementation/test defects exposed during compilation and fix them without weakening tests.
6. Verify all gates: `tools/check-source-shape --warnings-as-errors`, `tools/validate-docs`, `python3 -m mkdocs build --strict`, link validation, `git diff --check`.
7. Update wiki/ADR facts as required.
8. Check peer messages at midpoint and post midpoint findings.
9. Commit once with an imperative subject and comprehensive contract/verification body.
10. Check peer messages at handoff and post final handoff requesting independent Claude or GLM review.
