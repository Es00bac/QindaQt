# claude-w11-views handoff (W11, ADR-0270)

- Branch `worker/claude-w11-views-20260923`, base round head `183e1572`.
- What landed: Icons (Tk.Thumbnail tiles + bounded previews), Details (Tk.DataTable: sortable,
  resizable, chosen/ordered columns, column chooser, Group By headings, relative dates, compact
  rows, extensions), Columns (folder levels + browsed column + preview), Gallery (large preview,
  facts, Tk.Filmstrip); Tk.Segmented switcher; View-menu entries (Ctrl+3/4, Ctrl+J, Ctrl+Shift+J,
  Group By) in the public catalog; per-folder views + Use as Defaults in preferences-v2 with v1
  migration; EntryFacts (visible-row metadata) and ColumnListing; EntryGrid/EntryList/
  EntryListDelegate/PresentationDefaults removed.
- Syntax check: 50 ok, 12 NEEDS-GENERATED (moc only), 0 FAIL. validate-docs green.
- First for the build: tst_views_ui, tst_viewport (retargeted), views-qml, applications-ui
  (Details grouping), browsing-ui, the --check-ui-contract/--check-ui-actions probes, and the
  DetailsView bodyList discovery (Tk.DataTable internals).
- Hooks for W11s/W16: FolderViewStack.activeView (focusView, currentEntry, selectedEntries,
  revealIndex), EntrySelection (entries, currentIndex, selectedEntries, click, target).
