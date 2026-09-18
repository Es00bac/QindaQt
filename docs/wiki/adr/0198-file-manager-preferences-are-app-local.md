# ADR-0198: File Manager preferences are app-local, exact, and every one of them does something

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** File Manager
- **Supersedes:** ADR-0090's deferral of sort column/direction, hidden
  visibility, view mode and icon zoom to a later Settings1 schema decision
- **Superseded by:** None

## Context

Sort order, hidden files, view mode and icon size were session-local. ADR-0090
deferred persisting them because it was unclear whether they belonged in
Settings1, the session-wide settings service. Three waves later the answer is
plain: they are one application's view preferences, nobody else reads them,
and Settings1 rejects a whole snapshot on one unknown key — a fragility a
per-app view setting should not have to share. Meanwhile the file manager
gained things that genuinely need a durable answer before a window opens:
whether to browse the network at all (ADR-0200), and which scheme the Connect
dialog starts on.

## Decision

**File Manager preferences are app-local**, in `preferences-v1.json` beside
`bookmarks-v1.json` and the saved-location inventory under
`$XDG_STATE_HOME/qindaqt-file-manager`, over the same symlink-refusing,
size-bounded, atomically committed `StateFile` primitive.

- **Every preference does something.** A knob with no effect is worse than an
  absent one, so a field may not be added without wiring it in the same
  change, and the focused rows assert the wiring rather than the round trip
  alone. The nine are: default view mode, show hidden, folders first, sort
  column, sort direction, icon size, discover nearby servers, default connect
  scheme, and confirm before Trash.
- **Every default is what the application already did.** A first run must
  behave exactly as it did before preferences existed —
  `NavigationController`'s own grid view, hidden files off, folders first,
  name ascending, 64 px icons, discovery off, sftp, confirm on.
- **Exact, not tolerant.** The reader demands an exact key set and refuses a
  document from a newer schema whole. A value outside its documented set is
  refused, **never snapped**: an icon size off the zoom ladder would put the
  view at a size the zoom controls can never return to. A refused document
  leaves the defaults standing rather than a half-understood mixture.
- **A refused write changes nothing visible.** What the Preferences window
  shows is always what the next launch will read.
- **The controller applies nothing.** `PreferencesController` knows nothing
  about `NavigationController` and vice versa; one small QML collaborator
  binds them, so a preference change reaches the window already on screen
  without either class depending on the other. That apply step is idempotent,
  because `setSortColumn()` *flips* the direction when called with the active
  column and a needless re-sort moves the view under the user's selection.
- **Only the recoverable Trash may skip its confirmation.** Empty Trash is
  permanent and always asks, whatever the preference says.

The window is a separate non-modal window rather than a modal dialog, opened
with the platform-standard `Ctrl+,`, because changing a default is something a
user does while looking at the folder it affects.

## Consequences

- The ADR-0090 deferral is closed: sort, hidden visibility, view mode and zoom
  now survive a restart, without a Settings1 schema change and without the
  whole-snapshot fragility that would come with one.
- Nothing about this is session-wide. A second QindaQt application wanting the
  same preference does not read this file; that would be a Settings1 decision
  with its own ADR.
- The action catalog gains `app.preferences` (Ctrl+,); its pinned size moves
  from 31 to 32.
- Focused rows: `qindaqt.file-manager-preferences-store`,
  `qindaqt.file-manager-preferences-controller`, and one row in
  `qindaqt.file-manager-network-hub-ui` that opens the window through the real
  action, clicks a real control, and proves the change reached both the folder
  on screen and the file on disk.

## Revisit when

A second QindaQt application needs to read one of these — that is the point at
which the setting stops being app-local and becomes a Settings1 key, with a
migration out of this file. Adding a preference means `preferences-v2` and a
migration, exactly as the saved-location inventory does.
