# Dijkstra the 2nd — Terminal rereview P2/P3 and static evidence

- Time: 2026-08-28T17:04:36Z
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Current finding counts: P0/P1/P2/P3 = `0/2/3/4`
- Product edits: none

## P2 — exited action and actual selection truth are still false

`TerminalWindow::updateViewActionStates()` defines `viewLive` as a retained
widget excluding only ShuttingDown, then uses it for paste as well as retained
scrollback operations (`src/apps/terminal/ui/terminal_window.cpp:251-265`). A
normal reap deliberately changes to Exited without disposing the backend
(`src/apps/terminal/session/terminal_session.cpp:269-288`), so paste and primary
paste remain enabled with no child. The owning wiki says paste deactivates when
no generation is live (`docs/wiki/apps/terminal.md:111-117`). The action test
ends at Running→ShuttingDown and never proves Running→Exited
(`tests/apps/terminal/tst_terminal_window.cpp:397-442`).

The adapter has a real `hasSelectedText()` query, but Select All ignores it and
unconditionally emits `selectionChanged(true)` after selecting the buffer
(`src/apps/terminal/ui/terminal_widget_adapter.cpp:412-441`). A blank buffer can
therefore enable Copy with empty selected text. Gate paste on Running, keep
retained scrollback operations available, publish the actual selection result,
and cover both transitions.

## P2 — four registered Qt Widgets rows are not headless-safe

Every helper-created Terminal test links `qindaqt_terminal_support`, whose
PUBLIC dependency includes `Qt6::Widgets`
(`src/apps/terminal/CMakeLists.txt:33-46`; `tests/apps/terminal/CMakeLists.txt:3-10`).
All five C++ rows use `QTEST_MAIN`; Qt 6 selects `QApplication` whenever
`QT_WIDGETS_LIB` is defined (`/usr/include/qt6/QtTest/qtest.h:281-300`). Only
`qindaqt.terminal-window-offscreen` receives `QT_QPA_PLATFORM=offscreen`
(`tests/apps/terminal/CMakeLists.txt:37-49`). Launch-policy, PTY-bridge, session,
and appearance therefore try the default platform in display-less CI. The CI
offscreen environment is scoped to preview smoke, not the following general
CTest step (`.github/workflows/ci.yml:95-103`). Add explicit offscreen
environment to every Widgets/GUI-linked row (or make the support/test boundary
truly Core-only) and prove the selector with display variables absent.

## P2 — the widget-side PTY line discipline transforms output again

The bridge reads the child's already line-disciplined output from its master,
then the adapter writes those bytes into qtermwidget's teletype slave
(`src/apps/terminal/ui/terminal_widget_adapter.cpp:150-168,354-392`). The
locally preserved pinned 2.4.0 source identifies version `2.4.0`
(`/tmp/opencode/qtw-src/toplevel.txt:24-28`), creates its PTY with
`openpty(..., 0, 0)` (`kpty.cpp:228-237`), and `runEmptyPTY()`/
`setEmptyPTYProperties()` changes IXON/IXOFF, IUTF8, and VERASE only
(`Session.cpp:341-353`; `Pty.cpp:255-275`). It does not clear default output
processing. Thus Linux `OPOST`/`ONLCR` can transform an intentional bare LF to
CRLF on the second PTY. The bridge test ends at an abstract sink and cannot
observe this (`tests/apps/terminal/tst_pty_bridge.cpp:91-138`). Make only the
widget transport byte-transparent with fail-closed `tcgetattr`/`tcsetattr`, and
exercise exact control bytes through the real adapter.

## P3 — four lower-severity contracts remain open

1. Program and working-directory diagnostics promise byte ceilings but compare
   `QString::size()` UTF-16 code-unit counts
   (`terminal_launch_policy.cpp:115-120,191-197`). Multibyte filesystem values
   can exceed the claimed byte limit; argument/environment bounds share the
   same measurement pattern.
2. `closeChildDescriptors()` falls back only for `ENOSYS`/`EINVAL`; errors such
   as sandbox `EPERM` return immediately and leak inherited descriptors into
   exec (`terminal_widget_adapter.cpp:77-89`).
3. The per-process scheme name still opens a predictable
   `qindaqt-terminal-scheme-<pid>-<counter>.ini.tmp` with Truncate before rename
   (`terminal_widget_adapter.cpp:217-243`). A pre-created temp symlink redirects
   truncation, and a crash-stale target after PID reuse makes `QFile::rename`
   fail rather than atomically replace. Use secure exclusive temporary-file
   creation and a replace-safe install contract.
4. Accepted ADR-0040 says the slave is opened after `setsid`/`TIOCSCTTY`, and
   the source comment says open follows `setsid`
   (`docs/wiki/adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md:32-34`;
   adapter `:99-100`), but implementation opens at line 101 and calls `setsid`
   at line 106. Make the conventional setup order and durable wording agree.

## Independent gates and runtime boundary

- `git diff --check 2386e746..9bd54448`: exit 0.
- `tools/check-source-shape`: exit 0; 1030 files; largest Terminal production
  source is the adapter at 413 nonblank lines.
- `tools/validate-docs`: exit 0; 66 Markdown documents/navigation.
- `/tmp/opencode/mkdocs-venv/bin/mkdocs build --strict --site-dir <fresh-/tmp>`:
  exit 0; documentation built in 0.67 seconds.
- Exact SHA/tree/parent and clean detached worktree rechecked after gates.

Live process inspection found two serial `cmake --build`/Ninja processes and a
concurrent Gemini Terminal exact reviewer; qtermwidget6 is absent from this
host and there is no configured build in this worktree. I did not enter the
serialized registered/private PTY lane, start a GUI/session, or touch host
display/input/configuration. The source verdict is already blocking; final
repaired-prior dispositions and exact handoff follow.
