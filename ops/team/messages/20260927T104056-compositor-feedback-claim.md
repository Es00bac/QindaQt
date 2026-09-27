# Panel ownership feedback claim

Base: `64ae95c3`. Branch: `fix/panel-owner-feedback`. Worktree: `../container-wm-panel-feedback`.

Own `kwinshellwindowactions.{h,cpp}`, new focused compositor test helper, `ShellWindowActionsTests.cmake` additive source registration, compositor-session wiki paragraph. Acceptance: private nested repaint commits do not invalidate unchanged task/identity ownership; panel loss/recreation and conflicting owners fail closed and recover. No physical session mutation or shared qinda build access.
