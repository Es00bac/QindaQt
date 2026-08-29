# Mara Voss — Power applet P1 presentation-slice handoff

- Time: 2026-08-28T14:09:00Z
- Owner: Mara Voss, Power applet P1 implementer (GLM
  `zai-coding-plan/glm-5.3-flash`, reasoning high)
- State: complete for this slice; requesting exact-commit review before any
  integration
- **Exact candidate commit: `251c62065dcbc393c3d4067858bf28329f1f881d`**
- Tree: `d2a51f27bc2fae3ed475d0bf0a86cdf7f0c6d71a`
- Parent: `30783867d7f2f49c9ad740c90f1c824614510b72` (Priya-accepted PB-0)
- Branch/worktree: `worker/power-applet-p1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/power-applet-p1`, clean tree

## Outcome

Presentation-only Power applet model: power/battery aggregation rows from the
PB-0 snapshot, charging/discharging/unknown states, time remaining only when
known/bounded/direction-consistent, critical/low/full severity mapped
one-to-one from upstream truth, brightness control request state with typed
failure feedback, keyboard/accessibility identities on every row, and
loading/degraded/unavailable phases. Pure Qt Core values + free functions in
`QindaQt::Shell::PowerApplet`; consumes only public `power_protocol` and
`brightness_model` seams. No UPower/sysfs/logind/Wayland/brightness hardware
access, no policy ownership, no QObject/QML/transport/clocks.

## Changed paths (15 files, +2460 lines)

New: `src/shell/power_applet/**` (module: 3 public headers, private
`power_control_rows_p.h`, 3 sources, unwired CMakeLists), `tests/shell/power_applet/**`
(3 QtTest files, unwired CMakeLists registering 3 rows, `check_boundary.cmake`
static gate), `docs/wiki/shell/power-applet.md` (primary page).
Modified: `mkdocs.yml` (one additive Shell nav line only).

Not touched: the four declared PB-0 integration-conflict paths
(`docs/wiki/architecture/module-boundaries.md`,
`docs/wiki/development/testing-harness.md`, `src/CMakeLists.txt`,
`tests/CMakeLists.txt`), PB-0 sources, production shell, QML registries,
roadmap, `TASK_LIST`, `HANDOFF`, `features.json`, other applets.

## Evidence (source/static lane only — no configure/compiler/CTest/GUI/session/hardware ran)

- `cmake -DSOURCE_ROOT=<repo> -P tests/shell/power_applet/check_boundary.cmake`
  → pass, 10 files scanned (exit 0).
- `python3 tools/validate-docs --root .` → "Validated 66 Markdown documents
  and mkdocs.yml navigation" (exit 0).
- `PYTHONPATH=<isolated> python3 -m mkdocs build --strict` (mkdocs 1.6.1
  installed to a disposable target; not on host PATH) → pass (exit 0).
- `python3 tools/check-source-shape --root . --warnings-as-errors` → pass,
  1024 files, zero warnings (exit 0). Largest owned file: 438 non-blank.
- `git diff --check` staged + full-tree whitespace/tab/conflict-marker scan of
  owned paths → clean (exit 0).
- Tests **not compiled and not run** (0/0): the three QtTest targets
  (`qindaqt_power_applet_presentation_tests`,
  `qindaqt_power_applet_controls_tests`, `qindaqt_power_applet_request_tests`)
  and rows `qindaqt.power-applet-presentation`,
  `qindaqt.power-applet-control-rows`, `qindaqt.power-applet-request-state`,
  `qindaqt.power-applet-boundary` are registered in the candidate's own
  CMakeLists and require the wired seam. This absence is a declared boundary
  of the P1 lane, not a pass.

## Bounded caveats

1. Accessible phrases are deterministic English source strings; localization
   belongs to the future QML facade (documented in the wiki page).
2. The presentation re-checks hostile numbers/enums defensively; PB-0
   validation remains the wire-level authority.
3. `mkdocs` was unavailable on host PATH; strict build used a disposable
   pip-target install (`/tmp/opencode/mkdocs-venv`), no repo or host state
   changed beyond that temp dir.

## Requested additive registry seams (manager-owned, none edited by me)

1. `src/CMakeLists.txt`: guarded `add_subdirectory(shell/power_applet)`.
2. `tests/CMakeLists.txt`: `add_subdirectory(shell/power_applet)`.
3. `docs/wiki/architecture/module-boundaries.md`: one source-ownership row.
4. `docs/wiki/development/testing-harness.md`: four test-matrix rows.
5. Later applet integration (post PB-1/PB-5, wiki page documents order):
   manifest catalog entry, capability policy, compiled built-in registry +
   QML dispatcher entry, shell-private facade exposing this model.

## Requested next action

Manager routes an exact-commit review of `251c620` (Priya Nair is the natural
reviewer given her PB-0 audits; any qualified reviewer works). I will repair
blocking findings in this same worktree as a descendant commit. PB-0 remains
the exact parent; my slice adds no integration-conflict path.

Mara Voss is not live after this handoff; the record is updated accordingly.
