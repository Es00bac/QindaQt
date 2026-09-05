// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/icon_image_provider.h>

#include <qindaqt/shell/icons/icon_theme_limits.h>
#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QUrlQuery>

#include <QSvgRenderer>

namespace QindaQt::Shell::Icons
{
namespace
{

[[nodiscard]] int clampedLogicalSize(int size)
{
    return qBound(kMinIconLogicalSize, size, kMaxIconLogicalSize);
}

[[nodiscard]] double clampedScale(double scale)
{
    if (!(scale >= kMinIconScale)) {
        return kMinIconScale; // NaN, zero, and negatives land on the default.
    }
    return qMin(scale, kMaxIconScale);
}

[[nodiscard]] int devicePixelsFor(int logicalSize, double scale)
{
    const double device = double(clampedLogicalSize(logicalSize)) * clampedScale(scale);
    return qBound(1, int(device + 0.5), kMaxIconLogicalSize * int(kMaxIconScale));
}

[[nodiscard]] QColor parseColor(const QString &value)
{
    // Only the documented #rrggbb form is accepted; anything else is ignored
    // rather than guessed.
    if (value.size() != 7 || !value.startsWith(QLatin1Char('#'))) {
        return {};
    }
    const QColor color(value);
    return color.isValid() ? color : QColor();
}

} // namespace

IconImageProvider::IconImageProvider(QStringList iconRoots, QStringList themeNames)
    : QQuickImageProvider(QQmlImageProviderBase::Image)
    , m_locator(std::make_unique<IconThemeLocator>(std::move(iconRoots),
                                                   std::move(themeNames)))
{
}

IconImageProvider::~IconImageProvider() = default;

QImage IconImageProvider::requestImage(const QString &id, QSize *size,
                                       const QSize &requestedSize)
{
    QMutexLocker locker(&m_mutex);
    // AGENT-GUARD: Over-long ids are refused before parsing and before any
    // cache access. The LRU bounds only the entry count; keying it on a
    // caller-controlled id of arbitrary byte length would let hostile
    // requests grow shell memory without bound.
    if (id.toUtf8().size() > kMaxRequestIdUtf8Bytes) {
        const QImage refused = placeholder(devicePixelsFor(kDefaultIconSize, 1.0));
        if (size != nullptr) {
            *size = refused.size();
        }
        return refused;
    }
    const Request request = parseRequest(id, requestedSize);
    const int devicePixels = request.valid
        ? devicePixelsFor(request.size, request.scale)
        : devicePixelsFor(kDefaultIconSize, 1.0);

    const QString cacheKey = cacheKeyFor(request);
    const auto cached = m_cache.constFind(cacheKey);
    if (cached != m_cache.cend()) {
        // LRU refresh: a hit moves the key to the most-recent end.
        m_cacheOrder.removeAll(cacheKey);
        m_cacheOrder.append(cacheKey);
        if (size != nullptr) {
            *size = cached->size();
        }
        return cached.value();
    }

    QImage image;
    if (request.valid) {
        image = render(request, devicePixels);
    }
    if (image.isNull()) {
        image = placeholder(devicePixels);
    }
    // AGENT-GUARD: The provider must never hand Qt a null or empty image;
    // the QML image pipeline warns (fatal under the test environment) and
    // the shell would render nothing for a hostile name.
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        image = placeholder(qMax(1, kDefaultIconSize));
    }

    if (m_cacheOrder.size() >= kMaxCachedIconImages) {
        m_cache.remove(m_cacheOrder.takeFirst());
    }
    m_cache.insert(cacheKey, image);
    m_cacheOrder.append(cacheKey);
    if (size != nullptr) {
        *size = image.size();
    }
    return image;
}

int IconImageProvider::cacheEntryCount() const
{
    QMutexLocker locker(&m_mutex);
    return int(m_cache.size());
}

qsizetype IconImageProvider::cacheKeyBytes() const
{
    QMutexLocker locker(&m_mutex);
    qsizetype total = 0;
    for (const QString &key : m_cacheOrder) {
        total += key.toUtf8().size();
    }
    return total;
}

QString IconImageProvider::cacheKeyFor(const Request &request)
{
    // AGENT-GUARD: The cache is keyed on the parsed, bounded request tuple —
    // never the raw id — so retained key bytes stay bounded regardless of
    // the request's spelling, and spellings that parse to the same tuple
    // share one entry. Invalid requests share a single sentinel key: their
    // placeholder output is identical. The sentinel cannot collide with a
    // valid key, which always starts with a nonempty grammar name.
    if (!request.valid) {
        return QStringLiteral("\x1f");
    }
    return request.name + QLatin1Char('\x1f') + QString::number(request.size)
        + QLatin1Char('\x1f') + QString::number(request.scale, 'g', 17)
        + QLatin1Char('\x1f') + QLatin1Char(request.symbolic ? '1' : '0')
        + QLatin1Char('\x1f')
        + (request.color.isValid() ? request.color.name() : QString());
}

