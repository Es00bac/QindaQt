# Cora Vale handoff: immutable QindaQt.Controls S2 candidate

- **Timestamp:** 2026-08-28T03:27:26Z
- **Status:** candidate complete; independent exact-commit review requested
- **Branch:** `worker/controls-s2`
- **Base:** `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **Commit:** `10996f146ff78f69a6f1019933d812d1475faf85`
- **Tree:** `ed48f540b36f8d2d7f1f865d4493d02c74f9daf0`

## Outcome

The candidate adds compiled `QindaQt.Controls 1.0`: 14 cohesive token-only
QML components covering ordinary buttons/text/check/switch/slider primitives,
forms, sections, focus, state/degraded notices, theme choices, and token
swatches. It preserves native keyboard/value behavior, explicit accessible
names/roles/states and warning/error announcements, RTL geometry, long-text
flow, caller-owned availability, busy activation suppression, total hostile
theme-preview rejection, and QST-derived reduced motion/transparency across all
five built-in themes. Installed Controls resolves its sibling Tokens backing
library through a relocatable relative runpath.

Owned paths are `src/controls/**`, `tests/controls/**`,
`docs/wiki/shell/controls.md`, and ADR-0021. The only shared changes are small
additive CMake/module/wiki/nav/testing-harness registrations. No Settings1,
shell, LayerShellQt, Kirigami, AppShell, service client, route, theme-ID branch,
or second palette authority enters the module.

## Exact evidence

- Debug full build: exit 0; exact Controls discovery: 29.
- Debug Controls: **29/29**, exit 0, including behavior/accessibility,
  source policy, 25 visual comparisons, PSS, and clean installed import.
- Release configure and 1,307-step serial build: exit 0; exact discovery: 29.
- Release Controls: **29/29**, exit 0.
- Reviewed visual generation: **25/25**, followed by five contact sheets and
  all 25 originals at compact/ordinary/large 100% and ordinary 125/150%; normal
  comparison then passed **25/25**. Exact hashes are recorded in
  `1787883109-cora-vale-controls-visual-review-accepted.md`.
- Release detailed PSS/package: **2/2**, exit 0; median bare 17,023 KiB,
  Controls 37,322 KiB, delta 20,299 KiB, threshold null; staged installed
  import passed with ambient QML paths cleared.
- `qindaqt_controls_qml_qmllint`: exit 0.
- `all_qmllint`: exit 0; only existing shell-preview warnings outside Controls.
- `./tools/validate-docs`: 46 Markdown documents plus nav, exit 0.
- strict offline MkDocs: exit 0.
- source-shape: 818 files, exit 0; no allowlist skips.
- staged/final `git diff --check`: clean; no build, log, review, temporary, or
  ambient diagnostic artifact is committed.

## Bounded caveat

The broad Debug registry is not claimed clean. Tests 1-135 passed before the
unrelated `shell.production-surface.1080p` stage-1 timeout at test 136. The
manager-requested isolated repeat also failed 0/1 with the same bottom layer
surface becoming unmapped and `protocolAmbiguous: true`; no shell source was
edited and no nested process remained. Exact shell-owner evidence is in
`1787885291-cora-vale-controls-debug-broad-shell-repeat.md`. This does not alter
the independently clean Debug/Release 29/29 Controls evidence.

The worktree is clean at the exact commit and no compiler/test process remains.
Please assign a different worker to review commit
`10996f146ff78f69a6f1019933d812d1475faf85` itself, including generated install
runpath, public API/accessibility semantics, 25 reviewed pixels/process-row
isolation, CMake/package boundary, documentation accuracy, and the explicit
broad-suite caveat. Any blocking finding must return to this same worktree for
a new non-amended repair commit and exact rereview.
