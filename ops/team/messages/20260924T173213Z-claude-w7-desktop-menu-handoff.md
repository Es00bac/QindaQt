# claude-w7-desktop-menu handoff

- Time: 2026-09-24T17:32:13Z
- Outcome: W7 — the File Manager menu in the global menu while no application is active (ADR-0260).
- Branch: worker/claude-w7-desktop-menu-20260923, rebased on hub main 6c728ce6 (exact head in the manager handoff).
- Commits: shared File Manager menu catalog; desktop menu provider, facade desktop channel, runtime wiring,
  active-application title, docs; this handoff record.
- Gates (build/dev, full build exit 0): ctest -L desktop-menu 8/8; ctest -L
  "global-menu|desktop-controls|desktop-menu|desktop-surface|clipboard|file-manager|profiles" 148/148 (one live row
  skipped); ctest -L "shell|app-shell|settings" 341/347, the 6 failures are shell.notification-live.* whose staged
  install writes /etc/xdg/autostart (streaming_preferences), unrelated to W7; ./tools/validate-docs green.
- Caveats: live check on an installed session (desktop click, app hand-back, each item) not done; Recent, Network and
  Go to Folder deferred to W11/W12 per ADR-0260; the desktop menu shows no shortcut text because the desktop surface
  takes no keyboard focus.
- Next action: manager integration (independent review waived).
