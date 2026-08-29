# Micah Stone Terminal ADR-0030 renumber handoff — rereview request to Juno

- Time: 2026-08-28T14:33:10Z
- Exact commit: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
  ("Renumber the Terminal ADR to the reserved ADR-0030")
- Tree: `e263cdd265aa2f722b7d9277dbd61d1593f258e4`
- Parent: `f98d0e194e387bc63d7860de61ff760cf3ec2166` (repaired candidate,
  preserved unamended); ancestry
  `9db68c4023257b49421101fa1b13c73bbc2cfa85` → `a15a5f2` → `f98d0e1` →
  `2386e74`
- Branch/worktree: `worker/terminal-s0`; tree clean
- Exact sorted name-status manifest SHA-256 (13 paths: 12 M + 1 R098 rename):
  `cc70fe78a5a79532f7d3f9ea4a003e5738af9e6a100cec3ade371f27c7a45be9`

## What this descendant does

Per the manager's allocation
(`desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`),
public main owns ADR-0026 (contained virtual desktop) and ADR-0027 (AppShell);
Terminal S0 is reserved **ADR-0030**. This single narrow commit:

- `git mv`s `docs/wiki/adr/0028-confine-qtermwidget-behind-terminal-adapter.md`
  → `0030-confine-qtermwidget-behind-terminal-adapter.md` (R098; the only
  content delta inside the file is the H1 number — the Accepted decision,
  contracts, and consequences are otherwise untouched);
- updates the ADR index row, the `mkdocs.yml` navigation entry, the
  `apps/terminal.md` link and prose, the module-boundaries row and dependency
  paragraph, and every AGENT-marker/CMake comment inside `src/apps/terminal`;
- sweeps clean: zero stale `0028` references remain in docs/src/tests/mkdocs,
  and the complete candidate diff (base `9db68c4` → this commit) contains no
  unintended `0026`/`0027` references.

No product behavior, test, build-wiring, or CI content changed; comments and
documentation links only. No compile, CTest, PTY, GUI, session, or host
desktop/input/config use.

## Gates on the exact committed tree (all exit 0)

- `git diff --check`
- `tools/check-source-shape` — 1027 files checked
- `tools/validate-docs` — 65 documents and navigation validated
- `mkdocs build --strict`

## Requested next action

Juno Park: the pending exact rereview of the repaired Terminal candidate now
applies to tip `2386e74` (source-identical to `f98d0e1` except the ADR
number). Manager: the serialized compiler lane still owns the provisioned
`qtermwidget6 2.4.x` configure/build, the seven `^qindaqt\.terminal-` rows,
current-public focused regressions, and any authorized live gate; the ADR
number in Sagan's merge resolution is now `0030` with status Accepted.
