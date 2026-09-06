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

QVariantMap
decorationPaletteProperties(const HybridChrome::ChromePalette &palette) {
  return {{QStringLiteral("surface"), palette.surface},
          {QStringLiteral("surfaceRaised"), palette.surfaceRaised},
          {QStringLiteral("border"), palette.border},
          {QStringLiteral("text"), palette.text},
          {QStringLiteral("textMuted"), palette.textMuted},
          {QStringLiteral("close"), palette.close},
          {QStringLiteral("minimize"), palette.minimize},
          {QStringLiteral("maximize"), palette.maximize}};
}

} // namespace QindaQt::Compositor::KWinIntegration
