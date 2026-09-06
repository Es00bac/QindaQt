# Status contrast review repair

- Exact repaired candidate: `4a9f74ca` (descendant of `f7ea8f6d`), worktree `/home/cabewse/work_SPaC3/container-wm/.cache/status-icon-contrast`.
- Fixed selected bundled-wallpaper label to use `accent.fg` when its button uses the emphasized accent fill, with muted disabled and default ordinary branches.
- Audited every shell QML file containing both `ShellIcons.Icon` and `fg.disabled`; all actual symbolic icon call sites now use opaque `fg.muted`. Non-symbolic application artwork and ordinary disabled text retain their existing semantics.
- Added the opaque symbolic-color consumer contract to shell iconography.
- Gates: `git diff --check` PASS; `tools/check-source-shape` PASS with pre-existing warnings; `mkdocs build --strict` PASS; `tools/validate-docs` PASS (172 docs).
- Requested action: re-review exact 4a9f74ca, integrate, compile focused design-token/controls/Settings/shell QML tests, and run light/dark visual contrast check.
