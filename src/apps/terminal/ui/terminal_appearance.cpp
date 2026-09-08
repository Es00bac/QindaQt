// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"
#include "ui/terminal_chrome.h"
#include "ui/terminal_ansi_palette.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

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

AppearanceResult TerminalAppearanceAdapter::fromTheme(
    const QindaQt::Themes::ThemeSpec &theme,
    QindaQt::DesignTokens::AccessibilityInputs accessibility) {
  accessibility.highContrast = accessibility.highContrast ||
      theme.variant == QStringLiteral("high-contrast");
  const auto derived =
      QindaQt::DesignTokens::DesignTokenDeriver::derive(theme, accessibility);
  if (!derived.ok()) {
    return {.appearance = std::nullopt, .diagnostic = derived.diagnostic};
  }
  const auto &tokens = *derived.tokens;

  QPalette palette;
  palette.setColor(QPalette::Window, tokens.background().base);
  palette.setColor(QPalette::WindowText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::Base, tokens.background().base);
  palette.setColor(QPalette::AlternateBase, tokens.background().raised);
  palette.setColor(QPalette::PlaceholderText, tokens.foreground().muted);
  palette.setColor(QPalette::Light, tokens.background().highest);
  palette.setColor(QPalette::Midlight, tokens.background().raised);
  palette.setColor(QPalette::Mid, tokens.strongOutline());
  palette.setColor(QPalette::Dark, tokens.strongOutline());
  palette.setColor(QPalette::Shadow, tokens.strongOutline());
  palette.setColor(QPalette::Text, tokens.foreground().defaultColor);
  palette.setColor(QPalette::Button, tokens.background().raised);
  palette.setColor(QPalette::ButtonText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::Highlight, tokens.accent().defaultColor);
  palette.setColor(QPalette::HighlightedText, tokens.accent().foreground);
  palette.setColor(QPalette::ToolTipBase, tokens.background().highest);
  palette.setColor(QPalette::ToolTipText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::Disabled, QPalette::Text,
                   tokens.foreground().disabled);
  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                   tokens.foreground().disabled);

  QFont interfaceFont(tokens.typeScale().fontFamily);
  interfaceFont.setPointSizeF(tokens.typeScale().body);
  QFont terminalFont(tokens.typeScale().monoFontFamily);
  terminalFont.setPointSizeF(tokens.typeScale().body);
  terminalFont.setStyleHint(QFont::Monospace);
  terminalFont.setFixedPitch(true);

  TerminalViewAppearance appearance{
      .windowPalette = palette,
      .interfaceFont = interfaceFont,
      .terminalFont = terminalFont,
      .focusRing = tokens.focusRing(),
      .statusWarningForeground = terminalReadableColor(
          tokens.status().warning.foreground, tokens.background().raised,
          accessibility.highContrast ? 7.0 : 4.5),
      .statusDangerForeground = terminalReadableColor(
          tokens.danger().defaultColor, tokens.background().raised,
          accessibility.highContrast ? 7.0 : 4.5),
      .terminalBackground = tokens.background().base,
      .terminalForeground = tokens.foreground().defaultColor,
      .ansi = {},
      .sourceThemeId = tokens.sourceThemeId(),
      .chromeStyleSheet = terminalChromeStyleSheet(tokens),
      .highContrast = accessibility.highContrast,
      .textScale = tokens.inputs().textScale,
  };
  appearance.terminalBackground.setAlpha(255);
  appearance.terminalForeground = terminalReadableColor(
      appearance.terminalForeground, appearance.terminalBackground,
      accessibility.highContrast ? 7.0 : 4.5);
  const auto ansi = terminalAnsiPalette(appearance.terminalBackground,
                                       accessibility.highContrast);
  for (int index = 0; index < 16; ++index)
    appearance.ansi[index] = ansi[static_cast<std::size_t>(index)];
  return {.appearance = appearance, .diagnostic = {}};
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
