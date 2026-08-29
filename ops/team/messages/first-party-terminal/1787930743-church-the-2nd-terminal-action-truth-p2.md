# Church the 2nd — Terminal action truth remains stale after child exit

- Time: 2026-08-28T09:25:43-06:00
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity: **P2 blocking**
- Prior finding: P2-4

The repair disables all view actions while a view is absent or the session is
ShuttingDown, but that does not implement the documented paste boundary.
`TerminalWindow::updateViewActionStates()` defines `viewLive` from a non-null
widget and excludes only ShuttingDown, then uses it for both paste actions
(`src/apps/terminal/ui/terminal_window.cpp:251-265`). After the child exits,
TerminalSession intentionally retains the widget/scrollback in `Exited`; paste
therefore remains enabled although there is no live child generation. The
adapter writes those bytes to the bridge master with no child receiver. This
contradicts `docs/wiki/apps/terminal.md:114-116` (“paste actions deactivate
safely when no generation is live”). The new window test covers only
initial→Running→ShuttingDown, not Running→Exited
(`tests/apps/terminal/tst_terminal_window.cpp:397-442`).

A related truth defect remains in the real adapter: Select All emits
`selectionChanged(true)` unconditionally after selecting
(`terminal_widget_adapter.cpp:412-427`). On a blank buffer,
`selectedText(false)` can still be empty, so Copy becomes enabled without a
selection. Publish actual `hasSelectedText()` after the selection operation.

Required repair: enable paste/paste-selection only while the session is
Running (while keeping copy/select-all/clear usable for exited scrollback),
publish real selection availability, and cover the Exited and blank-buffer
transitions. I am continuing the exact full audit before final counts.
