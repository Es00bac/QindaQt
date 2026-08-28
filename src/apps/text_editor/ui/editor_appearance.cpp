// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_appearance.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Apps::TextEditor {

AppearanceResult
EditorAppearanceAdapter::fromTheme(const QindaQt::Themes::ThemeSpec &theme) {
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
  palette.setColor(QPalette::Button, tokens.background().raised);
  palette.setColor(QPalette::ButtonText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::BrightText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::Light, tokens.background().highest);
  palette.setColor(QPalette::Midlight, tokens.background().raised);
  palette.setColor(QPalette::Dark, tokens.strongOutline());
  palette.setColor(QPalette::Mid, tokens.divider());
  palette.setColor(QPalette::Shadow, tokens.strongOutline());
  palette.setColor(QPalette::Highlight, tokens.accent().defaultColor);
  palette.setColor(QPalette::HighlightedText, tokens.accent().foreground);
  palette.setColor(QPalette::ToolTipBase, tokens.background().highest);
  palette.setColor(QPalette::ToolTipText, tokens.foreground().defaultColor);
  palette.setColor(QPalette::PlaceholderText, tokens.foreground().muted);
  palette.setColor(QPalette::Link, tokens.accent().defaultColor);
  // QST-1 intentionally has one link/accent semantic; the plain-text editor
  // does not invent a visited-link color that a later rich-text consumer
  // could mistake for a shared token.
  palette.setColor(QPalette::LinkVisited, tokens.accent().defaultColor);
  palette.setColor(QPalette::Disabled, QPalette::Text,
                   tokens.foreground().disabled);
  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                   tokens.foreground().disabled);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   tokens.foreground().disabled);

  QFont interfaceFont(tokens.typeScale().fontFamily);
  interfaceFont.setPointSizeF(tokens.typeScale().body);
  QFont editorFont(tokens.typeScale().monoFontFamily);
  editorFont.setPointSizeF(tokens.typeScale().body);
  editorFont.setStyleHint(QFont::Monospace);
  editorFont.setFixedPitch(true);

  return {.appearance =
              EditorAppearance{
                  .palette = palette,
                  .interfaceFont = interfaceFont,
                  .editorFont = editorFont,
                  .focusRing = tokens.focusRing(),
                  .warningBackground = tokens.status().warning.background,
                  .warningForeground = tokens.status().warning.foreground,
                  .dangerBackground = tokens.danger().defaultColor,
                  .dangerForeground = tokens.danger().foreground,
                  .mediumRadius = tokens.radius().medium,
                  .sourceThemeId = tokens.sourceThemeId(),
              },
          .diagnostic = {}};
}

} // namespace QindaQt::Apps::TextEditor
