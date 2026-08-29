# Dijkstra the 2nd — Terminal exact-candidate rereview FAIL

- Time: 2026-08-28T17:12:53Z
- Reviewer: Dijkstra the 2nd (OpenAI collaboration runtime; exact serving model
  and reasoning unexposed)
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Tree: `87ed4cec98b1d8faf1a170514c29917286da108d`
- Parent: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Exact repair manifest: 26 paths; sorted name-status SHA-256
  `9da026270d0080ec3bc1ac7f58e86f7661c4cc0b309ca4b1758e46361945589f`
- Verdict: **FAIL — P0/P1/P2/P3 = `0/3/3/4`**

This verdict applies only to immutable commit `9bd54448`. I independently
verified its exact detached clean worktree, read the complete repair and every
owning Terminal source/test/doc/build contract, compared all former `0/4/5/4`
and consolidated `0/2/3/4` findings, checked pinned qtermwidget 2.4.0 source,
and ran a fresh strict root configure plus the proportional safe compiled and
headless gates. Candidate source and Git state were never edited.

## P1-1 — the production adapter does not compile

With the audited repository package `qtermwidget 2.4.0-1` extracted into a
temporary prefix, strict Debug root configuration succeeds. The serial
production build fails in the generated adapter MOC translation unit:

```text
terminal_widget_adapter.h:46:61: error: cannot convert ‘QTermWidget*’ to
‘QWidget*’ in return
terminal_widget_adapter.h:12:7: note: class type ‘QTermWidget’ is incomplete
```

`terminal_widget_adapter.h` forward-declares `QTermWidget`, stores its pointer,
and defines `QWidget *terminalWidget()` inline (`:12,46,65`). A translation
unit that includes the header without private `qtermwidget.h` cannot know the
inheritance and cannot perform the derived-to-base conversion. Move the
override out of line into the adapter `.cpp` after the private header is
included, and make the strict adapter/executable build a mandatory gate.

## P1-2 — Restart then real window Close still starts a replacement child

The session method cancels a pending restart when `beginShutdown()` is called
in ShuttingDown (`terminal_session.cpp:128-134`). The real window's
ShuttingDown `closeEvent()` branch never calls it; it sets only quit intent and
returns (`terminal_window.cpp:383-387`). Clean completion therefore observes
`m_restartAfterShutdown=true`, calls `spawnGeneration()`, and only then emits
the signal that queues application quit (`terminal_session.cpp:204-225`). The
test calls the session method directly and never closes a window
(`tst_terminal_session.cpp:404-424`). Add a window-level Restart→Close
regression proving one backend/widget/child.

## P1-3 — normal Exited scrollback retains a hot EIO/HUP notifier

`pumpMasterToSink()` returns for `EIO`, EOF, and hard errors without disabling
its read notifier (`pty_bridge.cpp:155-177`). Normal reap stops polling but
retains the backend/widget for Exited scrollback
(`terminal_session.cpp:269-288`). An independent display-free kernel probe
closed the slave, then observed five consecutive immediate `POLLHUP` events and
five `EIO` reads from the still-open master. The enabled Qt read notifier can
therefore reactivate indefinitely. Disable it on terminal read conditions and
add a retained-Exited bounded event-loop liveness regression.

## P2 findings

1. **Exited action and actual selection truth are false.** A retained Exited
   widget satisfies `viewLive`, so paste actions stay enabled without a running
   child (`terminal_window.cpp:251-265`), contrary to `terminal.md:111-117`.
   The action test never reaches Exited. Select All also emits selection true
   unconditionally instead of publishing `hasSelectedText()`
   (`terminal_widget_adapter.cpp:412-441`), enabling Copy on a blank buffer.
2. **Four registered rows abort headless.** All helper-created tests PUBLIC-
   inherit Qt Widgets and use `QTEST_MAIN`, but only the window row has an
   offscreen CTest property (`src/apps/terminal/CMakeLists.txt:33-46`;
   `tests/apps/terminal/CMakeLists.txt:3-49`). With `DISPLAY`,
   `WAYLAND_DISPLAY`, and `QT_QPA_PLATFORM` absent, launch-policy, PTY-bridge,
   session, and appearance abort before their tests; the explicit-offscreen
   window row alone passes. CI's offscreen environment is scoped only to the
   preceding preview smoke (`.github/workflows/ci.yml:95-103`).
3. **The second PTY transforms output again.** The adapter writes already
   line-disciplined bridge-master bytes into qtermwidget's slave
   (`terminal_widget_adapter.cpp:150-168,354-392`). Pinned 2.4.0 uses
   `openpty(...,0,0)` and its empty-PTY setup changes input flags/erase only;
   it does not clear output processing. A display-free probe confirmed the
   default slave has `OPOST|ONLCR` and transforms `b'A\nB'` into
   `b'A\r\nB'` at the master. Make the widget transport byte-transparent with
   fail-closed termios handling and prove exact control bytes through the real
   adapter.

## P3 findings

1. Program/working-directory diagnostics promise bytes while testing UTF-16
   `QString::size()` (`terminal_launch_policy.cpp:115-120,191-197`); multibyte
   values can exceed the stated ceiling. Arguments/environment use the same
   measurement pattern.
