# claude-w19-decoration-styles handoff

2026-09-24T21:26:00Z

Candidate for plan W19 (more window decoration styles and options) on
`worker/claude-w19-decoration-styles-20260923`, base `b8617122` (round head).
The exact commit is the branch head that contains this message. Code-first
round: nothing was built or run; configure and syntax checks only.

What landed (ADR-0264):

- One data-driven button painter. `DecorationButtonStyle` rows (shape, fill,
  color source, hover treatment, glyph family and ink, size/aspect/spacing,
  side, optional title height, tab title) drive `paintStyledButton`. The four
  shipped styles are rows whose paint calls match the old painters exactly;
  `qindaqt.decoration-button-styles` compares them with a frozen copy in 32
  states at 1x and 2x.
- Eleven new styles: gel, bevel, blue-tiles (XP-like, own colors/shapes/
  glyphs), wide (Win11-like, own), tab (BeOS-like title tab), bold (NeXT-like),
  minimal, pills, dots, outline, chunky. Names avoid vendor marks.
- Glyph set: solid strokes (the dashed pen is gone); triangle = maximize and
  square = restore unchanged.
- One name list (`Themes::DecorationThemeTokens::buttonStyles()`) validated by
  the theme and decoration-document loaders; unknown names still paint lights.
- Eleven Settings1 keys (schema v2, additive, in the arrangement scope):
  window button size/spacing/title height/corners/title weight/app icon/
  roll-up button/title double-click, container button size/spacing/title
  double-click. Containers take the same 15-style list as windows.
- Settings: both Buttons rows are menus; option rows live in
  `TitleBarOptions.qml` (window-only rows built through a Loader);
  `ChromeChoice`/`ChromeMenuChoice` became files so the section stays under
  the QML size limit.
- Existing actions only: the roll-up button and a roll-up double-click emit
  `qindaqtRollUpRequested()` (queued) from the decoration; the compositor
  relays it to `KWinHybridSession::iconifyWindow` (ADR-0203; KWin 6.6 has no
  native shade). Maximize/minimize use KDecoration's requests; a chosen
  double-click accepts KWin's second press. Containers: the router reports a
  title-row double-click and the session runs its existing maximize/restore,
  minimize or wheel roll-up; default `none` keeps the row inert.
- Containers paint named styles through a `ChromeButtonPainter` carried by
  `ChromeStyle` (supplied by the decoration painter), with cell/gap overrides.

What the final build/test run should look at first:

1. `qindaqt.decoration-button-styles` (legacy pixel match, every style at
   1x/2x) and `qindaqt.decoration-title-options`.
2. `qindaqt.appearance-window-decoration-page` (ComboBox menus via
   `indexOfValue`/`activated`, option rows) and `qindaqt.appearance-values`
   (schema/token agreement) — QML warnings are fatal there.
3. `hybrid-chrome.layout`/`.renderer`, `compositor.hybrid-chrome-pointer-router`,
   `qindaqt.theme-formats`, `qindaqt.decoration-theme-formats`,
   `qindaqt.decoration-painter`, `qindaqt.decoration-documents`,
   `qindaqt.member-handle-layout`.
4. The compositor plugin and `org.qindaqt` decoration module link
   (`qindadecorationinput.cpp`, `worn_luna_title.cpp`,
   `container_chrome_style.cpp` are new translation units).

Caveats: no live KWin check of the double-click or roll-up relay (needs a
nested session); the tab style leaves its side strip input-owned by the window
and the shadow follows the full frame; `decoration_painter.h` crossed the
header review threshold (370/400). Renders for Jarrod were not produced
(no builds in this round). Next action: the round's combined build.
