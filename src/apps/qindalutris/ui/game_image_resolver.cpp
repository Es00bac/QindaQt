// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_image_resolver.h"

#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QStandardPaths>

namespace QindaQt::QindaLutris {

QImage resolveGameImage(const Game &game) {
  if (!game.coverPath.isEmpty()) {
    QImageReader reader(game.coverPath);
    reader.setAllocationLimit(64); // MiB; hostile art fails, not the session
    reader.setScaledSize(QSize(512, 512));
    return reader.read();
  }
  if (!game.iconName.isEmpty()) {
    const QIcon icon = QIcon::fromTheme(game.iconName);
    if (!icon.isNull()) {
      return icon.pixmap(256, 256).toImage();
    }
    // AGENT-NOTE: a session with no platform icon theme (bare offscreen
    // runs, minimal sessions) leaves QIcon::fromTheme with nowhere to look.
    // The bounded fallback below checks fixed hicolor/pixmaps paths by name;
    // it never walks a directory and never follows a symlink.
    for (const QString &base : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
      for (const QLatin1String size :
           {QLatin1String("256x256"), QLatin1String("128x128"),
            QLatin1String("64x64"), QLatin1String("48x48"),
            QLatin1String("scalable")}) {
        for (const QLatin1String ext :
             {QLatin1String("png"), QLatin1String("svg")}) {
          const QString candidate = base
              + QStringLiteral("/icons/hicolor/") + size
              + QStringLiteral("/apps/") + game.iconName + QLatin1Char('.') + ext;
          const QFileInfo info(candidate);
          if (info.isFile() && !info.isSymLink()
              && info.size() < qint64(16) * 1024 * 1024) {
            QImageReader reader(candidate);
            reader.setAllocationLimit(64);
            reader.setScaledSize(QSize(512, 512));
            const QImage image = reader.read();
            if (!image.isNull()) {
              return image;
            }
          }
        }
      }
    }
  }
  return {};
}

} // namespace QindaQt::QindaLutris
