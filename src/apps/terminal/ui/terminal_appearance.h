// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

#include <optional>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::Apps::Terminal {

struct TerminalViewAppearance final {
  QPalette windowPalette;
  QFont interfaceFont;
  QFont terminalFont;
  QColor focusRing;
  QColor statusWarningForeground;
  QColor statusDangerForeground;
  // Terminal surface colors for the colorscheme document below.
  QColor terminalBackground;
  QColor terminalForeground;
  // The sixteen ANSI colors in index order; derived from public QST roles by
  // the adapter, never from hex literals.
  QColor ansi[16];
  QString sourceThemeId;
};

struct AppearanceResult final {
  std::optional<TerminalViewAppearance> appearance;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return appearance.has_value(); }
};

// AGENT-CONTRACT: This adapter consumes only public ThemeSpec and QST-1
// values (see ADR-0013 and the Text Editor precedent). It introduces no
// theme-selection policy, no settings dependency, and no hex literals; a
// complete appearance is returned or nothing is.
class TerminalAppearanceAdapter final {
public:
  [[nodiscard]] static AppearanceResult
  fromTheme(const QindaQt::Themes::ThemeSpec &theme);
};

// Renders the appearance as a Konsole-format .colorscheme document, which the
// qtermwidget adapter installs through its colorscheme loader. Pure function;
// the adapter owns file writing. Intense variants derive with one mechanical
// lighten step because QST-1 publishes no distinct intense roles; this is
// presentation adaptation, not a second token authority.
class TerminalColorSchemeDocument final {
public:
  [[nodiscard]] static QString render(const TerminalViewAppearance &appearance);
};

} // namespace QindaQt::Apps::Terminal
