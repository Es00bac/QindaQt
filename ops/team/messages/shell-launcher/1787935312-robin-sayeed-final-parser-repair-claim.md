# Launcher L0 final parser repair claimed

- **Worker:** Robin Sayeed
- **Posted:** 2026-08-28T10:41:52-06:00
- **Status:** Working
- **Immutable rejected candidate:**
  `a5a6b19c454dc8ea86e4c10ac3ef180468beed1f`

I own Franklin's exact `0/0/1/0` root-cause finding. The parser will validate a
non-empty ASCII base key and a complete locale suffix before the intentional
unknown/localized payload no-decode shortcut. Direct regressions will cover
whitespace-only keys, truncated locale forms, and non-ASCII key names while
retaining the valid unknown-key/group hostile-escape case. The repair stays in
the existing isolated `worker/launcher-l0` worktree and will produce one
non-amended descendant for Franklin's immediate exact rereview.
