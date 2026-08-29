# Cora Vale claim: QindaQt.Controls S2

- **Timestamp:** 2026-08-27T20:53:10Z
- **Worker:** Cora Vale — OpenAI Codex runtime; exact serving model and
  reasoning variant are not exposed and are not inferred
- **User-visible outcome:** one compiled, installed, accessible and
  keyboard-complete `QindaQt.Controls 1.0` module for first-party forms,
  cards, notices, theme choices, token swatches, focus, and ordinary actions
- **Exact base:** `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **Branch:** `worker/controls-s2`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`

## Path ownership

- `src/controls/**`
- `tests/controls/**`
- one new primary controls wiki page and its smallest navigation/link edits
- smallest additive `src/CMakeLists.txt` and `tests/CMakeLists.txt` registry edits
- deterministic control fixtures/baselines owned by `tests/controls/**`

No Settings1, shell, LayerShellQt, Kirigami, AppShell, service-client, app-route,
`docs/TASK_LIST.md`, or `docs/HANDOFF.md` paths are owned by this worker.

## Required evidence

- public-component behavior/accessibility, enabled/disabled/busy/error/degraded,
  keyboard/focus, RTL, long-localization, reduced-motion, and
  reduced-transparency offscreen coverage;
- reviewed deterministic five-theme compact/ordinary/large visual baselines,
  plus truthful 125% and 150% renderer rows;
- static no-theme-ID/no-palette-hex checks, compiled QML and lint, installed
  import consumer, strict Debug/Release focused and broad registries;
- documentation/link/source-shape/whitespace gates and measured token-plus-
  controls PSS delta against bare Qt Quick, with no invented threshold; and
- exact committed candidate reviewed by a different worker before integration.

## Collision and dependency risks

QST-1 is the sole palette/transparency/contrast authority. Controls will import
only its public `QindaQt.Tokens 1.0` surface plus Qt Quick, Controls2, and
Layouts. Shared source/test/MkDocs registries will receive additive edits only.
No ADR is currently expected: this slice implements the already accepted
boundary without a new dependency or process/persistence choice. Compilation
starts at one job because the manager reported host swap pressure; full visual
and broad matrices will wait for explicit manager coordination.
