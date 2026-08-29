# Astra Quill — Terminal S0 exact rereview FAIL

- Time: 2026-08-28T11:05:00-06:00
- Reviewer: Astra Quill (Google Antigravity Vertex ADC; Gemini 3.1 Pro (Low))
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Tree: `87ed4cec98b1d8faf1a170514c29917286da108d`
- Verdict: **FAIL — P0/P1/P2/P3 = 0/2/3/4**

I performed an independent review as a second opinion, confining all execution to the out-of-tree build root (`/home/cabewse/work_SPaC3/container-wm-private-agent-runs/astra-terminal-review/build`). I independently verified the source shape, whitespace, docs validation, and strict MkDocs. CMake configuration failed on the missing `qtermwidget6` dependency, so no executable/PTY tests were run.

## Independent Findings

### P1-1: Restart-Close race condition spawns an orphan
In `TerminalWindow::closeEvent`, if the session is in `ShuttingDown` (e.g. from an ongoing restart), the function sets `m_quitRequested = true`, accepts the event, and returns. It fails to call `m_session->beginShutdown()` which would have canceled `m_restartAfterShutdown`. Consequently, `completeShutdown` spawns a new replacement child just before `closeShutdownFinished` quits the application, leaking a spawned process.

### P1-2: EIO hot loop on child exit (100% CPU)
When the shell child exits, `TerminalPtyBridge::pumpMasterToSink` observes `errno == EIO` and returns without disabling `m_readNotifier`. The PTY master remains open, so the event loop immediately reactivates the notifier in an infinite hot loop.

### P2-1: Paste operations incorrectly enabled in Exited state
`TerminalWindow::updateViewActionStates` enables paste actions whenever `viewLive` is true. Since the `Exited` state retains the widget for scrollback, `viewLive` remains true and `m_pasteAction` remains enabled without a child process to receive the input.

### P2-2: Missing headless CI support
Tests linking `qindaqt_terminal_support` initialize `QApplication`, which aborts without a display server. The tests lack `QT_QPA_PLATFORM=offscreen` (only applied to the window test), causing the other tests to fail before they even start in CI environments.

### P2-3: Double line-discipline translation
Child PTY output (already line-disciplined, e.g., LF to CRLF) is pumped into qtermwidget's PTY. Because qtermwidget opens its PTY using default termios flags (which include OPOST/ONLCR), the output is processed a second time, mutating exact byte sequences.

### P3 Notes
1. **Path-byte bounds**: `resolveShell` uses `QString::size()` (UTF-16) instead of UTF-8 byte bounds for program limits.
2. **Descriptor-fallback bypass**: `close_range` returning an error (e.g., EPERM) avoids the fallback logic, leaking descriptors.
3. **Temporary-file**: Truncation-based `pid-counter.tmp` remains vulnerable to pre-existing symlinks or PID reuse collisions.
4. **Documentation divergence**: The source comment for ADR-0040 claims setsid before opening the slave, but implementation does it after.

## Evidence & Executed Commands
- Repository static gates: `git diff --check HEAD~1` (0), `tools/check-source-shape` (0, 1030 files), `tools/validate-docs` (0, 66 documents).
- MkDocs: `python3 -m venv ... && mkdocs build --strict` (0, successful out of tree).
- CMake: Failed configuring due to missing `qtermwidget6 2.4...<2.5`. No tests executed.

## Requested Next Action
Manager: DO NOT integrate. Return a non-amended descendant of `2386e74` repairing these exact findings to Micah Stone. After a source PASS, provision qtermwidget 2.4.x before rerunning the evaluation.
