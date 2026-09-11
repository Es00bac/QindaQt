// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeappearancepalette.h"

#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Compositor::KWinIntegration {

HybridChrome::ChromePalette
chromePaletteForTheme(const Themes::ThemeSpec &theme) {
  const auto color = [&theme](const char *key, QColor fallback) {
    const auto candidate = theme.colors.value(QString::fromLatin1(key));
    return candidate.isValid() ? candidate : fallback;
  };
  HybridChrome::ChromePalette palette;
  palette.surface = color("surface", palette.surface);
  palette.surfaceRaised = color("surfaceRaised", palette.surfaceRaised);
  palette.border = color("border", palette.border);
  palette.text = color("text", palette.text);
  palette.textMuted = color("textMuted", palette.textMuted);
  palette.accent = color("accent", palette.accent);
  palette.close = theme.decoration.closeColor;
  palette.minimize = theme.decoration.minimizeColor;
  palette.maximize = theme.decoration.maximizeColor;
  return palette;
}

QPalette nativePaletteForTheme(const Themes::ThemeSpec &theme) {
  const auto color = [&theme](const char *key, QColor fallback) {
    const auto candidate = theme.colors.value(QString::fromLatin1(key));
    return candidate.isValid() ? candidate : fallback;
  };
  QPalette palette;
  const QColor surface = color("surface", palette.color(QPalette::Window));
  const QColor raised = color("surfaceRaised", palette.color(QPalette::Base));
  const QColor text = color("text", palette.color(QPalette::WindowText));
  const QColor muted =
      color("textMuted", palette.color(QPalette::PlaceholderText));
  const QColor accent = color("accent", palette.color(QPalette::Highlight));
  const QColor accentText =
      color("accentText", palette.color(QPalette::HighlightedText));
  palette.setColor(QPalette::Window, surface);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, raised);
  palette.setColor(QPalette::AlternateBase, surface);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::Button, raised);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::Highlight, accent);
  palette.setColor(QPalette::HighlightedText, accentText);
  palette.setColor(QPalette::PlaceholderText, muted);
  palette.setColor(QPalette::Disabled, QPalette::Text, muted);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, muted);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
  return palette;
}

QVariantMap
decorationPaletteProperties(const HybridChrome::ChromePalette &palette,
                            const Themes::ThemeSpec &theme) {
  QVariantMap properties = {{QStringLiteral("surface"), palette.surface},
          {QStringLiteral("surfaceRaised"), palette.surfaceRaised},
          {QStringLiteral("border"), palette.border},
          {QStringLiteral("text"), palette.text},
          {QStringLiteral("textMuted"), palette.textMuted},
          {QStringLiteral("close"), palette.close},
          {QStringLiteral("minimize"), palette.minimize},
          {QStringLiteral("maximize"), palette.maximize}};
  // QindaDecoration switches to the worn Luna chrome only when the theme
  // authors it; omitting the keys keeps the classic rendering byte-identical.
  properties.insert(QStringLiteral("buttonStyle"), theme.decoration.buttonStyle);
  if (theme.decoration.titleBarColor.isValid()) {
    properties.insert(QStringLiteral("titleBar"),
                      theme.decoration.titleBarColor);
  }
  if (theme.decoration.titleBarInactiveColor.isValid()) {
    properties.insert(QStringLiteral("titleBarInactive"),
                      theme.decoration.titleBarInactiveColor);
  }
  if (theme.decoration.restoreColor.isValid()) {
    properties.insert(QStringLiteral("restore"),
                      theme.decoration.restoreColor);
  }
  return properties;
}

} // namespace QindaQt::Compositor::KWinIntegration
