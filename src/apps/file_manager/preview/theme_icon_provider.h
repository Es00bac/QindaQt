// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickImageProvider>

namespace QindaQt::Apps::FileManager {

// ADR-0116 presentation seam: stock Qt Quick has no themed-icon item, so the
// engine-owned "theme-icons" provider resolves public icon names through
// QIcon::fromTheme — the same platform-theme channel (ADR-0115) that supplies
// palette and fonts. Presentation only; no filesystem or session authority.
class ThemeIconProvider final : public QQuickImageProvider {
public:
  // Bounded decode target regardless of the requested delegate size.
  static constexpr int maximumIconPixels = 256;

  ThemeIconProvider();

  // id is a freedesktop icon name (for example "folder" or
  // "list-add-symbolic"). An unresolved name returns a valid transparent
  // pixmap rather than a null one: a null result makes QML Image emit a
  // qWarning, which the offscreen QT_FATAL_WARNINGS=1 rows treat as fatal.
  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override;
};

} // namespace QindaQt::Apps::FileManager
