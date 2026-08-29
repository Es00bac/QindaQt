# Church the 2nd — Terminal S0 P1: Select All creates an out-of-bounds qtermwidget selection

- Time: 2026-08-28T08:48:17-06:00
- Owner: Church the 2nd
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Severity: P1; user-facing copy path can index beyond qtermwidget's screen

`TerminalWidgetAdapter::selectAllInView()` computes `rows` as
`historyLinesCount() + screenLinesCount()`, then calls
`setSelectionEnd(rows, screenColumnsCount())`
(`src/apps/terminal/ui/terminal_widget_adapter.cpp:329-339`). Those APIs expose
counts, but qtermwidget's selection coordinates are zero-based row/column
indices.

Pinned upstream qtermwidget 2.4.0 makes the failure concrete:

- the count accessors return history count, column count, and screen-line
  count, while `setSelectionEnd(row,column)` passes those coordinates directly
  to `Screen::setSelectionEnd()`
  ([qtermwidget.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/qtermwidget.cpp#L675-L698));
- `Screen::setSelectionEnd()` corrects only `x == columns` by decrementing the
  linear position; it does not clamp a one-past row
  ([Screen.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Screen.cpp#L1226-L1245));
- selection extraction iterates through that bottom row and indexes
  `screenLines[screenLine]`; row `history+screenCount` produces
  `screenLine == screenLines.count()`, an out-of-bounds access
  ([Screen.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Screen.cpp#L1300-L1326),
  [Screen.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Screen.cpp#L1390-L1412)).

Thus Select All followed by selected-text evaluation/copy can assert in a
Debug dependency or access beyond the container in Release. The window/session
tests route Select All only through a fake counter, and no registered row links
the real adapter. Repair should use the last valid row (with explicit empty/
minimum handling) while respecting upstream's one-past-column convention, and
an isolated real-adapter test must extract/copy the selection without an OOB.

No product/Git mutation, compiler, PTY, GUI, session, or host interaction was
performed. Full exact verdict continues.
