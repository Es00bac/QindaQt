# Using the desktop

This page explains the QindaQt desktop the way you actually meet it: what is
on screen, how windows group together, where menus and notifications live,
and which keys do what. Nothing here requires touching a configuration file.

## The screen at a glance

The default layout has two bars:

- **The command bar** across the top. Its left button opens the **system
  menu** — About QindaQt, System Settings, Lock, Log out, Suspend, Restart,
  and Shut Down (the destructive ones ask for confirmation). Next to it sits
  the menu of the application you are working in; more on that under
  [The application menu](#the-application-menu). The middle shows your
  workspaces, and the right side collects status: clock, notifications,
  system tray, audio volume, power, Bluetooth, and clipboard.
- **The smart shelf** at the bottom. The launcher lives here (click it, type
  a name or browse categories, open the app), along with pinned apps and one
  entry per running window or group. The shelf steps aside when a window
  needs its space and comes back when there is room.

Other looks are available — see [Making it yours](customization.md).

## Window containers

Most desktops make you choose between independent windows and a tiling
system. QindaQt's answer is the **window container**: any windows you choose
share one outer frame with a shared title row and a tab per page, while
every window keeps its own slim title strip and its own close, minimize, and
maximize buttons. Windows you never group are untouched — containers are
opt-in.

### Combining windows

Hold **Meta+Shift** and drag one window onto another with the left button:

- drop over the **middle** of the target and the two become tabs of one
  container;
- drop near an **edge** and the two land side by side (or stacked) inside the
  frame;
- a preview shows exactly what you will get before you release;
- dropping a window that is already in a group onto empty space pulls it back
  out as an independent window.

You can start the drag anywhere on the window — title bar or content — not
just on a thin strip.

Prefer the keyboard? **Meta+Shift+D** starts docking mode on the active
window: the **arrow keys** choose an edge, **T** chooses the tab target,
**D** detaches a grouped member, **Enter** confirms, and **Esc** cancels.

### One shelf entry per container

A container appears as a **single entry** in the smart shelf, not one per
window. Clicking the entry brings the container back with its current page in
front; if one of the hidden windows needs attention, the entry shows that
urgency. The container's minimize button (or **Minimize group** in its menu)
collapses the whole group at once, and clicking the shelf entry restores it —
again with only the active page revealed.

### The shared bar and the group menu

A container's shared row carries, left to right in the default style: the
close, minimize, and maximize buttons for the **whole container** (their
symbols appear when you hover), the title, and the page tabs. The buttons on
the far side toggle the member title strips and open the **group menu**.

The group menu is also one right-click away — on the shared title, on a tab,
or on a member's title strip. It offers:

- **Arrange windows** — start the keyboard arrangement mode on the selected
  member (the same arrows / `T` / `D` / `Enter` / `Esc` commands as
  `Meta+Shift+D`);
- **Detach active window** — return the focused member to an independent
  window;
- **Ungroup** — release every member at once;
- **Minimize group** — collapse the whole container;
- keeping the group above or below other windows, showing it on all
  workspaces or specific ones, and moving it to another screen.

The container's close button never closes everything without asking: it opens
a small **Close All / Ungroup / Cancel** choice, with Cancel as the default.

### Taking a window back out

Drag the window's own title strip out of the frame and drop it anywhere — the
window detaches and follows your pointer like a normal drag. The group menu's
**Detach active window** does the same for the focused member without the
dragging. Everything else about the window — size, position, its own buttons
— works the way it did before you grouped it.

### Focus you can see

The focused container gets an accent line along the top of its frame, the
active tab carries a short accent underline, and the focused member gets an
accent ring on its side of the frame. The cues come from your theme, so they
stay readable in light and dark themes and when member titles are hidden.

### One member at full size

Maximizing or full-screening a member presents it alone within the
container's frame — the other members and the shared chrome step out of the
way, but the window stays in the group. Maximize it again (or leave
fullscreen) and the exact previous layout comes back.

### Moving and resizing

- Drag the shared title to move the whole container; drag its outer border to
  resize it.
- Drag the divider between two members to change how the split divides the
  space; windows with fixed size limits stop the divider where they must.
- Drag a tab to reorder pages, move a page to another container, or drop it
  on empty space to make it its own container.

### Keyboard reference

These are the **default** bindings; they can be reassigned through the
standard global shortcut settings. In the interactive modes, **Enter**
confirms and **Esc** cancels.

| Keys | What they do |
| --- | --- |
| `Meta+Shift` + drag | Combine or rearrange windows |
| `Meta+Shift+D` | Dock the active window (arrows = edge, `T` = tab, `D` = detach) |
| `Meta+Ctrl+Shift+D` | Dock the active page (whole tab) |
| `Meta+Shift+M` | Move the whole group with the arrow keys |
| `Meta+Shift+S` | Adjust the active split with the arrow keys |
| `Meta+Shift+R` | Resize the whole group with the arrow keys |
| `Meta+Ctrl+PageDown` / `Meta+Ctrl+PageUp` | Next / previous page |
| `Meta+Ctrl+Shift+PageDown` / `Meta+Ctrl+Shift+PageUp` | Reorder the active page |
| `Meta+Ctrl+Shift+N` | Minimize the whole group |
| `Meta+Ctrl+Shift+X` / `Meta+Ctrl+Shift+U` | Maximize / restore the whole group |
| `Meta+Ctrl+Shift+Q` | The group's Close All / Ungroup / Cancel choice |
| `Meta+Shift+C` | Show or hide member title strips (for this session) |
| `Meta+F1` | Show or hide the desktop shortcut note |
| `Meta+N` | Open the notification center |

One rule worth knowing: while a window is grouped, its frame belongs to the
container. Native per-window tiling shortcuts are redirected so a member
cannot slide out of its page on its own; maximize and fullscreen become the
solo presentation described above.

> **Rollout note.** The container controls described here are part of the
> current QindaQt build. On an already-running installed desktop the newest
> of them appear after the next compositor restart, which is deliberately
> scheduled while their interaction checks finish. The exact per-feature
> status is in the [feature catalog](catalog/features.md).

## The application menu

The menu of the window you are working in appears in the command bar, next to
the system menu — like on a Mac, so the app's window doesn't spend its height
on a menu row. Click an entry and its submenu opens right there; choosing an
item performs the application's own action.

The first-party Text Editor, Terminal, and File Manager export their menus
this way, and Sloom Studio's native menu wiring is supported — its
real-world use on the installed desktop is still being qualified.
Applications keep their in-window menu until the bar is actually displaying
their menu — if a profile has no menu bar, or the app has no export, nothing
is hidden and the bar simply leaves the space clear.

## The desktop shortcut note

The first time the desktop starts, a small **Desktop shortcuts** card sits on
the wallpaper listing the default combining and docking keys (the same list
as in the table above). **Got it** dismisses it, and the choice is remembered
for later sessions. **Meta+F1** hides or shows the card whenever you want it
back. The card is part of the wallpaper layer: it never appears in the task
list, never takes keyboard focus, and always labels its list as defaults.

## Notifications

Notifications arrive as brief popups. **Meta+N** opens the notification
center with your recent history. **Do Not Disturb** — toggleable in the
center and in System Settings → Notifications — silences the ordinary
popups while critical notifications still come through; banners suppressed
while it was on are not replayed when you turn it off. While the session is
locked, notification contents stay private.

## Gabbee dictation

Gabbee is a third-party dictation utility, not part of QindaQt. QindaQt's
portal configuration routes the standard GlobalShortcuts interface it relies
on to the installed KDE backend, so its global hotkeys can register. Text
insertion into QindaQt applications and windows — including grouped windows —
is still being qualified, so treat dictation as experimental until that work
is recorded as finished.

## Going deeper

Exact interaction contracts, invariants, and qualification limits live in
[window containers](../architecture/window-containers.md), [hybrid
topology](../architecture/hybrid-topology.md), [hybrid
chrome](../architecture/hybrid-chrome.md), [panels](../shell/panel-surfaces.md),
[the global menu](../shell/global-menu.md), and [notification
presentation](../shell/notification-presentation.md). Continue with
[making it yours](customization.md), [applications](applications.md), or the
[handbook index](index.md).
