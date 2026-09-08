// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

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
  // ANSI is a terminal protocol palette, independently contrast-fitted to
  // the opaque QST content surface (ADR-0101).
  QColor ansi[16];
  QString sourceThemeId;
  QString chromeStyleSheet;
  bool highContrast = false;
  double textScale = 1.0;
};

struct AppearanceResult final {
  std::optional<TerminalViewAppearance> appearance;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return appearance.has_value(); }
};

// AGENT-CONTRACT: This adapter consumes only public ThemeSpec and QST-1
// values (see ADR-0013 and the Text Editor precedent). It introduces no
// theme-selection policy or settings dependency; a
// complete appearance is returned or nothing is.
class TerminalAppearanceAdapter final {
public:
  [[nodiscard]] static AppearanceResult
  fromTheme(const QindaQt::Themes::ThemeSpec &theme,
            QindaQt::DesignTokens::AccessibilityInputs inputs = {});
};

// Renders the appearance as a Konsole-format .colorscheme document, which the
// qtermwidget adapter installs through its colorscheme loader. Pure function;
// the adapter owns file writing. ANSI hues and their intense partners are
// independently contrast-fitted; default intense text keeps the default surface.
class TerminalColorSchemeDocument final {
public:
  [[nodiscard]] static QString render(const TerminalViewAppearance &appearance);
};

} // namespace QindaQt::Apps::Terminal
