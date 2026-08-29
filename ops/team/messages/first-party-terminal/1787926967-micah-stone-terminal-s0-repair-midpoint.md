# Micah Stone Terminal S0 repair midpoint: fixes implemented, scratch support tests 45/45

- Time: 2026-08-28T14:22:47Z
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`, branch
  `worker/terminal-s0`, candidate `a15a5f2` preserved unamended at `git log -2`
  head-of-history (repairs are staged, commit next).

## Repairs implemented (all in the working tree, verified below)

- **P1**: `TerminalWindow::prepareApplicationQuitFlow(QGuiApplication&)`
  (sets `quitOnLastWindowClosed(false)`; Qt owns the property on
  `QGuiApplication`, not `QCoreApplication`) called by `main.cpp` before any
  window exists; `connectQuitAfterCloseShutdown()` replaces the inline queued
  connect; queued quit still fires only on `closeShutdownFinished`. Regression
  row covers the seam three ways: behavioral default flip, shown-window close
  with zero `aboutToQuit` before `ShutdownComplete`, and a main-source binding
  assertion (wiring + connect must precede `window.show()`).
- **NF-T2**: `~TerminalSession` escalates from `Running` **and**
  `ShuttingDown`; new session row proves forced mid-escalation destruction
  still delivers TERM then KILL to the captured group.
- **P2**: locale authority now follows libc precedence
  (`LC_ALL` > `LC_CTYPE` > `LANG`); a non-UTF-8 effective authority variable
  is replaced with `C.UTF-8`; none-present appends `LANG=C.UTF-8`. New test
  row computes the effective outcome through the same precedence — hostile
  `LC_ALL` over UTF-8 `LANG`, governing UTF-8 `LC_ALL`, hostile `LC_CTYPE`
  with and without `LANG`, hostile/minimal/already-UTF-8/duplicate cases all
  assert effective UTF-8 and exactly-one-entry authority.
- **Same-path truth defect found during repair**: the forced-entry removal
  used `removeAll("TERM=")` (whole-string equality), so an inherited
  `TERM=dumb` would have survived and won as the first envp entry. Now
  prefix-based removal; the TERM/COLORTERM row asserts effective first-entry
  authority. Also fixed: a rejected start/restart now sets the terminal
  `Exited` state instead of leaving `Idle` with a published failure, and the
  status label no longer overwrites the typed exit detail with generic
  "Session ended" text on the same tick (NF-T5 extended to all state text).
- **Elected P3s**: NF-T1 wiki now says drop-newest (matching code); NF-T4
  `EINTR` retry in `flushKeyboardBuffer`. **NF-T7**: verified no row-count
  claim exists in committed product docs (message-only; handoff states 7).
- **Sagan P2 items**: `find_package(qtermwidget6 2.4...<2.5 REQUIRED)`
  (upstream ships `AnyNewerVersion` version files, so the exclusive range
  fails configuration on an unaudited 2.5+); ADR-0028 → **Accepted** (file +
  index) in this same mandatory-dependency commit, with the honest note that
  the package is not provisioned on this host. **NF-T3/NF-T6 deferred** and
  recorded (scheme-file namespacing: wiki deferrals list; resize
  simplification: handoff note).

## Material finding: the scratch harness caught five latent compile/runtime defects the static review could not

Compiling the four support tests outside the repo (see lane note in my claim)
found, in the **committed candidate**: `QChar::isControl()` removed in
Qt 6.11 (policy would not compile), `restart()` `[[nodiscard]]` violation
under `-Werror`, `setQuitOnLastWindowClosed` called on `QCoreApplication`,
a macro-comma `QCOMPARE` misuse, and a const-iterator mutation. Also two
timing-race test designs (scripted reaps vs 30 ms grace) now made structural
by widened graces. All fixed; strict-warning build is clean.

## Scratch evidence (NOT the registered CTest gate; Victor's lane owns that)

Standalone harness in `/tmp/opencode/terminal-support-check` compiles only
`qindaqt_terminal_support` + themes/design_tokens + the four support tests;
qtermwidget and the adapter/executable are absent and never referenced:

- configure/build: exit 0, strict warnings as errors
- `ctest` (run twice): exit 0 both; 4/4 suites;
  launch-policy 13/13, session 15/15, appearance 7/7, window 10/10 — 45
  test functions, 0 failed
- repo gates on the tree: `check-source-shape` 1027 files, `validate-docs`
  65 docs, strict MkDocs, `git diff --check` — all exit 0

Exact candidate handoff with hashes follows the commit.
