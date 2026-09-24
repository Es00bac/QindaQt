# ADR-0268: Familiar desktop experiences are layout and theme pairs

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Shell (layout profiles, dock), Themes, Decorations
- **Supersedes:** [ADR-0124](0124-add-qindaqt-bliss-luna-option-set.md)
  decision 2's theme name, font and glyph buttons, and decision 3's rule that
  every authored title bar is weathered
- **Superseded by:** None

Extends [ADR-0223](0223-one-stock-profile-per-distinct-feel.md) (two more
stock profiles), [ADR-0264](0264-window-button-styles-are-data.md) (a theme
may author title-bar behaviour, its "revisit when" case) and
[ADR-0265](0265-keep-dock-items-in-one-structured-settings-value.md) (the
dock's permanent ends).

## Context

Plan W20 asks for familiar desktop experiences, each one layout profile, one
theme, a W19 decoration style, and the right global-menu rule: keep the
Mac-style experience as it is apart from a File Manager that always leads its
dock, a Trash that always ends it, and the system menu at the top left; make
the XP-like and Windows 11-like layouts feel like those desktops (the latter
without advertising or nags); add a modern BeOS (yellow title tabs, a
top-right bar, roll-up on double-click) and a modern Windows 3.1 (program
groups, windows that minimize to icons on the desktop, a folder tree beside
the file list); refresh the NeXT layout (miniwindows, NeXT buttons). The
owner's constraints: the fewest changes to existing code, no Microsoft or
Apple names or artwork in anything the user reads, internal ids unchanged,
and the global menu only in the Mac-style, QindaQt and Unity-style layouts.

What the code allowed:

- Themes carry the decoration style, but any theme that authors a title-bar
  color was painted with ADR-0124's weathered texture (rust chips, speckles,
  a Trebuchet caption), so a yellow tab or a navy bar could not be clean.
- W19's double-click and roll-up options are user preferences. A layout
  cannot set preferences, and choosing a layout does not change the saved
  theme (ADR-0074): a profile's `defaultTheme` only fills in when nothing is
  saved.
- The dock is one user-wide value (ADR-0265) shown by every quick-launch
  instance, and the Mac-style dock ordered the launcher, the pins, then the
  task strip, so no stored item can sit before the launcher or after the
  running applications.
- "Iconify" in this compositor is ADR-0203's roll-up to an icon chip on the
  desktop; KWin 6.6 has no native shade.
- The File Manager's style setting (Finder, Explorer, Commander; W11s,
  ADR-0271) is not written yet.

## Decision

1. **An experience is a stock profile plus the theme it names.** The theme
   carries the W19 button style and any title-bar behaviour; the profile
   carries the panels, the global-menu rule and the File Manager hint.

   | Profile (id) | Name shown | Theme (id, name) | Buttons | Global menu | File Manager |
   | --- | --- | --- | --- | --- | --- |
   | `macos-inspired` | Menu and Dock | `qinda-macos`, Qinda Mist | traffic lights | yes | finder |
   | `qinda-bliss` | Classic Taskbar | `qinda-bliss`, Qinda Classic Blue | `blue-tiles` | no | explorer |
   | `windows-modern` | Centered Taskbar | `qinda-daylight`, Qinda Daylight | `wide` | no | explorer |
   | `beos-inspired` | Corner Bar | `qinda-marigold`, Qinda Marigold | `tab` | no | finder |
   | `win31-inspired` | Program Groups | `qinda-classic-grey`, Qinda Classic Grey | `bevel` | no | explorer |
   | `nextstep-inspired` | Workspace Dock | `qinda-graphite`, Qinda Graphite | `bold` | no | finder |

   The global menu stays in the Mac-style, QindaQt and Unity-style layouts
   only; every other layout keeps menus in windows (ADR-0130).
2. **Names are our own.** Names and descriptions a user reads carry no
   vendor names or artwork titles; ids never change, so saved selections
   keep working. The Bliss profile shows as Classic Taskbar and its theme as
   Qinda Classic Blue (font Noto Sans instead of Tahoma), Qinda macOS as
   Qinda Mist, and the Luna Classic decoration document as Weathered Blue.
   The new themes use our own palettes, checked against the QST-1 contrast
   pairs. Program Groups is `win31-inspired`, not ADR-0223's retired
   `windows-classic`, so a stale saved selection never lands on a different
   layout.
