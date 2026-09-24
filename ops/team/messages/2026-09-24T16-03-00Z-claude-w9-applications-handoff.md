# claude-w9-applications handoff

2026-09-24T16:03:00Z

Candidate for plan W9 (Applications like macOS) on
`worker/claude-w9-applications-20260923`, base `32deef08`. The exact commit
is the branch head that contains this message.

What landed (ADR-0262, superseding ADR-0164's drill-down presentation only):

- Applications is the virtual location `applications:`, browsed by the ordinary
  NavigationController through lister/launcher decorators, in the normal Icon
  and Details views. It sorts A to Z, shows app icons at the current zoom, and
  the Kind column becomes Category. Selection, rubber-band, type-to-select,
  filter and zoom are unchanged.
- View > Group by Category (Ctrl+G) is the category sort, with section
  headings in Details. Applications keeps its own sort
  (`ApplicationsPlaceOrder`), so grouping never leaks into folders.
- A single click selects. Double-click, Enter, File > Open (Ctrl+O) and
  context Open all open through NavigationController.activate(), which asks
  the compositor first (docked replacement, ADR-0172) and then spawns
  locally. Chooser mode (ADR-0165) opens straight into the place and hands
  every choice to the compositor.
- Item menu: Open, Get Info (ApplicationInfoDialog), Show Desktop Entry File
  (Ctrl+Shift+E). File actions, drags and drops are off in the place.
- Rows that can't start from a plain window are dimmed. The reason is also
  shown in a tooltip, the accessible description, Get Info and the launch
  banner.
- The behaviour lives in the model/controller layer and feeds the standard
  views, so W11 inherits it.

Left out because no public boundary exists (see ADR-0262 §7): Keep in Dock
(waits on W13's dock-item store), Add to Desktop, Open in New Workspace, sort
by recently used, drag to dock/desktop, and dropping files on an app (W10).

Gates are listed in the commit body. Next action: manager integration.
