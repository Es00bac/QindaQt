// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Apps::Terminal {
namespace {

QString colorArgument(const QColor &color) {
  return QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(
      color.blue());
}

QColor intenseVariant(const QColor &color) {
  // One mechanical derivation for the eight "bright" ANSI slots; kept
  // deliberate and documented rather than silently invented per app.
  return color.lighter(125);
}

void appendSection(QString &document, const char *name, const QColor &color) {
  document += QStringLiteral("[%1]\nColor=%2\n")
                  .arg(QLatin1String(name), colorArgument(color));
}

} // namespace

AppearanceResult TerminalAppearanceAdapter::fromTheme(
    const QindaQt::Themes::ThemeSpec &theme) {
  QindaQt::DesignTokens::AccessibilityInputs accessibility;
  accessibility.highContrast = theme.variant == QStringLiteral("high-contrast");
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

  // ANSI mapping is a bounded adaptation of public QST roles (ADR-0030):
  // QST publishes red/green/yellow/blue semantics and no distinct magenta or
  // cyan hue, so magenta maps to the accent's subtle role and every "bright"
  // slot uses the same mechanical lighten step. A full palette profile is a
  // later, settings-backed slice and is not silently invented here.
  TerminalViewAppearance appearance{
      .windowPalette = palette,
      .interfaceFont = interfaceFont,
      .terminalFont = terminalFont,
      .focusRing = tokens.focusRing(),
      .statusWarningForeground = tokens.status().warning.foreground,
      .statusDangerForeground = tokens.danger().defaultColor,
      .terminalBackground = tokens.background().base,
      .terminalForeground = tokens.foreground().defaultColor,
      .ansi = {},
      .sourceThemeId = tokens.sourceThemeId(),
  };
  const QColor ansiBase[8] = {
      tokens.background().base,          // 0 black
      tokens.danger().defaultColor,      // 1 red
      tokens.status().success.foreground, // 2 green
      tokens.status().warning.foreground, // 3 yellow
      tokens.accent().defaultColor,      // 4 blue
      tokens.accent().subtle,            // 5 magenta (no QST hue; see above)
      tokens.status().info.foreground,   // 6 cyan
      tokens.foreground().defaultColor,  // 7 white
  };
  for (int index = 0; index < 8; ++index) {
    appearance.ansi[index] = ansiBase[index];
    appearance.ansi[index + 8] = intenseVariant(ansiBase[index]);
  }
  return {.appearance = appearance, .diagnostic = {}};
}

QString TerminalColorSchemeDocument::render(
    const TerminalViewAppearance &appearance) {
  QString document;
  appendSection(document, "Background", appearance.terminalBackground);
  appendSection(document, "BackgroundIntense",
                intenseVariant(appearance.terminalBackground));
  appendSection(document, "Foreground", appearance.terminalForeground);
  appendSection(document, "ForegroundIntense",
                intenseVariant(appearance.terminalForeground));
  static constexpr const char *kColorNames[16] = {
      "Color0",  "Color1",  "Color2",  "Color3",
      "Color4",  "Color5",  "Color6",  "Color7",
      "Color8",  "Color9",  "Color10", "Color11",
      "Color12", "Color13", "Color14", "Color15",
  };
  for (int index = 0; index < 16; ++index) {
    appendSection(document, kColorNames[index], appearance.ansi[index]);
  }
  return document;
}

} // namespace QindaQt::Apps::Terminal