QImage IconImageProvider::placeholder(int devicePixels)
{
    const int side = qBound(1, devicePixels, kMaxIconLogicalSize * int(kMaxIconScale));
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    // Neutral, font-free mark: a muted rounded outline with a centered dot.
    // Deterministic: identical device size produces identical pixels.
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor neutral(128, 128, 128, 255);
    const qreal margin = side * 0.125;
    const qreal penWidth = qMax(1.0, side / 24.0);
    painter.setPen(QPen(neutral, penWidth));
    painter.setBrush(QColor(128, 128, 128, 48));
    painter.drawRoundedRect(QRectF(margin, margin, side - 2 * margin, side - 2 * margin),
                            side * 0.2, side * 0.2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(neutral);
    const qreal dot = qMax(penWidth * 2.0, side * 0.25);
    painter.drawEllipse(QRectF((side - dot) / 2.0, (side - dot) / 2.0, dot, dot));
    return image;
}

IconImageProvider::Request IconImageProvider::parseRequest(const QString &id,
                                                           const QSize &requestedSize)
{
    Request request;
    const qsizetype queryStart = id.indexOf(QLatin1Char('?'));
    request.name = queryStart < 0 ? id : id.left(queryStart);
    if (!IconThemeLocator::isAcceptableIconName(request.name)) {
        return request;
    }

    int size = kDefaultIconSize;
    if (requestedSize.isValid() && requestedSize.width() > 0) {
        size = requestedSize.width();
    }
    double scale = 1.0;
    if (queryStart >= 0) {
        const QUrlQuery query(id.mid(queryStart + 1));
        const QString sizeValue = query.queryItemValue(QStringLiteral("size"));
        if (!sizeValue.isEmpty()) {
            bool ok = false;
            const int parsed = sizeValue.toInt(&ok);
            if (ok) {
                size = parsed;
            }
        }
        const QString scaleValue = query.queryItemValue(QStringLiteral("scale"));
        if (!scaleValue.isEmpty()) {
            bool ok = false;
            const double parsed = scaleValue.toDouble(&ok);
            if (ok) {
                scale = parsed;
            }
        }
        const QString symbolicValue = query.queryItemValue(QStringLiteral("symbolic"));
        request.symbolic =
            symbolicValue == QLatin1String("1") || symbolicValue == QLatin1String("true");
        request.color = parseColor(query.queryItemValue(QStringLiteral("color")));
    }
    request.size = clampedLogicalSize(size);
    request.scale = clampedScale(scale);
    request.valid = true;
    return request;
}

QImage IconImageProvider::render(const Request &request, int devicePixels)
{
    const QString path = m_locator->locate(request.name, request.size, request.scale,
                                           request.symbolic);
    if (path.isEmpty()) {
        return {};
    }
    if (path.endsWith(QLatin1String(".svg"))) {
        const QColor recolor =
            request.symbolic && request.color.isValid() ? request.color : QColor();
        return renderSvg(path, devicePixels, recolor);
    }
    return renderRaster(path, devicePixels);
}

QImage IconImageProvider::renderSvg(const QString &path, int devicePixels,
                                    const QColor &recolor)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    // AGENT-GUARD: The SVG byte ceiling bounds QSvgRenderer input; hostile
    // multi-megabyte vectors fail closed to the placeholder.
    const QByteArray raw = file.read(kMaxSvgSourceBytes + 1);
    if (raw.size() > kMaxSvgSourceBytes || raw.isEmpty()) {
        return {};
    }
    QSvgRenderer renderer(raw);
    if (!renderer.isValid()) {
        return {};
    }
    const QSizeF natural = renderer.defaultSize();
    if (natural.isEmpty() || natural.width() > kMaxSourceImageDimension * 8
        || natural.height() > kMaxSourceImageDimension * 8) {
        return {};
    }

    QImage image(devicePixels, devicePixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    // Preserve the icon's own aspect ratio inside the square device target.
    QSizeF fitted = natural;
    fitted.scale(QSizeF(devicePixels, devicePixels), Qt::KeepAspectRatio);
    const QRectF target((devicePixels - fitted.width()) / 2.0,
                        (devicePixels - fitted.height()) / 2.0, fitted.width(),
                        fitted.height());
    renderer.render(&painter, target);
    painter.end();

    if (recolor.isValid()) {
        return recolorSymbolic(image, recolor);
    }
    return image;
}

QImage IconImageProvider::renderRaster(const QString &path, int devicePixels)
{
    QImageReader reader(path);
    // Dimension metadata is checked before any pixel allocation; decoded
    // dimensions are checked again after decode.
    const QSize declared = reader.size();
    if (declared.isEmpty() || declared.width() > kMaxSourceImageDimension
        || declared.height() > kMaxSourceImageDimension) {
        return {};
    }
    QImage decoded = reader.read();
    if (decoded.isNull() || decoded.width() > kMaxSourceImageDimension
        || decoded.height() > kMaxSourceImageDimension) {
        return {};
    }
    if (decoded.width() == devicePixels && decoded.height() == devicePixels) {
        return decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return decoded.scaled(devicePixels, devicePixels, Qt::KeepAspectRatio,
                          Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QImage IconImageProvider::recolorSymbolic(QImage image, const QColor &color)
{
    // Symbolic icons are monochrome by convention: preserve the painted
    // alpha shape and replace the RGB with the requested token color.
    image = image.convertToFormat(QImage::Format_ARGB32);
    const int red = color.red();
    const int green = color.green();
    const int blue = color.blue();
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha > 0) {
                line[x] = qRgba(red, green, blue, alpha);
            }
        }
    }
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

} // namespace QindaQt::Shell::Icons
