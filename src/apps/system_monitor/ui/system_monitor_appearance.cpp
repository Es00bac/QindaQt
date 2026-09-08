// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_appearance.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Apps::SystemMonitor {

AppearanceResult SystemMonitorAppearanceAdapter::fromTheme(
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
  palette.setColor(QPalette::Base, tokens.background().raised);
  palette.setColor(QPalette::AlternateBase, tokens.background().highest);
  palette.setColor(QPalette::Text, tokens.foreground().defaultColor);
  palette.setColor(QPalette::PlaceholderText, tokens.foreground().muted);
  palette.setColor(QPalette::Button, tokens.background().raised);
  palette.setColor(QPalette::ButtonText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::Mid, tokens.divider());
  palette.setColor(QPalette::Highlight, tokens.accent().defaultColor);
  palette.setColor(QPalette::HighlightedText, tokens.accent().foreground);
  palette.setColor(QPalette::Link, tokens.accent().defaultColor);
  palette.setColor(QPalette::LinkVisited, tokens.accent().defaultColor);
  palette.setColor(QPalette::Disabled, QPalette::Text,
                   tokens.foreground().disabled);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   tokens.foreground().disabled);

  QFont font(tokens.typeScale().fontFamily);
  font.setPointSizeF(tokens.typeScale().body);
  return {.appearance = SystemMonitorAppearance{.palette = palette,
                                                .interfaceFont = font},
          .diagnostic = {}};
}

} // namespace QindaQt::Apps::SystemMonitor
