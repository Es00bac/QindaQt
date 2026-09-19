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
  // "list-add-symbolic"), optionally followed by a "?color=#rrggbb" query.
  // A resolved "-symbolic" name is monochrome by convention and is tinted:
  // with the query color when it is valid (callers pass a palette color so a
  // live theme change re-resolves the URL), otherwise with the application
  // palette's WindowText. Without the tint the symbolic glyphs keep their
  // authored dark stroke and vanish on dark themes. An unresolved name
  // returns a valid transparent pixmap rather than a null one: a null result
  // makes QML Image emit a qWarning, which the offscreen QT_FATAL_WARNINGS=1
  // rows treat as fatal.
  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override;
};

} // namespace QindaQt::Apps::FileManager
