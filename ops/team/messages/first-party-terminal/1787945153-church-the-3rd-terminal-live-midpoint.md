# Church the 3rd — real-adapter live midpoint and blank-selection defect

- Time: 2026-08-28T19:25:53Z
- Exact candidate: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Tree: `563a0793b1736238f8d59a54de81e022b0989c1a`
- Private compositor: Weston 15.0.1 headless/Pixman, private socket
  `qindaqt-terminal-church3`, no host `DISPLAY`, direct Qt events only
- Rendering dependency: extracted qtermwidget 2.4.0
- Product edits: none

The second complete harness pass has one material candidate defect already
reproduced: a never-started, otherwise blank real `TerminalWidgetAdapter`
receives Select All and `QTermWidget::selectedText(false)` returns exactly one
LF byte (`0a`). Candidate `hasSelectedText()` tests only `!isEmpty()`, so it
returns true and the presentation would enable Copy for an empty buffer. This
contradicts `docs/wiki/apps/terminal.md`'s explicit empty-buffer action-truth
contract. The fake window/backend rows did not expose upstream qtermwidget's
blank-selection representation.

The same exact run passed real Wayland window exposure, first-frame capture,
real keyboard→child delivery, UTF-8 snowman and ANSI rendering, populated
selection/copy, single-line clipboard paste, normal exit 7, SIGTERM exit
truth, generation replacement, restart→close cancellation, bounded teardown
of a child ignoring HUP/TERM, and final reap. Two apparent harness failures are
being isolated rather than attributed to the product: the bash-script WINCH
trap may defer the trap while its builtin `read` waits, and the first Qt PSS
reader used `QFile::atEnd()` on a zero-sized procfs pseudo-file. I am replacing
only those external probes and will return a final exact PASS/FAIL with full
counts, hashes, screenshots, and clean-source evidence.
