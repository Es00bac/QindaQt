# Micah Stone claims the Terminal ADR-0030 rename descendant

- Time: 2026-08-28T14:27:24Z
- Worker: Micah Stone, GLM `zai-coding-plan/glm-5.3-flash`, reasoning high —
  posted by the live process.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`
- Verified before edits: clean HEAD `f98d0e194e387bc63d7860de61ff760cf3ec2166`
  (repair commit preserved, unamended), branch `worker/terminal-s0`.
- Allocation read: `desktop-experience-coordination/1787926849-manager-
  parallel-adr-allocation.md` — Terminal S0 is reserved **ADR-0030**.

## Scope (one narrow non-amended descendant)

1. `git mv docs/wiki/adr/0028-confine-qtermwidget-behind-terminal-adapter.md`
   → `0030-confine-qtermwidget-behind-terminal-adapter.md`; update the H1.
2. Update every index/nav/prose reference: ADR index row, `mkdocs.yml` nav,
   `docs/wiki/index.md` if referenced, module-boundaries row + paragraph,
   owning wiki `apps/terminal.md`, and the AGENT-marker comments plus CMake
   comments inside `src/apps/terminal/**` and `tests/apps/terminal/**`.
3. Sweep the complete candidate diff for stale `0028` and unintended
   `0026`/`0027` references.

No product behavior change, no compile, no CTest, no PTY/GUI/session, no host
desktop/input/config. Gates: source shape, docs validation, strict MkDocs,
whitespace. Exact descendant handoff with hashes and a fresh rereview request
to Juno will follow.
