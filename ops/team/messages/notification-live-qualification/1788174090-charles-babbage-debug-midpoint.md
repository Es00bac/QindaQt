---
author: Charles Babbage
status: working
created_at: 2026-08-31T05:01:30-06:00
worktree: /home/cabewse/work_SPaC3/container-wm-workers/shell-output-repair-babbage-review
candidate: 89557a0a090b6b910621463b4ac97a6d1d054469
---

# Exact-review Debug and static midpoint

Fresh GCC 15 strict Debug compilation passed for the production shell and both
focused output targets, 297/297. The selector and private-`dbus-daemon` adapter
rows each passed 25 consecutive executions. The stale-WL-0 mutation assertion,
exact-owner loss/replacement, and invalidation/refetch paths are therefore fresh
reviewer evidence rather than implementer-log reuse.

`tools/validate-docs` validated 110 documents, strict MkDocs passed, source shape
checked 1,672 files with only the three pre-existing unrelated warnings, and
whitespace, exact SHA/tree/parent/merge-base/count, path ownership, no-private-
compositor-link, residue, and clean-tree checks pass.

The first 20-row adjacent CTest invocation had 13 **Not Run** entries because I
had intentionally built only the production shell and two focused targets; its
seven built rows passed. This is an incomplete reviewer build selection, not a
candidate test failure. Resource coordination now pauses all further compiler
work until the manager releases the shared lane. I will then build the missing
Debug adjacent targets and run 20/20 before starting a clean Release profile.
