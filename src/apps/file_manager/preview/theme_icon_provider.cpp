// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_icon_provider.h"

#include <QIcon>
#include <QPixmap>

#include <algorithm>

namespace QindaQt::Apps::FileManager {

ThemeIconProvider::ThemeIconProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Pixmap) {}

QPixmap ThemeIconProvider::requestPixmap(const QString &id, QSize *size,
                                         const QSize &requestedSize) {
  const int requested = requestedSize.isValid()
      ? std::max(requestedSize.width(), requestedSize.height())
      : 64;
  const int edge = std::clamp(requested, 1,
                              static_cast<int>(maximumIconPixels));
  const QIcon icon = QIcon::fromTheme(
      id, QIcon::fromTheme(QStringLiteral("application-octet-stream")));
  QPixmap pixmap = icon.pixmap(QSize(edge, edge));
  if (pixmap.isNull()) {
    pixmap = QPixmap(QSize(edge, edge));
    pixmap.fill(Qt::transparent);
  }
  if (size) {
    *size = pixmap.size();
  }
  return pixmap;
}

} // namespace QindaQt::Apps::FileManager
