# Chrome appearance candidate review — 2026-09-06T03:18:36Z

- Candidate: `390d7ddcf9c07c8f7191fd4ac713361557810232` in `.cache/chrome-appearance`, base `a7d92f2d`.
- Status: ACCEPT, pending integrated compile/focused gates.
- The compositor owns exactly one AppAppearance controller/Settings subscription, maps validated ThemeSpec colors into a ChromePalette, synchronously publishes the palette to current managed decorations, and refreshes grouped plans through `KWinHybridSession::setChromePalette`. New windows and decoration recreation are covered by registry observation plus `decorationChanged` republish.
- `QindaDecoration` uses the actual KDecoration3 public API (`DecoratedWindow::palette()`, `color()`, `paletteChanged`) as fallback, consumes the compositor-owned `qindaqtChromePalette` property when present, repaints on palette changes, and derives button glyph foregrounds from button fill luminance. No new service/protocol or per-window Settings client was introduced.
- Theme conversion tests cover qinda-light, qinda-dark, and high-contrast semantic palettes with contrast assertions; no concrete source blocker found.
