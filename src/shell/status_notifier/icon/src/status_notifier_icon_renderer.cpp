// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h>

#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_locator.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QPainter>

#include <QFile>

namespace QindaQt::StatusNotifier
{

StatusNotifierIconRenderer::StatusNotifierIconRenderer(QStringList themeRoots)
    : m_locator(std::make_unique<StatusNotifierIconLocator>(std::move(themeRoots)))
{
}

StatusNotifierIconRenderer::~StatusNotifierIconRenderer() = default;

QImage StatusNotifierIconRenderer::render(const IconPayload &icon, int size) const
{
    const int target = size > 0 ? size : 16;

    // The nearest bounded wire pixmap wins over theme lookup: an item that
    // ships its own pixels must not be second-guessed against the theme.
    QImage best;
    qint64 bestDistance = -1;
    for (const Pixmap &pixmap : icon.pixmaps) {
        QImage decoded = decodePixmap(pixmap);
        if (decoded.isNull()) {
            continue;
        }
        const qint64 distance = qAbs(qint64(decoded.width()) - qint64(target));
        if (bestDistance < 0 || distance < bestDistance) {
            best = std::move(decoded);
            bestDistance = distance;
        }
    }
    if (!best.isNull()) {
        return best;
    }

    const QImage themed = decodeThemeIcon(icon.iconName, target);
    if (!themed.isNull()) {
        return themed;
    }
    return fallbackIcon(target);
}

QImage StatusNotifierIconRenderer::decodePixmap(const Pixmap &pixmap)
{
    if (pixmap.width == 0 || pixmap.height == 0
        || pixmap.width > kMaxIconPixmapDimension
        || pixmap.height > kMaxIconPixmapDimension) {
        return {};
    }
    const qsizetype expectedBytes =
        qsizetype(pixmap.width) * qsizetype(pixmap.height) * 4;
    if (expectedBytes > kMaxIconPixmapBytes || pixmap.argb.size() != expectedBytes) {
        return {};
    }
    // AGENT-GUARD: The wire bytes are premultiplied ARGB32. The QImage wraps
    // the QByteArray's memory and must be copied before the caller can hold
    // it beyond this function.
    const QImage wrapped(reinterpret_cast<const uchar *>(pixmap.argb.constData()),
                         int(pixmap.width),
                         int(pixmap.height),
                         qsizetype(pixmap.width) * 4,
                         QImage::Format_ARGB32_Premultiplied);
    return wrapped.copy();
}

QImage StatusNotifierIconRenderer::fallbackIcon(int size)
{
    const int extent = size > 0 ? size : 16;
    QImage image(extent, extent, QImage::Format_ARGB32_Premultiplied);
    // Deterministic neutral placeholder: fixed colors, no text, no clock, so
    // baselines and assistive snapshots see a stable image.
    image.fill(QColor(154, 160, 166));
    QPainter painter(&image);
    painter.setPen(QPen(QColor(95, 99, 104), qMax(1, extent / 16)));
    painter.drawRect(image.rect().adjusted(0, 0, -1, -1));
    return image;
}

QImage StatusNotifierIconRenderer::decodeThemeIcon(const QString &iconName, int size) const
{
    const QString path = m_locator->locate(iconName, size);
    if (path.isEmpty()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.read(kMaxIconFileBytes + 1);
    if (bytes.size() > kMaxIconFileBytes) {
        return {}; // Beyond the shared budget: fail closed into the fallback.
    }
    QImage decoded;
    if (!decoded.loadFromData(bytes)) {
        return {};
    }
    return decoded;
}

} // namespace QindaQt::StatusNotifier
