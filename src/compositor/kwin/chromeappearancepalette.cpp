// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeappearancepalette.h"

#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Compositor::KWinIntegration {

HybridChrome::ChromePalette
chromePaletteForTheme(const Themes::ThemeSpec &theme) {
  // AGENT-CONTRACT: one derivation shared with the Settings preview
  // (ADR-0127); the compositor never re-maps theme colors on its own.
  return Decoration::chromePaletteForTheme(theme);
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
  return Decoration::DecorationChrome::fromChromePalette(palette, theme)
      .toVariantMap();
}

QVariantMap
decorationPaletteProperties(const HybridChrome::ChromePalette &palette,
                            const Themes::ThemeSpec &theme,
                            const Decoration::ChromePreferences &preferences) {
  return Decoration::applyWindowPreferences(
             Decoration::DecorationChrome::fromChromePalette(palette, theme),
             preferences)
      .toVariantMap();
}

} // namespace QindaQt::Compositor::KWinIntegration
