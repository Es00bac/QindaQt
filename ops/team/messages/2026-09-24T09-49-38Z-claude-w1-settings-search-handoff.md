# claude-w1-settings-search handoff

2026-09-24T09:49:38Z

Candidate for plan W1 (Ctrl+K Settings search) on
`worker/claude-w1-settings-search-20260923`, base `2e415cad`. ADR-0257.

- Registry: `SettingsRoute` gains bounded optional `keywords` and
  `destinations`, validated. `settings_route_search_metadata.cpp` adds
  keywords for all 21 routes and Input's five destinations. The `routes`
  projection carries both fields. `selectRouteDestination` re-delivers a
  repeated request to the open route.
- Shell: `SettingsCommandPalette.qml` wraps `Tk.CommandPalette` with the
  `QindaQtTheme` bridge. `SettingsSearchCommands.js` builds and ranks the
  commands. Ctrl+K is in `SettingsRouteShortcuts.qml`, which Alt+Left also
  moved into. The sidebar and compact header each have a Search button. The
  host Escape shortcut is disabled while the palette is visible.
  Unavailable routes are listed with their reason and are not selected.
- Input page: `onInitialDestinationChanged` (10 lines). Without it a
  destination could not open while Input is already open.
- Gates: full `cmake --build build/dev` exit 0; `ctest -L settings`
  135/135 passed; `./tools/validate-docs` exit 0. check-source-shape: the
  only change from base is Main.qml going from 377 to 384 non-blank lines
  (already over the 350 limit at base).

Requested next action: independent review of the exact commit.