2. `close_range` failures other than `ENOSYS`/`EINVAL` return without the
   bounded fallback (`terminal_widget_adapter.cpp:77-89`), so sandbox `EPERM`
   can leak inherited descriptors into exec.
3. Scheme creation opens a predictable `<pid>-<counter>.ini.tmp` with Truncate
   before rename (`terminal_widget_adapter.cpp:217-243`). A pre-created temp
   symlink redirects truncation, and a crash-stale target after PID reuse makes
   rename fail instead of replace safely.
4. Accepted ADR-0040 and the adjacent comment say the slave opens after
   `setsid`, while implementation opens at adapter line 101 and calls `setsid`
   at 106 (`ADR-0040:32-34`; adapter `:99-110`). Make durable wording and the
   conventional setup order agree.

## Prior-finding disposition

- The second application-owned PTY repairs the former fatal input direction:
  master writes reach child input, child output/echo returns at the bridge
  sink, and child stdio does not share the nonblocking master description.
  The direct bridge test passes 7/7, including winsize and close.
- Failed-escalation ownership is retained; start/restart/shutdown/close/quit are
  refused while a survivor remains. The fake-backed session/window rows pass.
- Select All's one-past row is repaired to the last valid zero-based row;
  availability remains incorrect on a blank buffer and real extraction could
  not run because the adapter does not compile.
- Locale authority now follows `LC_ALL→LC_CTYPE→LANG` independent of envp
  order, uses a strict codeset oracle, and rejects hostile substrings. The
  launch-policy test passes 13/13.
- argv/envp pointer arrays are prepared before fork; setup failures and signal-
  mask reset are handled. Descriptor fallback remains P3.
- qtermwidget is version constrained `2.4...<2.5`, linked PRIVATE, and included
  only by the adapter `.cpp`; the header's incomplete-type inline conversion is
  the new P1.
- `ECHILD` publishes typed UnknownExit, session status is truthful, and the
  session test passes 17/17.
- `isFile()` and per-instance scheme naming were added, but byte measurement
  and secure temp/replace semantics remain open. ADR-0030 is preserved as
  Superseded, ADR-0040 is Accepted, navigation is consistent, and source sizes
  stay below the production review threshold.

## Commands, counts, and exact exits

- Provenance/cleanliness: `git rev-parse HEAD HEAD^{tree} HEAD^`, sorted
  manifest hash, `git status --porcelain=v1`, final `git diff --check`: all
  exact/clean, exit 0.
- Temporary dependency: `pacman -Si qtermwidget` reports `2.4.0-1`; download,
  `pacman -Qp`, and extraction into
  `/tmp/dijkstra-terminal-gate.8l5adE/prefix`: exit 0; no system install.
- Strict root configure (Debug, tests ON, shell/production shell/KWin plugin/
  host uinput OFF, temporary prefix): exit 0.
- Serial production + five-test target build: exit 1 at adapter MOC incomplete-
  type conversion. Serial adapter-free five-test build: exit 0.
- Registered five-row run with all display/platform variables absent: CTest
  exit 8, **1/5 passed**; exactly four non-window rows aborted, explicit-
  offscreen window passed.
- Same five registered rows with global `QT_QPA_PLATFORM=offscreen`: exit 0,
  **5/5 passed**.
- Direct offscreen QtTest: launch-policy 13/13, PTY bridge 7/7, session 17/17,
  appearance 7/7, window 12/12 — **56/56**, exit 0.
- Full eight-row selector with offscreen and temporary library path: CTest exit
  8, **6/8 passed**. Desktop metadata joins the five C++ passes; CLI and
  installed-metadata fail because the production executable was not built.
- Kernel-only PTY probes with no display/session: exit 0; five consecutive
  `POLLHUP`+`EIO` observations after slave close, and default
  `OPOST=true/ONLCR=true` transformed LF to CRLF.
- `tools/check-source-shape`: exit 0, 1030 files; Terminal adapter 413 nonblank.
- `tools/validate-docs`: exit 0, 66 documents/navigation.
- `/tmp/opencode/mkdocs-venv/bin/mkdocs build --strict --site-dir <fresh-/tmp>`:
  exit 0, 0.67 s.

## Caveats and requested next action

No real-adapter live shell, keyboard→child, renderer control-byte/UTF-8,
resize/SIGWINCH, select/copy extraction, real signal exit, Restart→Close,
failed-escalation, first-frame, or PSS claim is possible while the production
adapter does not compile. No host display, desktop, compositor, input, session
bus, configuration, or hardware was touched. Generated evidence remains only
under the named `/tmp` root; no process remains and the serialized lane is
released.

Do not integrate `9bd54448`. Micah Stone should preserve it and return one
non-amended descendant repairing all `3 P1 / 3 P2 / 4 P3` findings, including
the strict production build and direct regressions. Return that exact commit to
Dijkstra the 2nd for rereview. After a source/compile PASS, run all eight
registered rows with display variables absent and then one serialized private
real-adapter PTY round trip covering the caveats above before integration.
