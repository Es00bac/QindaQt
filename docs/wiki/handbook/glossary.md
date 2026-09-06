# Glossary

Two kinds of words show up in QindaQt documentation: the everyday words for
what you see on screen, and the engineering terms used in the deeper
reference pages. Both tables use the same fixed meanings everywhere, so a
term never means two things. The stable user-facing term for grouped windows
is **window container**.

## Everyday terms

| Term | Meaning |
| --- | --- |
| Window container | A group of windows sharing one outer frame, one title row, and one tab per page; created by combining windows and undone by detaching or ungrouping. |
| Member | One of the real windows inside a window container. It keeps its own slim title strip and its own window buttons. |
| Page | One tab-selectable layout inside a window container; each page is a tree of splits ending in tiles. |
| Split / tile | A division between two members / the slot holding exactly one member. |
| Shared bar (shared row) | The container's combined title and tab row, including its whole-group buttons. |
| Outer frame | The visible frame around a whole window container; dragging it moves or resizes the entire group. |
| Docking | Combining or rearranging windows — by `Meta+Shift` drag or `Meta+Shift+D`. |
| Detach | Returning one member of a container to an independent window. |
| Ungroup | Releasing every member of a container at once. |
| Command bar | The top bar: system menu, application menu, workspaces, and status applets. |
| Smart shelf | The bottom bar: launcher, pinned apps, and the task list; it steps aside for windows. |
| Launcher | The applet that finds and opens installed applications by name or category. |
| Task list | The window list in the shelf; each window container appears there as one entry. |
| System menu | The left command-bar button: about, settings, lock, log out, suspend, restart, shut down. |
| Application menu (global menu) | The focused application's menu hosted in the command bar. |
| Panel | One bar on a screen edge, described by a profile. |
| Applet | One component living on a panel (clock, launcher, volume, …), admitted by the host. |
| Profile | A saved layout: which panels exist and which applets they carry. |
| Theme | A saved look: colors, typography inputs, corner rounding, motion, and icons. |
| Welcome guide | The first-run introduction, reopenable from the launcher. |
| Notification center | The `Meta+N` surface with recent notifications and Do Not Disturb. |
| Settings Center | The System Settings application; each subject (Appearance, Display, …) is a route. |
| Route | One page of the Settings Center. |

## Terms in the deeper documentation

| Term | Meaning |
| --- | --- |
| AppShell | The shared application framework for lifecycle, actions, menus, and integration — not the desktop shell. |
| Client | A compositor-managed application toplevel. |
| Chrome | The frame, title bars, tabs, and buttons of a window or container, and how they are painted and hit-tested. |
| Hybrid | The architecture combining independent windows and window containers. |
| QST-1 | QindaQt's semantic design-token contract; consumers ask for meaning (“surface”, “accent”) rather than raw colors. |
| Controls | The reusable, theme-driven QML primitives shared by QindaQt applications. |
| Exact owner | The current unique service identity behind a bus name, not just the reusable name. |
| Epoch / revision | Lineage and ordering values used to reject obsolete state. |
| Intent | A requested operation; not proof that the authoritative change happened. |
| Lease | Bounded, revocable permission for an ongoing activity such as discovery. |
| Fail closed | Deny exposure or action when required trust or validation cannot be established. |
| Logical pixel | The desktop coordinate unit before per-screen scaling is applied. |
| Exclusive area | Screen space a panel reserves, keeping windows from placing themselves underneath. |
| Nested session | A test desktop running inside an isolated parent compositor. |
| Rootless XWayland | Legacy X11 application windows integrated into the Wayland desktop. |
| Portal | A standard desktop mediation interface (files, screenshots, shortcuts, …); which backends implement it is explicit. |
| PSS | Proportional set size: memory accounting that apportions shared pages. |
| ADR | Architecture decision record: a versioned decision with rationale and consequences. |
| Stopping point | The exact implemented boundary of a feature, with its evidence. |
| Candidate / integration | A reviewable isolated commit / an accepted commit incorporated on the manager branch. |

For normative definitions see
[window containers](../architecture/window-containers.md),
[module boundaries](../architecture/module-boundaries.md), [team-board
maturity](../contributing/team-board.md), and the [protocol
catalog](catalog/reading.md). Return to the [handbook index](index.md).
