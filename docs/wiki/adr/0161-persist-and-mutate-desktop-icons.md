# ADR-0161: Persist and mutate desktop icons behind owned boundaries

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** Shell desktop surface, File Manager boundary
- **Supersedes:** None
- **Superseded by:** None

## Context

The first desktop-icons slice listed and opened real Desktop-directory entries,
but laid every tile out in an immutable flow. It offered no per-item menu or
rename path. Background menus were also assigned `Popup.x` and `Popup.y`; a
Wayland `Popup.Window` is positioned from its parent item's xdg anchor instead,
so those menus appeared at compositor-chosen corners rather than beneath the
pointer.

Free icon placement introduces durable state, while rename introduces
filesystem mutation authority. Neither belongs in the same presentation
object: geometry is shell preference state, and file mutation policy already
belongs to File Manager.

## Decision

Every window-backed desktop popup is parented to a one-pixel anchor item. The
desktop surface moves that anchor to the requested pointer coordinate, clamps
the popup inside the output, then opens it. Background context, Applications,
and per-icon menus all use this contract.

`DesktopIconLayoutStore` persists a versioned, bounded JSON document at
`$XDG_DATA_HOME/qindaqt/desktop-icon-layout.json` using `QSaveFile`. Positions
are partitioned by output name and keyed by the listed entry's device/inode
identity, not its path, so rename retains placement. Input is limited to 32
outputs, 4096 icons per output, bounded keys, and finite bounded coordinates;
malformed documents fail closed to default column placement. **Arrange Icons**
clears only the current output's saved positions.

The icon view owns drag presentation, item selection, and its **Open** and
**Rename…** context menu. Rename is dispatched through
`FileBoundary::createLocalMutationController`, with the complete device,
inode, size, modification-time, and mode identity captured by the last
Desktop listing. The mutation remains asynchronous and refreshes the listing
only after commit. QML receives no direct filesystem authority.

## Consequences

- Icons can be dragged anywhere within their output and retain that position
  across shell restarts and renames.
- Moving a file to a different filesystem identity intentionally returns it to
  default placement; stale positions cannot authorize or identify a mutation.
- A failed persistence write leaves the running layout usable but cannot be
  claimed durable. A malformed saved file is ignored rather than partially
  trusted.
- File Manager remains the sole owner of rename validation, identity fencing,
  worker lifetime, and filesystem execution.

## Revisit when

Output identity becomes more durable than the compositor's output name, or a
user-facing layout migration/import feature requires a richer schema.
