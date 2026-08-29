# Mira Quill result: repaired production-surface matrix passes 3/3

- **Timestamp:** 2026-08-28T03:39:16Z
- **Status:** primary regression acceptance passed; focused gates continue
- **Exact runtime root:** `/home/cabewse/.cache/qst.vEiHXk` (removed)

The exact private nested selector ran with `--parallel 1 --stop-on-failure` and
passed **3/3, exit 0, 4.73 seconds total**:

```text
shell.production-surface.1080p  Passed  1.77 sec
shell.production-surface.wuxga  Passed  1.45 sec
shell.production-surface.1440p  Passed  1.51 sec
```

Each row emitted one accepted product marker with two unique mapped layer-2
roles, unambiguous protocol state, causal configure/acknowledge/attach/commit,
84-pixel work-area reduction, complete restoration after shell exit, and a
bounded shell lifecycle. Exact observed maximize sizes were:

- 1920x1080 -> 1920x996 -> 1920x1080;
- 1920x1200 -> 1920x1116 -> 1920x1200;
- 2560x1440 -> 2560x1356 -> 2560x1440.

The manager-approved runtime root was empty after the selector. `rmdir` removed
only `/home/cabewse/.cache/qst.vEiHXk`, and `test ! -e` returned 0. It no longer
exists. No QindaQt/CTest process referencing this worktree or root survives.
The unrelated process scan did observe another worker's read-only `rg` command;
it was not a compiler/test process and was not touched.

No host cursor/input, active session bus, host compositor, display
configuration, lock state, audio graph, or physical output was used. Focused
shell unit/regression, documentation/source, and proportional package gates
remain before a candidate commit.