3. **A theme may author title-bar behaviour.** Three optional keys join the
   theme `decoration` block (schemas v1 and v2):
   - `titleDoubleClick`: `maximize`, `roll-up` or `minimize`; absent leaves
     KWin's own double-click. `appearance.windowTitleDoubleClick` = `theme`
     now means "the theme's action, else KWin's"; any other value still
     wins.
   - `minimizeAction`: `minimize` (default) or `roll-up`. With `roll-up` the
     arrangement shows the roll-up control (ADR-0264's upward chevron) in
     minimize's place, once, so the button rolls the window up to its icon.
   - `titleWear`: default `true`, an authored title bar is weathered as
     ADR-0124 painted it; `false` fills it flat in the authored color, with
     the theme's font, caption ink chosen by the bar's lightness and no drop
     shadow. A decoration document that authors a title color keeps its
     weathered bar.

   The published chrome map gains `minimizeRollsUp` and `titleWorn`, both
   omitted at their defaults, so every other theme's map is unchanged.
4. **The Mac-style dock has permanent ends.** A quick-launch instance takes
   the profile setting `items`: `file-manager` shows one fixed File Manager
   tile, `trash` one fixed Trash tile, `others` every stored item except the
   File Manager and the Trash, and anything else (the default `all`) the
   whole dock as before. Fixed tiles are not stored (row `index` -1,
   `fixed` true): they take no drops, cannot be dragged, and their menus
   offer no move, removal or grouping. The File Manager tile opens like a
   pinned application; the Trash tile opens the Trash and offers Empty
   Trash. While a File Manager end is shown (its instance holds the dock
   facade, counted across outputs) the dock claims the File Manager's
   windows, so it keeps one icon. The Mac-style dock reads File Manager,
   Applications, the stored items, the running applications, Trash; the
   dock value and its codec are unchanged.
5. **A layout names the File Manager arrangement it expects.** The profile
   `workflow` object gains `fileManager` (default `finder`; the experiences
   use `finder` or `explorer`, and `commander` is the third style), a
   non-blank hint like the others that survives the strict round trip.
   Windows 3.1's folder tree beside the file list is the Explorer
   arrangement. This is a placeholder for W11s: its style setting uses the
   selected layout's hint as the default until the user picks a style.
   Nothing reads the hint before W11s lands.
6. **Iconify and roll-up reuse ADR-0203.** Program Groups has no task bar;
   its theme's minimize rolls a window up to its icon on the desktop.
   Corner Bar's theme rolls a window up on a title double-click. In the
   Workspace Dock a minimized window waits as its tile in the dock column
   (the task list lives there), so its minimize stays a real minimize.

## Consequences

- Eleven stock profiles and sixteen built-in themes ship; the profile,
  resolver, theme, contrast, benchmark and Appearance preview rows count
  them. Program Groups and Corner Bar add the System Status applet so the
  network, sound and power stay reachable.
- The pairing is advisory: picking a layout does not change the saved theme.
  The handbook names each layout's theme; Settings layout presets (W15) may
  offer to apply the pair.
- Classic Blue's title bar is clean now; the weathered look stays available
  as the Weathered Blue decoration document.
- A clean authored bar uses the flat title painter, so it follows the
  decoration corner radius: square when maximized, and 2 px for the three
  retro themes through their schema v2 `radii.decoration`.
- Under a theme whose minimize rolls up, a contained window's handlebar
  shows no minimize (a member never rolls up alone; its More menu still
  minimizes), and the control is offered like W19's roll-up button, not
  hidden for windows KWin marks unminimizable.
- The Mac-style dock's fit arithmetic counts each permanent end as one tile;
  the `others` slice still counts a stored File Manager or Trash it hides,
  so the dock may shrink tiles one or two slots early when it is full.
- Program groups are the dock value's groups, shared with the Mac-style
  dock; a fresh dock has none until the user makes one.
- Left for follow-up: the Luna taskbar dressing still asks for Trebuchet MS
  and Tahoma by family name (advisory, no font ships), and the bundled
  wallpaper's label "Qinda bliss" comes from its file name, which is its
  saved id.
- Tests: `qindaqt.profile-formats` (experience pairs, vendor-free names,
  the Mac dock order, the hint's default, round trip and rejection),
  `qindaqt.theme-formats` (styles and behaviour keys, defaults, rejection),
  `qindaqt.decoration-title-options` (resolution, override, map round trip,
  the swapped control, the flat clean bar), `qindaqt.decoration-documents`,
  `qindaqt.desktop-controls-dock` and `qindaqt.desktop-controls-offscreen-dock`
  (permanent ends and slices), `qindaqt.applet-runtime-resolution`,
  `qindaqt.global-menu-runtime-composition-private-bus`,
  `qindaqt.design-tokens-built-in-contrast`, `qindaqt.appearance-preview`,
  `qindaqt.appearance-page` and `qindaqt.shell-capture-matrix` (each
  experience with its theme).

## Revisit when

W11s lands (read and test the hint), W15 decides whether choosing a preset
applies its theme, a theme wants more of the title-bar options, KWin regains
native shading, or docks need their own item sets per layout.
