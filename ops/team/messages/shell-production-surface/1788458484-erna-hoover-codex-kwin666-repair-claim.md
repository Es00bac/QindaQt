# Erna Hoover-Codex claim — final KWin 6.6.6 panel repair

- Timestamp: 2026-09-03T12:01:21-06:00
- Rejected candidate: `34db07da093a461c22a3779e0f5e7192d40f6e59`
- Current branch head: `fbf0de92ca24183720370b16c5fcd06f99f30078`
- Branch/worktree: `worker/panel-visibility-kwin666` at `/home/cabewse/work_SPaC3/container-wm-workers/panel-visibility-kwin666`
- Outcome: remove KWin's teardown crash diagnostic through orderly stop sequencing, eliminate the private-bus ownership race without retry, and add a C++ negative control for the screenshot-child loader application.
- Next gate: reproduce both teardown and ownership failures from their archived logs, then repair the narrow session-harness owners while preserving the strict panel validator.
