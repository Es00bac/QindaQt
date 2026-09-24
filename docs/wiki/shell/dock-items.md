# Dock items

The dock is the quick-launch applet: on the macOS-style and smart-shelf docks
it is the pinned part of the dock, and on taskbars it is the quick-launch
strip. It shows one user-wide list of items, stored in the Settings1 value
`panels.dockItems` ([ADR-0265](../adr/0265-keep-dock-items-in-one-structured-settings-value.md)).
Every panel that hosts the applet shows the same items. Growth, magnification,
and the task strip's own reordering are described in
[Dock interactions](dock-interactions.md).

## Items

| Kind | Tile | Click | Menu |
| --- | --- | --- | --- |
| Application | Its icon; a dot when running | Brings a running window forward (cycling), or starts it | Open, Open New Window (while running), New Group…, Move, Remove from Dock |
| Group | A plate showing its first four icons | Unfolds its applications | Open, Rename Group…, Ungroup, Move, Remove from Dock |
| Folder | Folder icon | Unfolds its first 48 children (a stack) | Open, Open in File Manager, Move, Remove from Dock |
| File | Its type's icon | Opens with the default application | Open, Move, Remove from Dock |
| Trash | `user-trash` | Opens the Trash in the File Manager | Open, Open in File Manager, Empty Trash…, Move, Remove from Dock |

Every menu ends with **Show Trash in Dock** while the dock has no Trash item.
Applications that are no longer installed keep their stored place but show no
tile, like the launcher's stale pins. The launcher's Pinned section, the start
menu's pinned grid, and application tiles all list the dock's applications,
group members included.

## Gestures

- **Drop to add.** Launcher rows (and the File Manager's Applications view,
  plan W12) offer an application under
  `application/x-qindaqt-desktop-entry-id`. Files and folders arrive as
  `text/uri-list`: a folder becomes a folder item, a file a file item, a
  `.desktop` file that names an installed application becomes that
  application, and the Trash's files folder becomes the Trash item. Remote
  URLs and missing paths are ignored; a drop with nothing usable is refused
  with feedback. While something is dragged over the dock the tiles on each
  side move half a slot apart, and the drop lands in that gap. An empty dock
  still accepts a drop within one tile of its place.
- **Group.** Dropping an application on the middle of an application or a
  group tile groups them (the target is outlined). Two applications from the
  same launcher category name the group after it ("Office"); otherwise it is
  "Group". **New Group…** wraps one application and asks for a name.
- **Move.** Drag a tile along the dock (a click never moves it), or use
  Move left/right in its menu, which keeps keyboard focus on the moved tile.
- **Remove.** Drag a tile a whole tile's height off the dock ("Release to
  remove from the Dock" appears), or use **Remove from Dock**.
- **Out of a group.** Drag a member from the group's list back onto the dock,
  or use **Remove from Group** in the member's menu (it lands after its
  group).

## Stacks

Groups and pinned folders open a list that unfolds out of the dock's edge
(upward from a bottom dock). It is the panels' own popup
(`ControlPopupFrame` over `PanelPopup`), so it opens away from the panel edge,
takes keyboard focus into its first row, moves with Up and Down, and closes
on Escape or a click outside. A folder's list ends with **Open in File
Manager**. Reduced motion skips the unfold.

## Pinning from elsewhere

| Where | Action |
| --- | --- |
| Launcher rows | Pin/Unpin button, right-click or Menu key: Pin to Dock / Remove from Dock, or drag onto the dock |
| Start menu program list | Right-click or Menu key: Pin to Dock / Remove from Dock |
| Running window (task list) | Right-click or Menu key: Keep in Dock, when an installed application owns the window |
| Desktop icons | Right-click: Pin to Dock (a `.desktop` file) or Add to Dock (anything else); the whole selection joins |
| Another process | `Settings1DockPins` (below) |

A pinned application that is running shows a dot, and its accessible
description begins with "Running". In a dock that shows the pins, the task
strip leaves out windows that a top-level pinned tile stands for, so each
application has one icon. Members of a group keep their task tiles.

## Storage and authority

`panels.dockItems` is `{"version": 1, "items": [...]}`. Its record shapes,
bounds (32 items, 64 applications, 16 per group, names up to 64 characters,
paths up to 1,024), and the rules below are ADR-0265's. The only codec is
`QindaQt::Services::DockItems` in `src/services/dock_items`.

- **Migration.** Until the first dock edit, the dock is read from ADR-0076's
  `panels.launcherPinned` (rejected as a whole when malformed). The first
  edit writes the structured value; the old key is never written again.
- **Writes.** The launcher's `LauncherPersistenceController` is the shell's
  only writer. An edit applies to the live dock and commits the whole value;
  a refusal restores the last confirmed dock and shows why; an uncertain
  outcome is never replayed; a second edit while one is saving is refused
  ("still saving").
- **Malformed values** read as an empty dock with a visible status, and the
  next edit replaces them.
- **Paths are data.** The dock never runs or reads a stored path. Folders and
  the Trash open through the Places menu's File Manager opener; files, folder
  listings, and their children go through the File Manager's `FileBoundary`;
  Empty Trash uses its mutation controller and always asks first.

### For other processes

Link `QindaQt::DockPins` and give a `SettingsClient` the scope
`Settings1DockPins::scopedKeys()`. `pinApplication(id)` appends an
application; `unpinApplication(id)` removes it wherever it is. A `true`
return means the request was admitted; `requestFinished(id, pin, outcome)`
reports `Saved` only after a same-owner, same-epoch snapshot reads the dock
back. `Refused`, `Conflict`, and `Uncertain` are never retried. The shell
picks the change up from Settings1 like any other writer's.

## Code map

| Piece | Location |
| --- | --- |
| Value, bounds, codec, migration | `src/services/dock_items` (`DockItems`) |
| Pin helper for other processes | `src/services/dock_items` (`Settings1DockPins`) |
| Persistence and pinned projection | `src/shell/launcher/src/launcher_persistence.*` |
| Dock facade | `src/shell/desktop_controls/src/quick_launch_controller.cpp`, `quick_launch_dock_edits.cpp` |
| File Manager seam | `dock_path_port.h`, `file_manager_dock_paths.*` |
| Presentation | `QuickLaunchApplet.qml`, `DockItemTile.qml`, `DockItemMenu.qml`, `DockStackPopup.qml`, `DockPromptPopup.qml`, `DockGestures.qml`, `DockDropGeometry.js` |

## Focused tests

```sh
ctest --test-dir build/dev -R 'dock|launcher-persistence|launcher-settings-contract' --output-on-failure
```

| Test | Scope |
| --- | --- |
| `qindaqt.services-dock-items` | Edits, uniqueness, bounds, the strict codec's hostile corpus, migration |
| `qindaqt.services-dock-pins` | Whole-value writes, migration on first pin, readback, refusal and conflict without replay, stale readback |
| `qindaqt.launcher-persistence` | Dock commits, migration, malformed dock, refusal revert |
| `qindaqt.launcher-settings-contract` | The shipped schema stores the dock value on disk and across a service restart |
| `qindaqt.desktop-controls-dock` | Rows, drops, groups, removal, File Manager seam, running indicators, Keep in Dock, feedback |
| `qindaqt.desktop-controls-offscreen-dock` | Tiles and accessible text, stacks, drag to move/group/remove, drops with a live gap, keyboard menu |
| `qindaqt.panel-geometry-offscreen` | Claimed task tiles in the dock fit arithmetic |

Dragging with a real pointer, drops from the File Manager across windows, and
the Wayland popup placement of the stack need a live session.
