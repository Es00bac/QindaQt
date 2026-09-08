// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFont>
#include <QPalette>

#include <optional>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::Apps::SystemMonitor {

struct SystemMonitorAppearance final {
  QPalette palette;
  QFont interfaceFont;
};

struct AppearanceResult final {
  std::optional<SystemMonitorAppearance> appearance;
  QString diagnostic;
  [[nodiscard]] bool ok() const { return appearance.has_value(); }
};

// Converts QST semantic tokens to Qt Widgets roles. Charts consume palette
// roles, never a product-specific accent color.
class SystemMonitorAppearanceAdapter final {
public:
  [[nodiscard]] static AppearanceResult
  fromTheme(const QindaQt::Themes::ThemeSpec &theme);
};

} // namespace QindaQt::Apps::SystemMonitor
