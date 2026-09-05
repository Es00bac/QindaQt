// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtGui/QColor>
#include <QtGui/QImage>

#include <QtQuick/QQuickImageProvider>

#include <memory>

namespace QindaQt::Shell::Icons
{

class IconThemeLocator;

// AGENT-CONTRACT: `image://qindaqt-icon/` provider for the shell. The URL id
// is `<name>?size=<px>&scale=<f>&color=<#rrggbb>&symbolic=1` with every query
// item optional: `size` clamps to [1, 512] logical pixels (default 32 or the
// QML `sourceSize`), `scale` clamps to [1, 4] (default 1), `color` applies
// only together with `symbolic=1` and recolors a symbolic SVG by replacing
// every painted pixel's RGB while preserving its alpha, and `symbolic`
// prefers the `<name>-symbolic` variant through the locator chain.
//
// SVG icons render through QSvgRenderer at the exact requested device size;
// raster sources are dimension-checked before decode and smoothly scaled.
// An unresolved, refused, or undecodable name returns the deterministic
// neutral placeholder (never a null or empty image, never a warning), so
// `QT_FATAL_WARNINGS=1` consumers stay clean on hostile input.
//
// Bounded resources: an id over kMaxRequestIdUtf8Bytes is refused before any
// parsing or cache access, and the LRU is keyed on the parsed, bounded
// request tuple (name, size, scale, color, symbolic) rather than the raw id,
// so retained key bytes stay bounded no matter how hostile the spelling.
//
// Threading: Qt may call requestImage() off the GUI thread; the locator and
// the bounded LRU cache are serialized behind an internal mutex. The
// instance is owned by the QQmlEngine once installed through
// IconRuntime::install().
class IconImageProvider : public QQuickImageProvider
{
public:
    IconImageProvider(QStringList iconRoots, QStringList themeNames);
    ~IconImageProvider() override;
    IconImageProvider(const IconImageProvider &) = delete;
    IconImageProvider &operator=(const IconImageProvider &) = delete;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    // Deterministic neutral placeholder: same device size, same pixels.
    [[nodiscard]] static QImage placeholder(int devicePixels);

    // Observability seams for the module's bounded-resources test rows:
    // cache occupancy and the total UTF-8 bytes retained in cache keys.
    // Product code must not branch on these.
    [[nodiscard]] int cacheEntryCount() const;
    [[nodiscard]] qsizetype cacheKeyBytes() const;

private:
    struct Request {
        QString name;
        int size = 0;
        double scale = 1.0;
        QColor color;     // invalid when absent or malformed
        bool symbolic = false;
        bool valid = false;
    };

    [[nodiscard]] static Request parseRequest(const QString &id,
                                              const QSize &requestedSize);
    [[nodiscard]] static QString cacheKeyFor(const Request &request);
    [[nodiscard]] QImage render(const Request &request, int devicePixels);
    [[nodiscard]] static QImage renderSvg(const QString &path, int devicePixels,
                                          const QColor &recolor);
    [[nodiscard]] static QImage renderRaster(const QString &path, int devicePixels);
    [[nodiscard]] static QImage recolorSymbolic(QImage image, const QColor &color);

    mutable QMutex m_mutex;
    std::unique_ptr<IconThemeLocator> m_locator;
    QHash<QString, QImage> m_cache;
    QList<QString> m_cacheOrder;
};

} // namespace QindaQt::Shell::Icons
