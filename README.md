# QindaQt team delivery record

This is an **orphan branch**. It shares no history with `main` and contains no
product source. It exists to preserve the team's delivery record off-machine.

`ops/team/**` is deliberately excluded from product history — the Bluetooth B0
and Text Editor AppShell exact reviews both verify "zero `ops/team/**` paths"
in a candidate diff, and commit `8e200f3` ("Untrack local ops/team board
artifacts from product history") removed such paths when they leaked in. That
rule is preserved here: the record lives on its own branch, never in `main`.

## Contents

- `ops/team/features.json` — the canonical outcome ledger. Identical to the
  copy tracked in `main`, which is the authoritative one for tooling.
- `ops/team/ROSTER.md`, `OPERATING_MODEL.md`, `providers.json` — the live
  worktree copies, which diverged from the ones `main` tracks. They are not
  uniformly newer, so neither side was auto-merged: `providers.json` here is
  about seven hours fresher and records the remaining Codex capacity and its
  2026-09-03 reset; `ROSTER.md` here carries more roster rows while `main`'s
  has the manager's later prose; `OPERATING_MODEL.md` in `main` is the longer
  and later one. Reconcile by hand before trusting either.
- `ops/team/messages/**` — every claim, midpoint, handoff, finding, review
  verdict, and integration record, by channel.
- `ops/team/workers/**` — durable per-worker employee records.

## How this was assembled

Collected during the 2026-08-28 worktree consolidation by union-merging
`ops/team/` from the `team-board` worktree (the live board) with board files
that existed only in the `display-color-c0-gemini-solene`, `display-d1`,
`text-editor-appshell-claude-keir`, and main worktrees, plus the board paths
committed on `worker/display-settings-d5-prism` and the stray `ops/team-board`
branch. Where a file existed in more than one place the live board's copy won.

Rendered by `tools/team-board/server.mjs` on `main`, pointed at a checkout of
this branch.
