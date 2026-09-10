// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

#include "profiles/terminal_profile.h"

namespace QindaQt::Apps::Terminal {

// Content-only appearance for one terminal surface. Chrome roles (window
// palette, interface font, focus ring, QSS) were retired by ADR-0116: they
// belong to the Qt platform theme and are deliberately absent here.
struct TerminalViewAppearance final {
  QFont terminalFont;
  QColor terminalBackground;
  QColor terminalForeground;
  // ANSI is a terminal protocol palette, independently contrast-fitted to
  // the opaque content surface (ADR-0112).
  QColor ansi[16];
  QString schemeId;
  bool highContrast = false;
};

// Derives the terminal content appearance from the active application palette
// plus the selected scheme and the platform high-contrast hint — never from
// QST tokens. System follows the palette's Base/Text roles; Light and Dark
// are the explicit content choices a profile can pin. Pure function.
class TerminalAppearanceAdapter final {
public:
  [[nodiscard]] static TerminalViewAppearance
  derive(const QPalette &applicationPalette, TerminalContentScheme scheme,
         bool highContrast, const QFont &monospaceFont);
};

// Live platform state readers (ADR-0115): the platform theme's high-contrast
// preference and the System-scheme content appearance derived from the active
// QGuiApplication palette and FixedFont. GUI-thread only.
[[nodiscard]] bool terminalPlatformHighContrast();
[[nodiscard]] TerminalViewAppearance terminalDesktopContentAppearance();

// Renders the appearance as a Konsole-format .colorscheme document, which the
// qtermwidget adapter installs through its colorscheme loader. Pure function;
// the adapter owns file writing. ANSI hues and their intense partners are
// independently contrast-fitted; default intense text keeps the default surface.
class TerminalColorSchemeDocument final {
public:
  [[nodiscard]] static QString render(const TerminalViewAppearance &appearance);
};

} // namespace QindaQt::Apps::Terminal
