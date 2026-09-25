# claude-w11s-fm-styles-and-tabs handoff (W11s + tabs + W20 placeholder, ADR-0271)

- Branch `worker/claude-w11s-fm-styles-20260923`, base round head `7af46599`, round head
  `ab3c5505` (W16) merged.
- What landed: Preferences ▸ General ▸ File manager style (match the desktop layout, Finder,
  Explorer, Commander) in preferences-v2 (`fileManagerStyle`, `layoutStyle`); picking a style
  writes its starting view (Explorer/Commander Details). The selected layout's
  `workflow.fileManager` (Settings1 `panels.layoutProfile` + catalog, `runtime/layout_style_hint`)
  is the style until the user picks one. Every tab has its own NavigationController
  (`runtime/folder_navigations`; the five action binders take an optional context object and
  follow the active tab). FolderPanes/FolderPane/FolderTab: one pane or Commander's two, each with
  tabs (Ctrl+T, Ctrl+W, Ctrl+Tab, Ctrl+Shift+Tab, drop onto a tab). Explorer: FolderTree under the
  places, address-bar breadcrumb, CommandBar. Commander: Tab between panes, Copy To/Move To at the
  other pane, FunctionKeyBar F3 Quick Look … F8 Trash.
- Syntax check: 42 ok, 11 NEEDS-GENERATED (moc only), 0 FAIL; my five test files compile clean
  with the moc include stripped. validate-docs green.
- First for the build: `qindaqt.file-manager-styles-ui` (F5 winning over Refresh relies on
  Keys.onShortcutOverride propagating from the focused view to FolderPanes), `-folder-navigations`,
  `-layout-style-hint`, `-preferences-store` (key count now 17) and `-controller`, then the existing
  views-ui, everyday-ui, browsing-ui, file-actions-ui and the --check-ui-contract/--check-ui-actions
  probes (Main.qml now reaches views/selection through FolderPanes).
