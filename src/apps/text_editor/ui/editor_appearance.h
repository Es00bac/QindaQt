// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

#include <optional>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::Apps::TextEditor {

struct EditorAppearance final {
  QPalette palette;
  QFont interfaceFont;
  QFont editorFont;
  QColor focusRing;
  QColor warningBackground;
  QColor warningForeground;
  QColor dangerBackground;
  QColor dangerForeground;
  double mediumRadius = 0.0;
  QString sourceThemeId;
};

struct AppearanceResult final {
  std::optional<EditorAppearance> appearance;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return appearance.has_value(); }
};

// AGENT-CONTRACT: This adapter consumes only public ThemeSpec and QST-1 values.
// It has no theme-selection, settings, persistence, or widget ownership. A
// complete appearance is returned or the caller keeps its prior palette.
class EditorAppearanceAdapter final {
public:
  [[nodiscard]] static AppearanceResult
  fromTheme(const QindaQt::Themes::ThemeSpec &theme);
};

} // namespace QindaQt::Apps::TextEditor
