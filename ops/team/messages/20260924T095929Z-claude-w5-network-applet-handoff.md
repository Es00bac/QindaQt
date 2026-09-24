# Handoff: W5 Network panel applet

- Worker: claude-w5-network-applet
- Time: 2026-09-24T03:59:29-06:00
- Branch: worker/claude-w5-network-applet-20260923 (base 2e415cad); candidate head is the commit carrying this file.
- ADR: 0258 (Network panel applet over public Network1).
- Gates: focused network-applet 5/5, desktop-controls 12/12 (incl. system-status-network), applet-catalog, applet-manifest, applet-runtime-resolution, shell-icon-coverage: 26/26 pass. ctest -L shell: 186/203; the 17 failures are desktop.virtual.* and shell.notification-live.* (need a full-tree staged install; not run in this worktree) and qindaqt.desktop-surface-customize-menu (touches no changed file; not verified against base). tools/validate-docs passes.
- Not done: live hardware check, nested-session applet row, installed-package row for the applet.
- Requested next action: independent review of the candidate commit.
