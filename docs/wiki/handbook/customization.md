# Making it yours

Three things shape how QindaQt looks and behaves, and they are kept separate
on purpose:

- a **profile** describes the layout — which panels exist, where they sit, and
  which applets they carry;
- a **theme** describes the look — colors, corner rounding, motion, icons;
- your **saved choices** — theme, wallpaper, fonts, and other preferences —
  are stored individually, so you can change the palette without disturbing
  the layout and vice versa.

Everything below happens in System Settings; no file editing required.

## Themes, wallpaper, and fonts

Open **System Settings → Appearance**. The page has three tabs across the
top, and everything you try is previewed live on the page before you save.

### Themes

Among the themes that ship with QindaQt:

| Theme | Character |
| --- | --- |
| QindaQt Smoked Plum | The dark default: smoked plum, pearl text, and an apricot accent |
| QindaQt Pearl | The light counterpart: pale ceramic and apricot |
| QindaQt Velvet | Between the two, for late evenings |
| Qinda High Contrast | Maximum separation for readability |
| Qinda Mist | A mist-and-sage take on the familiar Mac look |
| Qinda Classic Blue | An XP-era option: squared corners, beige surfaces, blue title bars, and blue tile buttons |
| Qinda Daylight | A bright, softly translucent modern look with wide flat window buttons |
| Qinda Marigold | Warm greys with a yellow title tab that rolls the window up on double-click |
| Qinda Classic Grey | Bevelled greys and a navy title bar; minimizing turns a window into a desktop icon |
| Qinda Graphite | Mid greys, black title bars, and bold square buttons |

The separate **Qinda Seven** package adds Saffron, Glacier, Orchid, Forge,
Fern, Nocturne, and Signal. Each has a paired window and container decoration
and a KDE color scheme. The Gentoo `x11-themes/qinda-seven` package installs
them into the same appearance catalog; installation does not select one for
you. Its source and previews live in the `QindaThemes` repository.

Pick a card to preview it. The **Preferred color scheme** control on the same
tab offers **System**, **Light**, and **Dark**. Light and Dark fix the scheme.
System follows the desktop's current light-or-dark appearance preference and
changes whenever that preference changes — it is not a clock or a day-and-night
schedule. If the theme you picked is the wrong kind for the scheme in force
(Nightfall under a light preference, say), QindaQt shows the matching built-in
QindaPunk theme instead until it fits again; Qinda High Contrast fits both.
A theme in QindaQt is one choice with two reach: the panels, Settings, and
bundled applications paint from the QST tokens, and ordinary Qt applications
(anything built on stock Qt6) receive the same colors, fonts, and icons
through the Qt platform theme — the **Applications and toolkits** card shows
the exact palette they get. When you **Apply**, the whole desktop follows —
panels, controls, window frames, and container chrome — without a restart.
**Revert** puts everything back to your saved choices.

### Wallpaper

Choose from the six bundled wallpapers — Jade Fold, Porcelain Dawn, Ink
Tide, Qinda Punk, Compile Club, and Qinda Bliss (rolling green hills under
a blue sky) — or pick **any image on your disk**. A
picture mode of scaled, centered, or tiled decides how the image fills the
screen. After **Apply**, the wallpaper appears on every screen.

### Fonts

Pick any font family installed on the machine; a live sample shows your text
in each candidate. A slider sets the size (6–36 pt), and finer controls
cover anti-aliasing, hinting, and subpixel ordering. First-party
applications pick the choice up when they start.

One thing deliberately lives elsewhere: **screen scaling** belongs to
**System Settings → Display**, which owns the live screen configuration.
Appearance offers a direct jump there rather than a second copy of the
setting.

## Changing the layout

Open **System Settings → Customize** to pick a layout. The page shows each
layout as a small monitor thumbnail of where it puts its panels: the
built-in layouts first (the default Menu and Dock, QindaQt's own, a minimal one,
layouts inspired by GNOME, Unity and XFCE, and five familiar desktops), then
**My presets**. Click one and the desktop switches to it; the page says so once
the switch is confirmed. Each familiar desktop pairs with a theme; choosing the
layout keeps your current theme, so pick its theme in Appearance for the whole
feel:

| Layout | Feels like | Pair it with |
| --- | --- | --- |
| Menu and Dock | A Mac: menu bar on top, dock below with the File Manager first and the Trash last | Qinda Mist |
| Classic Taskbar | XP: start button, quick launch, window buttons, tray | Qinda Classic Blue |
| Centered Taskbar | Windows 11, without advertising or account prompts | Qinda Daylight |
| Corner Bar | BeOS: yellow title tabs and a bar in the top-right corner | Qinda Marigold |
| Program Groups | Windows 3.1: program groups on a top bar, windows that minimize to desktop icons | Qinda Classic Grey |
| Workspace Dock | NeXTSTEP: a dock column down the right edge | Qinda Graphite |

Menus stay inside windows in every one of these except Menu and Dock. These are
starting points in the spirit of those desktops, not recreations of them.

To change a layout, edit the panels themselves. Right-click a panel and
choose **Edit Panels**:

- **Move things.** Drag an applet within a panel, onto another panel, or
  onto a panel on another display; a gap shows where it will land.
- **Add and remove.** **Add applet…** on the panel's edit bar lists what
  fits each zone. Meta+right-click an applet to move it, open its settings,
  **Duplicate** it or remove it.
- **Reshape a panel.** Meta+right-click the panel: **Panel** holds its edge,
  alignment, auto-hide, size and length, and **Displays** puts it on this
  display only or on all of them. **Add panel** and **Remove panel** are
  there too.
- **The desktop.** Meta+right-click the desktop for **Desktop icons**:
  show or hide them and set their size, placement and menu style.
- **Undo** walks back one change at a time; **Done** or Escape leaves edit
  mode.

Every change is saved straight away to the layout you are using. Editing a
built-in layout keeps **your own copy**: its card then says **Modified**,
and **Restore original** brings the built-in back. To keep a layout as it is
now, use **Save current layout as preset…**; your presets can be renamed,
duplicated and deleted (deleting the one in use switches to the default
first). The page also holds the auto-hide delay for all panels.

Editing the panels with the keyboard alone is not possible yet: the menus
work with the keyboard once open, but opening them needs the pointer.

## What each part is

- The top panel and the dock are both **panels**; the things living on them
  (clock, launcher, task list, volume, …) are **applets**.
- The full inventory of shipped profiles, themes, and applets is in the
  [packaged assets catalog](catalog/assets.md).
- Exact preference names and defaults are enumerated in the
  [settings catalog](catalog/settings.md).

For the engineering behind it — one gesture per edit, atomic saves,
confirmed switching — read [the customization
editor](../shell/customization-editor.md), [the panel
menus](../shell/panel-surfaces.md#in-place-customization-meta-right-click) and
[the Customize route](../apps/customize-settings.md). Theme and profile file formats are
specified in the [theme schema](../reference/theme-schema-v1.md) and
[profile schema](../reference/profile-schema-v1.md); fonts are covered under
[font preferences](../architecture/font-preferences.md). Continue with
[using the desktop](desktop.md), [applications](applications.md), or the
[handbook index](index.md).
