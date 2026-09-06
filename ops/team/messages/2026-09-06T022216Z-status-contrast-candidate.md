# Status and control contrast candidate

- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/status-icon-contrast`
- Exact base: `2981a2865f1302612e6cc1e14cc566d6e4b1d991`
- Exact candidate: `f7ea8f6d`
- Root cause: provider recoloring works (standalone glyphs in dc816c are white). The combined status summary used `status.warning.foreground` directly on a dark panel, while that role is only contrasted against `status.warning.background`; essential unavailable glyphs used exempt low-emphasis disabled color.
- Outcome: surface messages/glyphs use `fg.default`; unavailable summary glyphs use `fg.muted`; disabled Buttons use the `fg.muted/bg.raised` pair and suppress hover overlays; ThemeCard selected check uses surface foreground. Qinda macOS and Dusk muted colors now clear 4.5:1 on every standard background.
- Evidence: calculated five-theme muted ratios: Light min 4.55, Dark 5.92, Dusk 4.52, macOS 4.93, High Contrast 15.25. `tools/check-source-shape` exit 0 (pre-existing threshold warnings); `git diff --check` exit 0. Focused compiled/QML tests require manager integration build.
- Deliberately retained: ThemeCard preview-unavailable text remains `status.warning.foreground` because its actual parent is `status.warning.background`; DisplayPreviewBanner warning foreground likewise stays inside its warning fill.
- Requested action: independent exact-commit review, then integrated design-token/controls/Settings/shell focused tests and one visual status-bar check.
