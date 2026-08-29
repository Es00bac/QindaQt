# Cora Vale assignment: Nia Hart Controls repair rereview

- **Timestamp:** 2026-08-27T23:52:49Z
- **To:** Nia Hart
- **From / decision owner:** Cora Vale, Controls S2 lead/keeper
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Mode:** read-only current-diff rereview; do not edit, build, run tests, generate images, or commit

Please reread your findings in `1787874240-nia-hart-controls-s2-audit-findings.md`
and terminal audit `1787874320-nia-hart-controls-s2-terminal-audit.md`, then verify
each item I marked consumed in `1787874374-cora-vale-nia-hart-audit-triage.md`
against the current files. Focus on whether the authored state truly closes:

- visual-fixture coverage for error, busy, disabled, and ordinary controls before
  the 25-image baseline is generated;
- non-color accessibility semantics for Button, TextField, and FormRow error state;
- FormRow explicit editor ownership/reparenting documentation, positive compact and
  wide geometry, and no obvious childrenRect/width-loop regression;
- ThemeCard hostile/non-object/partial totality and accurate selected-theme
  announcement wording;
- distinct Warning/Error announcement urgency and all five StateCard status roles;
- ordinary-control Tab traversal and caller-facing `Button.available` truth;
- installed-consumer ambient import-path clearing.

The repaired focused Debug gate now passes: Controls qmllint has zero warnings,
focused CTest is 3/3, and behavior QtTest is 19/19 without QML/binding-loop
warnings (`1787874645-cora-vale-controls-focused-debug-pass.md`). Do not duplicate
that execution. Post one new timestamped rereview finding/handoff that names exact
files/lines for any still-open issue, or explicitly confirms that every consumed
finding is source-closed and baseline generation may proceed. I retain all mutation,
compiler, baseline, commit, and final handoff authority.
