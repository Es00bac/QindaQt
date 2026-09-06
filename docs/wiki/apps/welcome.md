# Welcome to QindaQt

`qindaqt-welcome` is the responsive first-run guide and an ordinary launcher
application. Its 900x640 initial window shrinks to 640x480: wide windows use a
seven-chapter left rail and compact windows replace it with a chapter header.
Each chapter combines short task cards, a small QML diagram, and relevant
shortcut pills instead of presenting one long page of prose.

## First launch preference

The application owns one local `QSettings` value,
`welcome/showAtNextLaunch`, under organization `QindaQt` and application
`qindaqt-welcome`. It defaults to `true`. The exact **Show at next launch**
checkbox writes it immediately.

After the first shell process starts, `qindaqt-session` launches the installed
sibling `qindaqt-welcome --first-launch` as an optional tracked child. That mode
checks the preference before theme discovery or QML construction and exits
successfully when it is false. The optional process never participates in shell
readiness or session failure policy and is not restarted. It remains bound to
the supervisor's lifetime and is stopped before essential desktop children
during orderly teardown. A manual desktop-entry launch omits `--first-launch`
and always opens the guide, including after opt-out.

The guide never writes Settings1. It consumes a public Settings1 client scoped
only to `appearance.theme` and `appearance.colorScheme`, then delegates
selection to the shared application-appearance controller. The controller
publishes a complete QST-1 generation before the root QML is constructed and
re-publishes on confirmed appearance changes. QindaQt.Controls supplies every
interactive control; diagrams and surfaces use semantic tokens only. The
application palette and font are derived from the same confirmed theme, so
native Quick Controls also follow light, dark, system, and high-contrast
selection.

The optional Qinda Punk hero is discovered through XDG generic-data roots and
the relocatable prefix beside the executable. Missing artwork removes that
illustration without putting text over a broken or unreadable image.

## Guide content

The seven chapters cover:

1. a short orientation to familiar independent windows and optional groups;
2. the launcher, dock, global application menu, and ordinary floating
   controls;
3. exact `Meta+Shift` left-drag arrangement, with an edge creating a split,
   the center or tab strip creating a desktop page, and divider dragging
   resizing the two sides; dropping outside all targets leaves an independent
   window independent and detaches a grouped member;
4. the difference between a top-level desktop page and document tabs inside an
   application, plus recursive splits and page keyboard navigation;
5. whole-group movement through the outer title, member detach through the
   preserved member title, page reorder/move/detach, and the group menu;
6. preset selection and reversible Customize drafts, distinguishing a cancelled
   move from whole-draft Discard and covering apply, undo, pointer, and keyboard
   paths; and
7. wallpaper, light/dark/system appearance, accessibility choices, and manual
   reopening.

The practical buttons use a closed allowlist to launch the real Appearance and
Customize Settings routes, Text Editor, and File Manager. The interaction
descriptions follow [Window containers](../architecture/window-containers.md);
they do not claim that dropping a tab on an edge creates a split.

## Verification

`qindaqt.welcome-preferences` proves the default and persisted local opt-out.
`qindaqt.welcome-first-launch-lifecycle` constructs the real compiled QML root
under a fresh isolated configuration, verifies its responsive geometry and
controls, unchecks the real checkbox, proves a later `--first-launch` exits
before opening, and proves a manual launch still opens. The session-supervisor
test proves Welcome starts only after an essential shell exists, receives the
exact `--first-launch` argument, does not restart after exit, and cannot end the
session.

Ordinary private desktop rows seed the Welcome preference to false so the new
first-run surface cannot obscure their established interaction screenshots.
`DesktopVirtual` stages the executable and desktop entry; a fresh interactive
first-run capture remains the visual review boundary.
