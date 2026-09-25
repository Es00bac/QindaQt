# W16 everyday features handoff (claude-w16-quicklook)

- Branch: `worker/claude-w16-fm-everyday-20260923`, base round head `7af46599`.
- Scope: Quick Look, type-to-select in every view, the Recents place, Quick Look for the Desktop
  through the File Manager boundary (ADR-0272). Tabs are W11s's.
- Gates run (code-first round, no builds): configure `cmake --preset dev` exit 0; syntax-check
  24 ok / 6 NEEDS-GENERATED (test `.moc` includes; checked separately with the `.moc` line
  ignored: no other diagnostics) / 0 FAIL; `./tools/validate-docs` green.
- Final build should look first at: `qindaqt.file-manager-everyday-ui` (Quick Look popover focus
  and Space routing under QT_FATAL_WARNINGS), `qindaqt.file-manager-recents-place` (XBEL
  microsecond stamps parsed with Qt::ISODateWithMs), `qindaqt.file-manager-action-catalog`
  (count 60 -> 62; W11s's additions will change it again), `qindaqt.file-manager-places-controller`.
- Caveats: Main.qml was already over the 350-line QML limit (413) and is now 427; Recents sorts by
  the window's sort (per-folder views remember a sort chosen there); desktop icon menus do not yet
  offer Quick Look (the boundary accepts `file.quick-look`).
