# Controls S2 review repair source-complete midpoint

- Author: Cora Vale
- Time: 2026-08-28T03:53:05Z
- Exact parent: `10996f146ff78f69a6f1019933d812d1475faf85`
- Runtime state: not run; Mira retains the sole compiler/private-runtime lane

The two P2 repairs are authored:

- `qt_query_qml_module` now supplies the one-to-one QML source/deploy-path
  inventory used by installation. The staged gate compares the exact 14 QML
  paths, copies its consumer outside source paths, runs strict `qmllint` with
  ambient QML paths cleared and only staged/system Qt roots, then runs the
  compiled installed module. Existing sibling-Tokens RUNPATH is unchanged.
- StateCard readiness is now private and an internal zero-interval event-turn
  timer coalesces status/title/message mutations. Tests reject synchronous,
  stale, duplicate, and public-bookkeeping behavior while covering every status,
  same-status Warning/Error message updates, and status-before-content order.

The three safe P3 repairs are included: the visual wrapper now distinguishes
process isolation from QST-derived pixel settlement; visual startup requires
both named Noto families and docs no longer claim byte-pinned fonts; the source
policy accepts only `QtQuick`, `QtQuick.Controls`, `QtQuick.Layouts`, and
`QindaQt.Tokens`. StateCard test logic moved to one cohesive helper so the
existing behavior source remains below the decomposition threshold without
changing the 29-test registry.

Static evidence, all exit 0: `tools/validate-docs` (46 documents/nav),
`tools/check-source-shape` (820 files; zero allowlisted), direct Controls source
policy (14 QML), and `git diff --check`. The next action after explicit lane
release is serial configure/build, focused behavior/source-policy/qmllint,
installed inventory/tooling/runtime/RUNPATH proof, both font witnesses, exact
Controls 29/29 Debug and Release, then docs/source/whitespace and a non-amended
descendant commit for Tessa Rowan's rereview.
