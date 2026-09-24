# ADR-0265: Keep the dock's items in one structured Settings1 value

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Shell (dock, launcher), Settings service (schema)
- **Supersedes:** The pinned-application id list of
  [ADR-0076](0076-register-launcher-persistence-in-panel-settings.md)
  (`panels.launcherPinned`) as the pin authority. ADR-0076's recent list and
  its schema registration stay in force.
- **Superseded by:** None

## Context

Plan W13 asks for a dock that takes dropped applications, folders, and files,
groups applications into named groups that unfold upward, keeps a Trash item,
pins applications from everywhere, and shows a running pinned application as
one icon. Before this change the pins were ADR-0076's `panels.launcherPinned`:
a flat list of at most 16 desktop-entry ids, applications only. Nothing in
the shell accepted a drop, and the task strip showed every running window as
its own tile, so a pinned application that was running appeared twice.

Options considered:

- **Encode other kinds into the id list** (`"folder:/home/u"`). Rejected: a
  string grammar inside an id list is ambiguous, cannot express groups, and
  would reach every reader of the key.
- **One key per kind** (folders, files, groups). Rejected: the order across
  kinds cannot be expressed, and Settings1 commits one key per transaction,
  so a drop that moves an application into a group would tear across keys.
- **A dock file outside Settings1.** Rejected: it would be a second
  persistence authority without confirmed writes or change notification.
- **One structured value.** Chosen.

## Decision

1. **`panels.dockItems`** (schema v2, `object`, default `{}`) holds
   `{"version": 1, "items": [...]}` with exactly these item records:
   `{"kind": "application", "id"}`, `{"kind": "folder" | "file", "path"}`,
   `{"kind": "group", "name", "applications": [ids]}`, and
   `{"kind": "trash"}`. An unknown kind or field, another version, or any
   bound violation rejects the whole value.
2. **Bounds.** At most 32 top-level items and 64 applications (group members
   included), 16 applications per group, group names of 1–64 characters on
   one line, and absolute, clean paths of at most 1,024 characters. An
   application appears once anywhere in the dock, a path once, and there is
   one Trash. Groups are one level deep. The largest admissible value fits
   Settings1's value bound, which a compile-time check keeps true.
3. **One codec.** `QindaQt::Services::DockItems` (`src/services/dock_items`)
   is the only reader and writer of the shape. Every edit validates on a
   copy and publishes only on success.
4. **One-time migration.** While `panels.dockItems` is absent or `{}`, the
   dock is read from `panels.launcherPinned` under ADR-0076's rules (whole
   list rejected when malformed, 16 ids at most). The first dock edit writes
   the complete structured value. `panels.launcherPinned` is never written
   again and is ignored once the dock value exists. A malformed dock value
   reads as an empty dock with visible status (the legacy list does not stand
   in for it) and is replaced by the next explicit edit.
5. **One writer in the shell.** `LauncherPersistenceController` stays the
   shell's single dock writer. Its `pinned()` projection is every dock
   application, so the launcher's Pinned section, the start menu's pinned
   grid, and application tiles follow the dock. Writes keep ADR-0012's
   semantics: live apply, a confirmed refusal reverts, an uncertain outcome is
   never replayed, and one write is in flight at a time. The launcher's pinned
   ceiling rises from 16 to the dock's 64 applications.
6. **Other processes** pin and unpin through `Settings1DockPins`
   (`QindaQt::DockPins`): one whole-value write, reported Saved only after a
   same-owner, same-epoch snapshot at or beyond the Applied revision reads it
   back (the ADR-0249/ADR-0255 pattern), never replayed.
7. **Opening goes through existing seams only.** Folders and the Trash open
   in the File Manager through the Places menu's folder opener (the
   launcher's process seam with absolute candidates). Files, a pinned folder's
   listing, and its children go through the File Manager's public
   `FileBoundary`. Empty Trash uses `FileBoundary`'s mutation controller after
   an explicit confirmation. Stored paths are data: nothing is executed, and a
   dropped `.desktop` file is used only as the name of an installed
   application.
8. **One icon per application.** The quick-launch manifest gains
   `windows.read` and `windows.activate`. The dock matches windows to
   desktop entries with the task list's own resolver, shows a running
   indicator, and brings a running pinned application forward instead of
   starting it again. In a dock zone that also shows the pins, the task
   strip leaves out the windows a top-level pinned tile stands for; group
   members keep their task tiles.

## Consequences

- Existing pins survive: the first dock edit migrates them.
- The dock value is one per user. Every panel that hosts the quick-launch
  applet shows the same dock, as the pins were shared before.
- A shell older than this change reads only `panels.launcherPinned`, which
  stops following the dock after migration; no downgrade path is offered.
  The schema key and the shell ship together, since Settings1 rejects a
  whole scoped snapshot that names an unknown key.
- The File Manager's Applications "Keep in Dock" (plan W12) uses
  `Settings1DockPins`, not the shell.
- Dragging a desktop icon onto the dock is not supported, because desktop
  icons move by an internal drag rather than a platform drag. Desktop icons
  join the dock through their menu (Add to Dock, Pin to Dock).
- Focused tests: `qindaqt.services-dock-items`, `qindaqt.services-dock-pins`,
  `qindaqt.launcher-persistence`, `qindaqt.launcher-settings-contract`,
  `qindaqt.desktop-controls-dock`, `qindaqt.desktop-controls-offscreen-dock`,
  the applet resolver, and the panel geometry QML rows.

## Revisit when

Revisit if Settings1 gains multi-key transactions, if docks need per-panel
item sets, or if groups need nesting.

See [Dock items](../shell/dock-items.md).
