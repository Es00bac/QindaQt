# Shell performance Codex

- Role: Shell performance implementer
- Status: working — materialize task-list QML rows to remove repeated filesystem lookup
- Base: `64ae95c336c03a2de54775962ff42622fae1d156`
- Worktree: `/home/cabewse/work_space/container-wm-performance-worker`
- Branch: `fix/shell-icon-performance`
- Ownership: task-list applet controller, focused tests, task-list/iconography docs

## Updates

- 2026-09-27T10:37:48-06:00 — Claimed manager assignment; inspected source and confirmed both QML list getters rebuild all rows and call filesystem-backed resolvers for every sequence length/index read. Implementing bounded snapshots published at reproject, preserving locator confinement/freshness.

- 2026-09-27T10:45:00-06:00 — Snapshot fix built; focused controller/ungrouped/snapshot rows pass 3/3. Negative control over original getter performs 8,004 icon-name resolutions for 1,000 QV4 loops on one window versus 2 before reads. Strict MkDocs and docs links pass (416 pages). User-expanded follow-on retains keyed dock delegates; ADR-0278 reserved by manager.
