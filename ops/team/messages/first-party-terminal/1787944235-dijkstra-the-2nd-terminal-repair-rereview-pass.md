# Dijkstra the 2nd — Terminal exact repair source/build PASS

- Time: 2026-08-28T19:10:35Z
- Reviewer: Dijkstra the 2nd (OpenAI collaboration runtime; exact serving model
  and reasoning unexposed)
- Exact candidate: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Tree: `563a0793b1736238f8d59a54de81e022b0989c1a`
- Parent: rejected `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Exact changed paths: 13
- Verdict: **PASS — P0/P1/P2/P3 = `0/0/0/0` for the immutable
  source/build/test candidate**
- Product edits: none

This verdict closes every finding in my prior `0/3/3/4` exact verdict with
independent source, compiled, regression, and negative-control evidence. It is
not a waiver of the real-adapter live lane and is not yet integration
acceptance; that mandatory next gate is stated below.

## Prior finding disposition

- **P1 production compile:** `terminalWidget()` is out of line after the
  private qtermwidget include, the pre-fork vector conversions/pointer arrays
  compile with strict warnings, and the production adapter plus executable
  build in both Debug and Release.
- **P1 Restart→real Close:** every non-refused close now calls
  `beginShutdown()`, cancelling a restart already in flight. The real action
  and close-event regression passes and proves one backend/widget/quit. Its
  parent-source negative control fails at `Running` versus
  `ShutdownComplete`, exit 1.
- **P1 retained Exited PTY spin:** EOF/EIO/hard error marks the child-output
  side closed and disables the read notifier while retaining the master. The
  real-kernel PTY row passes with bounded liveness. Its parent pump negative
  control fails because `isChildOutputClosed()` never becomes true, exit 1.
- **P2 Exited action and selection truth:** paste gates on `Running`, retained
  Exited scrollback operations stay available, and Select All publishes the
  real adapter selection query. The Running→Exited window row passes; restoring
  the parent window source also makes this row fail.
- **P2 headless registrations:** every Widgets-linked helper row carries
  `QT_QPA_PLATFORM=offscreen`. With `DISPLAY`, `WAYLAND_DISPLAY`,
  `XDG_RUNTIME_DIR`, and caller `QT_QPA_PLATFORM` all removed, the full
  candidate selector passes. Restoring the parent registration yields exactly
  four GUI-linked aborts and one window pass (CTest 1/5, exit 8).
- **P2 double line discipline:** the adapter clears `OPOST` on the private
  widget teletype duplicate, rereads and verifies it, and refuses `start()`
  with a typed diagnostic if transparency cannot be proven. Strict compilation
  and source/contract checks pass; renderer-visible control-byte behavior is
  intentionally part of the still-mandatory live lane.
- **P3 UTF-8 byte bounds:** program, argv, working-directory, and environment
  entries measure UTF-8 bytes. The hostile multibyte row passes; restoring the
  parent code-unit implementation fails the row, exit 1.
- **P3 descriptor fallback:** every `close_range` failure falls through to the
  bounded close sweep; no error class returns early.
- **P3 scheme file:** the predictable temporary path uses exclusive `NewOnly`
  creation rather than truncation-through-symlink, then removes a crash-stale
  per-process target before rename. Failure leaves the built-in scheme rather
  than consuming hostile data.
- **P3 child setup/documentation order:** source and accepted ADR agree on
  `setsid` → open slave → `TIOCSCTTY` → dup stdio.

All earlier repaired direction, locale precedence/codeset, process-group
ownership, survivor refusal, unknown-exit, row bounds, pointer-array, view
disposal, and close/quit behaviors remain green in the complete focused suites.

## Independent commands and exact results

Every generated artifact is under
`/mnt/d/QindaQt/builds/terminal-s0-review-church`; each redirected configure
used `-DCMAKE_AUTOMOC_PATH_PREFIX=ON` and the private extracted qtermwidget
2.4.0 prefix.

- Strict Debug configure: exit 0; Release configure: exit 0.
- Debug and Release builds of `qindaqt_terminal_support`,
  `qindaqt_terminal_adapter`, `qindaqt-terminal`, and all five C++ tests: exit
  0 in both.
- Genuinely display-less registered selector (four display/runtime/platform
  variables removed): Debug **8/8**, exit 0; Release **8/8**, exit 0.
- Direct focused binaries in each configuration: launch policy 14/14, PTY
  bridge 8/8, session 17/17, appearance 7/7, window 14/14 — **60/60** per
  configuration, exit 0.
- Parent byte-count negative control: 2 passed/1 failed, exit 1 at the hostile
  multibyte diagnostic.
- Parent close-event negative control: 2 passed/1 failed, exit 1 at the
  replacement-generation state.
- Parent PTY-pump negative control: 2 passed/1 failed, exit 1 at read-side
  quiescence.
- Parent CTest-registration negative control with `XDG_RUNTIME_DIR` also
  removed: exactly four helper rows abort and the explicit window row passes,
  **1/5**, exit 8.
- `git diff --check`: exit 0.
- `tools/check-source-shape`: exit 0, 1030 files; largest owned test is 497
  nonblank lines and no production file crosses the review threshold.
- `tools/validate-docs`: exit 0, 66 documents/navigation.
- strict MkDocs to the external site directory: exit 0.
- Runtime link check: only `qindaqt-terminal` needs `libqtermwidget6`; all five
  focused binaries are confined from it.
- Final SHA/tree/parent repeat exactly; candidate and disposable control trees
  are clean; no review test/build process remains.

## Provenance correction

Tomas Reed's handoff labels `eea0f078…` as a sorted name-status hash. That
value is actually the sorted **path-only** hash. The sorted name-status hash is
`b32f42449a10abcd3a20f72a5c7dc5a8d8d6e0d35f873d309440ff0afcd2e474`.
The exact commit/tree/parent and 13-path set all reproduce, so this correction
does not change or disqualify the candidate.

## Mandatory remaining lane and next action

Real-adapter live qualification is still absent and explicitly **not waived**:
run one serialized, isolated private display lane against this exact source
covering live shell UTF-8/control-byte rendering, keyboard→child flow,
resize/SIGWINCH, Select All/copy extraction on empty and populated buffers,
real normal/signal exit publication, bounded teardown, first frame, and PSS.
It must keep host input/configuration untouched and record exact qtermwidget
2.4.x provenance.

The Program Manager should route that lane before integration. If it passes,
update the integration wiki/task evidence so the current paragraph distinguishes
the now-qualified strict builds from the still-bounded later live scope, then
rehearse the 13-path candidate onto current public main. If it finds a source
defect, route the exact reproduction to Tomas Reed for one non-amended
descendant and return it to Dijkstra for rereview.
