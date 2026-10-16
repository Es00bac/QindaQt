// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridoutputrescue.h"

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// Squared distance from a point to a rectangle (0 inside it).
qreal distanceSquared(const QPointF &point, const QRectF &rect)
{
    const qreal dx = std::max({rect.left() - point.x(), 0.0, point.x() - rect.right()});
    const qreal dy = std::max({rect.top() - point.y(), 0.0, point.y() - rect.bottom()});
    return dx * dx + dy * dy;
}

// The nearest frame of the same size inside `area`, shrunk only when it does
// not fit. QRect::right()/bottom() are inclusive, hence the +1.
QRect containedWithin(const QRect &frame, const QRect &area)
{
    QRect result = frame;
    if (result.width() > area.width()) {
        result.setWidth(area.width());
    }
    if (result.height() > area.height()) {
        result.setHeight(area.height());
    }
    result.moveTo(
        std::clamp(result.left(), area.left(), area.right() - result.width() + 1),
        std::clamp(result.top(), area.top(), area.bottom() - result.height() + 1));
    return result;
}

} // namespace

bool isStranded(const QRectF &frame, const QVector<RescueOutput> &outputs)
{
    if (outputs.isEmpty() || !frame.isValid()) {
        return false;
    }
    return std::none_of(outputs.cbegin(), outputs.cend(), [&frame](const auto &output) {
        return QRectF(output.geometry).intersects(frame);
    });
}

std::optional<QRectF> rescuedFrame(const QRectF &frame, const QVector<RescueOutput> &outputs)
{
    if (!isStranded(frame, outputs)) {
        return std::nullopt;
    }
    const QPointF centre = frame.center();
    qsizetype nearest = -1;
    qreal nearestDistance = std::numeric_limits<qreal>::infinity();
    for (qsizetype index = 0; index < outputs.size(); ++index) {
        const qreal distance = distanceSquared(centre, QRectF(outputs[index].geometry));
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = index;
        }
    }
    if (nearest < 0) {
        return std::nullopt;
    }
    const auto &output = outputs[nearest];
    const QRect area = output.workArea.isValid() ? output.workArea : output.geometry;
    if (!area.isValid()) {
        return std::nullopt;
    }
    // Work in whole logical pixels: KWin frames are logical and the work area
    // is integral, so rounding the size keeps the result inside the area.
    const QRect rounded(qRound(frame.x()), qRound(frame.y()),
                        std::max(1, qRound(frame.width())),
                        std::max(1, qRound(frame.height())));
    return QRectF(containedWithin(rounded, area));
}

std::optional<QRect> rescuedFrame(const QRect &frame, const QVector<RescueOutput> &outputs)
{
    const auto rescued = rescuedFrame(QRectF(frame), outputs);
    return rescued ? std::optional<QRect>(rescued->toRect()) : std::nullopt;
}

} // namespace QindaQt::Compositor::KWinIntegration
