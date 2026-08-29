# Church the 3rd — exact Terminal real-adapter live FAIL

- Time: 2026-08-28T19:32:11Z
- Reviewer: Church the 3rd (OpenAI collaboration runtime; exact serving model
  and reasoning unexposed)
- Exact candidate: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Tree: `563a0793b1736238f8d59a54de81e022b0989c1a`
- Parent: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Verdict: **FAIL — P0/P1/P2/P3 = `0/1/1/0`**
- Product edits: none

The mandatory live lane finds two independent candidate defects. The final
immutable run records 37 passing assertions and three failed assertions: one
for the P2 selection defect and two visual assertions (first and populated
frames) for the same P1 theme defect. Source/build verdicts remain Dijkstra's
domain; this verdict does not reopen any of his already-passed findings.

## P1 — the real terminal surface ignores the selected Qinda theme

The harness loads the candidate's actual `data/themes/qinda-dark.json` through
`ThemeLoader` and `TerminalAppearanceAdapter`, exactly as `main.cpp` does. Its
derived terminal background is `#171a18`. Under the private Weston Wayland
display, both an unselected first frame and an unselected populated frame show
the qtermwidget surface center as `#ffffff`; the surrounding menu/status
palette is dark. The retained PNGs make the mismatch independently visible.

The suspect production path is
`src/apps/terminal/ui/terminal_widget_adapter.cpp:252-295`: it writes the
Konsole-format document to a cache pathname and passes that pathname to
`QTermWidget::setColorScheme()` at line 287, but the live widget keeps its
white default. This verdict does not prescribe the repair; Tomas must verify
the upstream lookup/application contract and add a real-adapter color
regression rather than merely changing the call until this harness looks dark.
The documented requirement is `docs/wiki/apps/terminal.md:137-149`.

## P2 — blank Select All falsely enables Copy

A newly constructed, never-started real `TerminalWidgetAdapter` receives
`selectAllInView()`. qtermwidget 2.4.0 then returns exactly one LF byte from
`selectedText(false)` (`blank_selection_utf8_hex=0a`, length 1). Candidate
`hasSelectedText()` at
`src/apps/terminal/ui/terminal_widget_adapter.cpp:500-506` tests only
`!selection.isEmpty()`, returns true, and line 491 publishes the false
selection availability. That contradicts the explicit action-truth contract
at `docs/wiki/apps/terminal.md:126-130`: an empty buffer must never enable
Copy. A fake backend cannot reproduce qtermwidget's blank-grid encoding, so a
real-adapter regression is required.

## Passing private live evidence

The generated qualifier links the candidate's already-reviewed static
`qindaqt_terminal_adapter`, `qindaqt_terminal_support`, QST, and theme
libraries. It dynamically resolves only the extracted qtermwidget 2.4.0
library at
`/mnt/d/QindaQt/builds/terminal-s0-review-church/deps/qtermwidget-prefix/lib/libqtermwidget6.so.2`
(real file SHA-256
`b1440218096965e6161d67fab56d5f4ef6da869ad02cdb8999e98aa95a990dd1`).

The final run passes these 37 assertions:

- private Qt Wayland platform/socket and absent host `DISPLAY`;
- real adapter child start, Weston exposure in 14 ms, and captured first
  frame;
- fixture output, direct Qt keyboard→child input, UTF-8 snowman and ANSI
  control rendering;
- real resize from 99x24 to 139x34, child-observed SIGWINCH, and child-read
  updated PTY geometry;
- populated selection, exact private clipboard copy, and single-line
  production paste back to the child;
- normal exit code 7 and SIGTERM exit truth with both children reaped;
- two successful restarts and three distinct adapter generations;
- Restart followed by real Window Close cancels the replacement, emits one
  close completion, reaches `ShutdownComplete`, and escalates a third child
  that ignores HUP/TERM through SIGKILL before reaping it;
- PSS from `/proc/*/smaps_rollup`: application 71,500 KiB plus child 300 KiB,
  aggregate **71,800 KiB**, below the 1,024 MiB ceiling;
- lifecycle signal observation, final child/process cleanup, and two captured
  rendered sizes (800x500 and 1120x680).

## Commands, containment, and retained artifacts

Generated source/build/evidence lives only under
`/mnt/d/QindaQt/builds/terminal-s0-live-church3`. The strict harness configure
and build were:

```sh
cmake -S /mnt/d/QindaQt/builds/terminal-s0-live-church3/harness-src \
  -B /mnt/d/QindaQt/builds/terminal-s0-live-church3/harness-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/mnt/d/QindaQt/builds/terminal-s0-review-church/deps/qtermwidget-prefix \
  -DCMAKE_AUTOMOC_PATH_PREFIX=ON
cmake --build /mnt/d/QindaQt/builds/terminal-s0-live-church3/harness-build \
  --target terminal_live_qualifier terminal_live_fixture --parallel 2
```

The immutable run started Weston 15.0.1 with headless/Pixman, fake private
seat, 1280x720 output, no configuration, and socket
`qindaqt-terminal-church3` under mode-0700
`/run/user/1000/qindaqt-terminal-church3-runtime`. The qualifier had
`QT_QPA_PLATFORM=wayland`, no `DISPLAY`, a disabled session bus, private
XDG cache/config/data, and direct in-process Qt key/pointer events only. No
`uinput`, `dotool`, host compositor, host socket, host cursor, or host-global
input was used.

Retained exact artifacts and SHA-256 values:

- `evidence/live-run-immutable-final.log`:
  `17ca446758e9e3dd218823665b365174ae58a1f43634628e5e17b103ae139791`
- `evidence/first-frame-unselected.png` (800x500):
  `dc14f67929634fdbd09b4d0f61fc50533eb0fa11a5f3a9efd7c8e4d31c163401`
- `evidence/populated-frame-unselected.png` (1120x680):
  `f8854e0d84b617880b96be7d68c394a868ed22f8baf92cf007cd072366fb55ba`
- qualifier binary:
  `736068bfe2c908422916cef34666bd03a4d4ea7fc0209283161473a26517cf26`
- direct signal/PTY fixture:
  `d70b0f857d18c0247b14e44e45b5ddfef10cd52f512c4f3fdf04486268483450`

Final `git status --porcelain`, `git diff --check`, and `git diff --exit-code`
are empty/exit 0. Commit/tree/parent repeat exactly. No qualifier, fixture,
Weston, or candidate child process remains and the private runtime directory
is empty.

## Required next action

Do not integrate `bf195b6`. Route both exact reproductions to Tomas Reed for
one non-amended repair descendant with real-qtermwidget regressions. Return
that immutable descendant to Dijkstra the 2nd for focused source/build
rereview, then to Church the 3rd for this same private live harness. Preserve
the existing candidate and every generated artifact.
