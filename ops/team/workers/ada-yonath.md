---
name: Ada Yonath
role: production shell runtime repairer
provider: OpenAI Codex
model: gpt-5.6-sol
reasoning: high
status: handoff
feature: Production shell runtime repair — make the real session usable
worktree: /home/cabewse/work_SPaC3/container-wm-workers/shell-production-runtime-repair
started_at: 2026-09-04T15:32:45-06:00
updated_at: 2026-09-04T17:53:41-06:00
---

# Ada Yonath

- Role: production shell runtime repairer.
- Provider/model: OpenAI Codex `gpt-5.6-sol` (high reasoning).
- Status: handoff — exact candidate `99de545aa521c4d5428b72ee491268e401a5b84c` is green and ready for independent exact review.
- Exact base: `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af`.
- Branch: `worker/shell-production-runtime-repair`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/shell-production-runtime-repair`.
- Product authority: `src/shell/**`, token-only `src/notification_host/**`, terminal-only `src/apps/terminal/**`, minimal cause-driven `src/compositor/**`, `tests/shell/**`, additive `tests/session/**`, `tests/apps/terminal/**`, assigned shell/design-token/reference wiki pages, ADR after 0070 if needed, `mkdocs.yml`, this record, and `ops/team/messages/shell-production-surface/**`.

## Updates

- 2026-09-04T15:32:45-06:00 — Claimed the production shell runtime repair lane at exact base `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af`; normative shell, QST, Controls, task-list, Global Menu, compositor-wire, and current integration documentation read before product inspection.
- 2026-09-04T16:04:56-06:00 — Material finding: the production shell omitted the required QST singleton publication entirely, while the terminal disabled its PTY notifier on the transient `EIO` before the child opened the slave. Implemented fail-closed shell publication plus live token evidence, and retained a guarded slave descriptor until terminal teardown; hostile rows now cover both failure modes.
- 2026-09-04T17:18:27-06:00 — Resumed the preserved work after the manager stopped the earlier windowed run. Re-read the required contracts, confirmed the assigned base and dirty tree, and restricted remaining execution to offscreen/private-bus or KWin virtual-headless rows; no host display/session-bus action will be taken.
- 2026-09-04T17:36:15-06:00 — Verification midpoint: 131/131 focused user rows and 3/3 nonnested DesktopVirtual rows passed in each of Debug and Release. The permitted virtual boot and both panel-visibility rows then passed serially with no surviving private compositor; boot evidence authenticated the live shell and reported ready QST-1 generation `1`, theme `qinda-dark`, and `#171a18` background truth.
- 2026-09-04T17:53:41-06:00 — Handoff: committed immutable product candidate `99de545aa521c4d5428b72ee491268e401a5b84c` (tree `de8b32ee719b7f6509aa2ec172df40e8bac8b7af`). Exact-base negative controls fail for fatal shell warnings, PTY gap, and widget-before-child ordering; repaired Debug/Release evidence and static gates pass. Requested independent exact review then manager integration.
