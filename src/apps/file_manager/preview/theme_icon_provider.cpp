// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_icon_provider.h"

#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QUrlQuery>

#include <algorithm>

namespace QindaQt::Apps::FileManager {
namespace {

// Same contract as the shell icon provider: only the opaque #rrggbb form is a
// recolor target; anything else is ignored rather than guessed.
[[nodiscard]] QColor parseTint(const QString &value) {
  if (value.size() != 7 || !value.startsWith(QLatin1Char('#'))) {
    return {};
  }
  const QColor color(value);
  return color.isValid() ? color : QColor();
}

} // namespace

ThemeIconProvider::ThemeIconProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Pixmap) {}

QPixmap ThemeIconProvider::requestPixmap(const QString &id, QSize *size,
                                         const QSize &requestedSize) {
  const int requested = requestedSize.isValid()
      ? std::max(requestedSize.width(), requestedSize.height())
      : 64;
  const int edge = std::clamp(requested, 1,
                              static_cast<int>(maximumIconPixels));
  const qsizetype queryStart = id.indexOf(QLatin1Char('?'));
  const QString name = queryStart < 0 ? id : id.left(queryStart);
  QColor tint;
  if (queryStart >= 0) {
    tint = parseTint(QUrlQuery(id.mid(queryStart + 1))
                         .queryItemValue(QStringLiteral("color")));
  }
  QIcon icon = QIcon::fromTheme(name);
  // Only a resolved -symbolic request is monochrome by convention and may be
  // recolored; full-color icons and the octet-stream fallback keep their own
  // pixels.
  const bool symbolic = !icon.isNull() && name.endsWith(QLatin1String("-symbolic"));
  if (icon.isNull()) {
    icon = QIcon::fromTheme(QStringLiteral("application-octet-stream"));
  }
  QPixmap pixmap = icon.pixmap(QSize(edge, edge));
  if (pixmap.isNull()) {
    pixmap = QPixmap(QSize(edge, edge));
    pixmap.fill(Qt::transparent);
  }
  if (symbolic && !pixmap.isNull()) {
    if (!tint.isValid()) {
      tint = QGuiApplication::palette().color(QPalette::WindowText);
    }
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), tint);
    painter.end();
  }
  if (size) {
    *size = pixmap.size();
  }
  return pixmap;
}

} // namespace QindaQt::Apps::FileManager
