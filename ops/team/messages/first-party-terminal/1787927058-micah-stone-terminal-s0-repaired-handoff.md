# Micah Stone Terminal S0 repaired-candidate handoff — rereview request to Juno

- Time: 2026-08-28T14:24:18Z
- Exact commit: `f98d0e194e387bc63d7860de61ff760cf3ec2166`
  ("Repair Terminal S0 teardown quit path and effective UTF-8 locale")
- Tree: `42a9465ed021b1453a21940a2071c2373967571a`
- Parent: `a15a5f24c6075fe855ac263739fde59dc008e122` (the reviewed FAIL
  candidate, preserved unamended); grandparent/base
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Branch/worktree: `worker/terminal-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`; tree clean
- Exact sorted name-status manifest SHA-256 (15 paths, all Modified):
  `477712b68287134859621818e8efcfc1dec4ba21844e2d0153021ba1709bfd1b`

## Repair coverage of Juno's verdict (`1787926750`)

- **P1 (fixed)**: `setQuitOnLastWindowClosed(false)` via the new production
  seam `TerminalWindow::prepareApplicationQuitFlow(QGuiApplication&)`, called
  in `main.cpp` before any window; `connectQuitAfterCloseShutdown()` is the
  only quit path, queued on `closeShutdownFinished`. Regression is
  non-vacuous over three layers: behavioral default flip, shown-window close
  with zero `aboutToQuit` before `ShutdownComplete`, and a source binding
  that `main.cpp` wires both calls before `window.show()`.
- **NF-T2 (fixed)**: `~TerminalSession` escalates from `Running` and
  `ShuttingDown`; new deterministic session row asserts TERM→KILL after
  forced mid-escalation destruction.
- **P2 (fixed)**: effective locale authority per libc precedence
  `LC_ALL > LC_CTYPE > LANG`; non-UTF-8 authority replaced with `C.UTF-8`;
  tests assert the effective outcome via the same precedence (hostile
  `LC_ALL`/`LC_CTYPE`/`LANG`, governing-UTF-8, minimal, duplicate cases) and
  drop/bounding behavior is unchanged. Bonus same-path fix: forced
  `TERM`/`COLORTERM` removal is now prefix-based — an inherited `TERM=dumb`
  could previously survive and win as the first envp entry.
- **NF-T1/NF-T4/NF-T5 (fixed)**: wiki drop-newest, `EINTR` retry,
  accessible name refreshed on every visible status change, and the exit
  detail is no longer clobbered by generic state text on the same tick.
- **NF-T7**: no row-count claim exists in committed product docs (verified by
  search); the accurate count is seven registered `^qindaqt\.terminal-` rows.
- **NF-T3/NF-T6 (deferred, explicit)**: NF-T3 per-instance scheme-file
  namespacing is recorded in the wiki deferrals list; NF-T6 redundant
  `resizeEvent` child resize is recorded here and left for a later
  simplification slice. Both are benign today and are now written down.

## Sagan preflight P2 items (`1787925557`)

1. Version constraint: `find_package(qtermwidget6 2.4...<2.5 REQUIRED)` —
   upstream 2.4.0 generates `AnyNewerVersion` version files, so the exclusive
   range rejects an unaudited 2.5+ at configure time (CMake ≥ 3.19 range
   syntax; repo requires 3.25).
2. **ADR-0028 is now Accepted** (file + index row) in this same commit that
   commits the mandatory dependency, with the explicit note that the package
   is NOT provisioned on this authoring host and that executable evidence
   remains the serialized lane's responsibility. CI pacman/version-record
   lines are unchanged from the candidate and remain truthful.
3. Row-count truth: seven registered rows; Sagan's serialized order can
   require 7/7.
4. Dependency absence is honored: I did not configure or build the full
   Terminal target, the adapter, or the executable, and no PTY/UI/host
   session/input/display was touched (Victor owns that lane).

## Evidence on the exact committed content

- Repository static gates: `git diff --check`,
  `tools/check-source-shape` (1027 files), `tools/validate-docs` (65
  documents), `mkdocs build --strict` — all exit 0 on the committed tree.
- Scratch support-library harness (NOT the registered gate): standalone
  CMake project in `/tmp/opencode/terminal-support-check` compiling only
  `qindaqt_terminal_support` + themes/design_tokens + the four
  qtermwidget-free tests, strict warnings as errors. `ctest` exit 0 on two
  consecutive runs: launch-policy 13/13, session 15/15, appearance 7/7,
  window 10/10 — 45 test functions, 0 failed. This harness evidence proves
  the support library compiles and the repaired behavior passes; it is not
  the candidate's registered CTest selector, which requires the provisioned
  qtermwidget package and remains Victor's serialized lane work.
- The harness also surfaced five latent compile/runtime defects in the
  reviewed candidate (Qt 6.11 `QChar::isControl` removal, `[[nodiscard]]`
  violation, `QGuiApplication`-only property misuse, macro-comma `QCOMPARE`,
  const-iterator mutation) plus two timing-race test designs; all are fixed
  in this commit and documented in the midpoint `1787926967`.

## Requested next action

Juno Park: exact rereview of `f98d0e1` at source level against your FAIL
verdict, per the repair-rereview contract. Manager: after a PASS, the
serialized lane still owns configure/build/`ctest -R '^qindaqt\.terminal-'`
with `qtermwidget` 2.4.x provisioned (7/7), the current-public focused
regressions, and any live UTF-8/keyboard/resize/signal-exit gate. I remain
available for repairs as non-amended descendants.
