---
name: Claude Program Manager
role: Program Manager and final integrator
provider: Anthropic Claude Code
model: claude-fable-5-1
reasoning: high
status: working
feature: Wave-1 lane dispatch, exact review routing, and manager integration of accepted candidates
worktree: /home/cabewse/work_SPaC3/container-wm
started_at: 2026-09-02T20:42:51-06:00
updated_at: 2026-09-02T20:42:51-06:00
---

# Claude Program Manager

- Role: Program Manager and final integrator. Owns `main`, `docs/HANDOFF.md`,
  `docs/TASK_LIST.md`, `ops/team/features.json`, `ops/team/providers.json`, and
  `ops/team/queues/**`. Took over the manager loop from the OpenAI Codex
  Program Manager on 2026-09-02T20:42:51-06:00 at the user's direction.
- Provider/model: Anthropic Claude Code, `claude-fable-5-1`, reasoning high.
- Status: working — dispatching wave-1 lanes, preserving build infrastructure on
  persistent disk, routing exact reviews, and integrating accepted candidates.
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`.
- Branch: `main`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm`.
- Product authority: integration branch and manager-owned ledgers only.

## Updates

- 2026-09-02T20:42:51-06:00 — Claimed the Program Manager loop. Preserved the KWin 6.6.5 prefix,
  KF6 6.26 prefix, and nested runtime root from the nearly full tmpfs onto
  `/home/cabewse/work_SPaC3/builds/qindaqt-deps/`, generated a reusable
  initial-cache file reproducing the accepted S3 configuration, created a
  persistent docs venv, reconfigured a fresh manager Debug root with 404
  registered rows, and opened six non-overlapping lanes (see
  `messages/team-operations/1788403371-claude-program-manager-wave-1.md`).
