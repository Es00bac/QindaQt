# Tomas Reed — Terminal S0 repair midpoint (all blockers implemented, gates green)

- Time: 2026-08-28T18:49:30Z
- Worker: Tomas Reed (Z.AI via OpenCode, `zai-coding-plan/glm-5.3-flash`,
  reasoning high) — posted by the live process
- Base: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`, branch
  `worker/terminal-s0-repair-tomas`, tree clean except my owned Terminal paths
  (13 modified files, no commit yet)

## Implemented (all findings from Astra `20260828T110500`, Dijkstra
`1787936455`/`1787936676`/`1787936950`/`1787937173`, design per Maren
`1787940360`)

1. P1 Restart→Close: the ShuttingDown short-circuit in
   `TerminalWindow::closeEvent` is gone; every non-refused close reaches
   `TerminalSession::beginShutdown()`, which cancels the pending restart.
   `ShutdownFailed` refusal stays first. Guard comment names the invariant.
2. P1 PTY EIO/HUP hot loop: `TerminalPtyBridge::pumpMasterToSink()` treats
   EOF/`EIO`/hard errors as terminal — sets `m_childOutputClosed`, disables
   the read notifier, keeps the master (single-owner close and SIGHUP path).
   `isChildOutputClosed()` added as test/diagnostic observation only.
3. P1 adapter strict compile: `terminalWidget()` moved out of line; the four
   masked `-Werror` errors fixed (two `static_cast<size_t>` reserves, two
   non-const `QByteArray` iteration loops for argv/envp aliasing) with an
   AGENT-GUARD on the pointer-array contract.
4. P2 Exited action truth: paste/paste-selection now gate on `Running`;
   Select All/Clear/copy-with-selection remain available on a retained Exited
   view; `selectAllInView()` publishes the real `hasSelectedText()`.
5. P2 headless rows: `qindaqt_add_terminal_test` sets
   `QT_QPA_PLATFORM=offscreen` for every registered row (window duplicate
   removed); launch-policy/bridge/session/appearance/window all pass with
   display variables absent.
6. P2 double line discipline: `makeWidgetTransportByteTransparent()` clears
   `OPOST` on the adapter's teletype duplicate with fail-closed
   tcgetattr/tcsetattr plus re-read verification; start() refuses with a
   typed diagnostic when transparency cannot be proven.
7. P3-1 byte bounds: `utf8ByteLength()` (UTF-8) replaces `QString::size()`
   for program/argument/working-directory/environment-entry ceilings.
8. P3-2: `close_range` falls through to the bounded sweep on ANY error
   (EPERM included).
9. P3-3: scheme temp file uses `QIODevice::NewOnly` exclusive create
   (symlink-safe); install removes a crash-stale target before rename
   (replace-safe after PID reuse).
10. P3-4: child setup is now setsid → open slave → TIOCSCTTY, matching the
    accepted ADR-0040 wording.
11. Wiki `terminal.md` + ADR-0040 updated in the same change (quiescence,
    transparency, close-during-restart, Exited action truth, headless rows,
    byte ceilings).
12. New regressions: `slaveCloseQuiescesReadNotifierAndKeepsMaster` (bridge),
    `restartThenCloseSpawnsNothingBeforeQuit` +
    `exitedStateDisablesPasteAndKeepsScrollbackOps` (window),
    `byteBoundsRejectMultibyteValues` (launch policy); session-test comment
    no longer mislabels the session call as "the close path".

## Negative controls (rows fail on parent behavior, then restored)

- Parent `closeEvent` restored → window Restart→Close row FAILS
  (state Running=2, expected ShutdownComplete=4), exit 1.
- Parent `pumpMasterToSink` restored → bridge quiescence row FAILS
  (`isChildOutputClosed()` never set), exit 1.
- Parent `QString::size()` measurement restored → byte-bounds row FAILS
  (no "exceeds" diagnostic), exit 1.

## Gates so far (all exit 0)

- Strict Debug + Release builds of all eight Terminal targets against the
  pinned extracted `qtermwidget 2.4.0-1` prefix: exit 0.
- Registered selector headless (DISPLAY/WAYLAND_DISPLAY/QT_QPA_PLATFORM
  absent, private lib path only): 8/8 passed. Installed-metadata needs
  `LD_LIBRARY_PATH` to the private prefix for `libqtermwidget6.so.2` —
  environment, not display, dependence.
- Focused Debug + Release: launch 14/14, bridge 8/8, session 17/17,
  appearance 7/7, window 14/14 (60 rows total per build).
- `git diff --check` clean; `check-source-shape` exit 0 (1030 files; my
  largest, tst_terminal_window.cpp, 497 nonblank — under review threshold);
  `validate-docs` exit 0 (66 documents); strict MkDocs exit 0.

## Thread re-check

No new Dijkstra findings posted as of this message (latest is still
`1787937173`). I will re-read the thread immediately before the final
commit and incorporate any new durable finding.

Note: /tmp tmpfs filled up mid-build (shared with other lanes); my build
root moved to
`/home/cabewse/work_SPaC3/container-wm-private-agent-runs/tomas-terminal-gate/`.
Unrelated notification-presentation test link failures exist in the shared
tree build — not my ownership, not touched.

Next: full-diff self-review, final thread check, single non-amended commit,
handoff requesting Dijkstra's exact rereview.
