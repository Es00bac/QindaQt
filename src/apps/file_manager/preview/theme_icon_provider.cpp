// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_icon_provider.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMetaObject>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSemaphore>
#include <QThread>
#include <QUrlQuery>

#include <algorithm>
#include <memory>

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

// AGENT-GUARD: Qt Quick calls requestPixmap() on its image-reader thread for
// asynchronous Images, but QIcon's theme lookup and icon engines share global,
// unsynchronized caches with the GUI thread. A live crash (2026-09-28) had the
// reader thread destroying an icon while the GUI thread parsed another SVG.
// Every QIcon and palette use therefore runs on the GUI thread; the reader
// waits a bounded time, so an exiting application never deadlocks with it.
constexpr int guiThreadWaitMs = 2000;

struct IconRequest final {
  QString name;
  QColor tint;
  int edge = 64;
};

// GUI thread only.
[[nodiscard]] QImage renderThemeIcon(const IconRequest &request) {
  QIcon icon = QIcon::fromTheme(request.name);
  // Only a resolved -symbolic request is monochrome by convention and may be
  // recolored; full-color icons and the octet-stream fallback keep their own
  // pixels.
  const bool symbolic =
      !icon.isNull() && request.name.endsWith(QLatin1String("-symbolic"));
  if (icon.isNull()) {
    icon = QIcon::fromTheme(QStringLiteral("application-octet-stream"));
  }
  QPixmap pixmap = icon.pixmap(QSize(request.edge, request.edge));
  if (pixmap.isNull()) {
    return {};
  }
  if (symbolic) {
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(),
                     request.tint.isValid()
                         ? request.tint
                         : QGuiApplication::palette().color(QPalette::WindowText));
    painter.end();
  }
  return pixmap.toImage();
}

[[nodiscard]] QImage renderOnGuiThread(const IconRequest &request) {
  QCoreApplication *const application = QCoreApplication::instance();
  if (application == nullptr || QThread::currentThread() == application->thread()) {
    return renderThemeIcon(request);
  }
  struct Handoff final {
    QSemaphore done;
    QImage image;
  };
  const auto handoff = std::make_shared<Handoff>();
  static_cast<void>(QMetaObject::invokeMethod(
      application,
      [handoff, request] {
        handoff->image = renderThemeIcon(request);
        handoff->done.release();
      },
      Qt::QueuedConnection));
  if (!handoff->done.tryAcquire(1, guiThreadWaitMs)) {
    return {};
  }
  return handoff->image;
}

} // namespace

ThemeIconProvider::ThemeIconProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Pixmap) {}

QPixmap ThemeIconProvider::requestPixmap(const QString &id, QSize *size,
                                         const QSize &requestedSize) {
  const qsizetype queryStart = id.indexOf(QLatin1Char('?'));
  const QString name = queryStart < 0 ? id : id.left(queryStart);
  const QUrlQuery query(queryStart < 0 ? QString() : id.mid(queryStart + 1));
  const QColor tint = parseTint(query.queryItemValue(QStringLiteral("color")));
  // ADR-0270: a "size" query stands in for Image.sourceSize where the item
  // drawing the icon (a QindaTK Thumbnail) does not expose one.
  const int hinted = query.queryItemValue(QStringLiteral("size")).toInt();
  const int requested = requestedSize.isValid()
      ? std::max(requestedSize.width(), requestedSize.height())
      : (hinted > 0 ? hinted : 64);
  const int edge = std::clamp(requested, 1,
                              static_cast<int>(maximumIconPixels));
  const QImage image =
      renderOnGuiThread({.name = name, .tint = tint, .edge = edge});
  QPixmap pixmap = image.isNull() ? QPixmap() : QPixmap::fromImage(image);
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
