// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"
#include "ui/terminal_ansi_palette.h"

#include <QAccessibilityHints>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStyleHints>

namespace QindaQt::Apps::Terminal {
namespace {

QString colorArgument(const QColor &color) {
  return QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(
      color.blue());
}

void appendSection(QString &document, const char *name, const QColor &color) {
  document += QStringLiteral("[%1]\nColor=%2\n")
                  .arg(QLatin1String(name), colorArgument(color));
}

} // namespace

TerminalViewAppearance TerminalAppearanceAdapter::derive(
    const QPalette &applicationPalette, TerminalContentScheme scheme,
    bool highContrast, const QFont &monospaceFont) {
  QColor background;
  QColor foreground;
  switch (scheme) {
  case TerminalContentScheme::Light:
    background = QColor(Qt::white);
    foreground = QColor(Qt::black);
    break;
  case TerminalContentScheme::Dark:
    background = QColor(Qt::black);
    foreground = QColor(Qt::white);
    break;
  case TerminalContentScheme::System:
    background = applicationPalette.color(QPalette::Base);
    foreground = applicationPalette.color(QPalette::Text);
    break;
  }
  const double contrast = highContrast ? 7.0 : 4.5;
  background.setAlpha(255);
  foreground = terminalReadableColor(foreground, background, contrast);
  QFont terminalFont(monospaceFont);
  terminalFont.setStyleHint(QFont::Monospace);
  terminalFont.setFixedPitch(true);
  TerminalViewAppearance appearance{
      .terminalFont = terminalFont,
      .terminalBackground = background,
      .terminalForeground = foreground,
      .ansi = {},
      .schemeId = terminalContentSchemeId(scheme),
      .highContrast = highContrast,
  };
  const auto ansi = terminalAnsiPalette(background, highContrast);
  for (int index = 0; index < 16; ++index)
    appearance.ansi[index] = ansi[static_cast<std::size_t>(index)];
  return appearance;
}

bool terminalPlatformHighContrast() {
  if (auto *hints = QGuiApplication::styleHints()->accessibility()) {
    return hints->contrastPreference() ==
           Qt::ContrastPreference::HighContrast;
  }
  return false;
}

TerminalViewAppearance terminalDesktopContentAppearance() {
  return TerminalAppearanceAdapter::derive(
      QGuiApplication::palette(), TerminalContentScheme::System,
      terminalPlatformHighContrast(),
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

QString TerminalColorSchemeDocument::render(
    const TerminalViewAppearance &appearance) {
  QString document;
  appendSection(document, "Background", appearance.terminalBackground);
  appendSection(document, "BackgroundIntense",
                appearance.terminalBackground);
  appendSection(document, "Foreground", appearance.terminalForeground);
  appendSection(document, "ForegroundIntense",
                appearance.terminalForeground);
  // AGENT-CONTRACT (qtermwidget 2.4 ColorScheme::colorNames): Konsole scheme
  // files encode bright ANSI slots as Color0Intense..Color7Intense. Groups
  // named Color8..Color15 are ignored and fall back to upstream defaults.
  for (int index = 0; index < 8; ++index) {
    const QByteArray name = QStringLiteral("Color%1").arg(index).toLatin1();
    appendSection(document, name.constData(), appearance.ansi[index]);
  }
  for (int index = 0; index < 8; ++index) {
    const QByteArray name =
        QStringLiteral("Color%1Intense").arg(index).toLatin1();
    appendSection(document, name.constData(), appearance.ansi[index + 8]);
  }
  document += QStringLiteral(
      "[General]\nDescription=QindaQt generated scheme\nOpacity=1\n");
  return document;
}

} // namespace QindaQt::Apps::Terminal
