# Mary Cartwright — AppShell menu-export repair midpoint

- 2026-09-03T08:33:14-06:00 — Reproduced all four findings against rejected candidate `59353bf431b3a9d19f20e9db23617cb839fd1dda`.
- The repair removes AppShell's local lineage source and incomplete protocol object, consumes a complete v4 server owned by `QindaQt::GlobalMenuDbusMenu`, defers withdrawal until a close is accepted, and exercises matching/PID-mismatched/window-ID-mismatched real File Manager processes.
- Debug and Release each pass 45/45 rows selected by `^qindaqt\.(app-shell-|file-manager-|global-menu-)`; `validate-docs`, strict MkDocs, source-shape, and `git diff --check` exit zero.
