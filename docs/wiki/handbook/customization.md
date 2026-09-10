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

Five themes ship with QindaQt:

| Theme | Character |
| --- | --- |
| QindaQt Smoked Plum | The dark default: smoked plum, pearl text, and an apricot accent |
| QindaQt Pearl | The light counterpart: pale ceramic and apricot |
| QindaQt Velvet | Between the two, for late evenings |
| Qinda High Contrast | Maximum separation for readability |
| Qinda macOS | A mist-and-sage take on the familiar Mac look |

Pick a card to preview it. The **Preferred color scheme** control on the same
tab offers **System**, **Light**, and **Dark**. Light and Dark fix the scheme.
System follows the desktop's current light-or-dark appearance preference and
changes whenever that preference changes — it is not a clock or a day-and-night
schedule. If the theme you picked is the wrong kind for the scheme in force
(Nightfall under a light preference, say), QindaQt shows the matching built-in
QindaPunk theme instead until it fits again; Qinda High Contrast fits both.
When you **Apply**, the whole desktop follows — panels, controls, window
frames, and container chrome — without a restart. **Revert** puts everything
back to your saved choices.

### Wallpaper

Choose from the five bundled wallpapers — Jade Fold, Porcelain Dawn, Ink
Tide, Qinda Punk, and Compile Club — or pick **any image on your disk**. A
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

Open **System Settings → Customize**. The page opens with a gallery of layout
previews — small monitor thumbnails that show where each preset puts its
panels. Pick the one that looks right: the default QindaQt layout, a minimal
one, or layouts inspired by GNOME, Unity, MATE, XFCE, NeXTSTEP, macOS, and
classic or modern Windows. These are starting-points in the spirit of those
desktops, not recreations of them, and the preview below updates the moment
you pick one.

From there the monitor lets you rework the layout directly — it mirrors what
your desktop will look like, in your current theme:

- **Move things.** Drag an applet between panels or between the start,
  middle, and end zones of a panel; the target lights up before you let go.
- **Add and remove.** Add applets from the icon palette (hover a tile to see
  what it is), duplicate a selected applet (`Ctrl+D`), or remove it
  (`Delete`).
- **Reshape a panel.** Click it on the monitor and the side panel offers a
  compass for the screen edge, alignment buttons, thickness and length
  sliders, and a visibility choice — icons with tooltips, not walls of text.
- **Stay safe.** Every gesture is one **Undo** step; **Redo** walks forward
  again. `Ctrl+Return` applies the draft, `Ctrl+Shift+Return` throws it away,
  and closing the window with unsaved changes asks first. A cancelled drag
  never leaves a half-finished layout behind.

Keyboard editing works too: select an applet and press `Space` to start a
move, `Ctrl+Left`/`Ctrl+Right` to step along the panel, `Alt+Left`/
`Alt+Right` to change zone, and `Ctrl+Shift+Left`/`Ctrl+Shift+Right` to
change panel; `Space` commits.

Your edits are saved as **your own copy**. The built-in presets stay
untouched, so you can always go back and start fresh from one.

A selected applet shows its own settings for inspection; editing those values
from this page is not part of the current version.

## What each part is

- The top panel and the dock are both **panels**; the things living on them
  (clock, launcher, task list, volume, …) are **applets**.
- The full inventory of shipped profiles, themes, and applets is in the
  [packaged assets catalog](catalog/assets.md).
- Exact preference names and defaults are enumerated in the
  [settings catalog](catalog/settings.md).

For the engineering behind the editor — preview commits, atomic saves,
conflict handling — read [the customization
editor](../shell/customization-editor.md) and [the Customize
route](../apps/customize-settings.md). Theme and profile file formats are
specified in the [theme schema](../reference/theme-schema-v1.md) and
[profile schema](../reference/profile-schema-v1.md); fonts are covered under
[font preferences](../architecture/font-preferences.md). Continue with
[using the desktop](desktop.md), [applications](applications.md), or the
[handbook index](index.md).
