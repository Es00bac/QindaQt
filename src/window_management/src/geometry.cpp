// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/window_management/command.h>
#include <QtMath>
#include <cmath>
#include <limits>
namespace QindaQt::WindowManagement {
std::optional<QRect> regionalFrame(const QRect &area, const QRectF &region)
{
    if (!area.isValid() || !std::isfinite(region.x()) || !std::isfinite(region.y())
        || !std::isfinite(region.width()) || !std::isfinite(region.height())
        || region.x() < 0 || region.y() < 0 || region.width() <= 0
        || region.height() <= 0 || region.right() > 1 || region.bottom() > 1)
        return std::nullopt;
    // Use widened edges: a normalized rectangle at a far negative/positive
    // output origin must never overflow before the containment check.
    const qint64 left = qint64(area.x()) + qRound64(region.x() * area.width());
    const qint64 top = qint64(area.y()) + qRound64(region.y() * area.height());
    const qint64 right = qint64(area.x()) + qRound64(region.right() * area.width());
    const qint64 bottom = qint64(area.y()) + qRound64(region.bottom() * area.height());
    constexpr auto low = std::numeric_limits<int>::min();
    constexpr auto high = std::numeric_limits<int>::max();
    if (left < low || top < low || right - 1 > high || bottom - 1 > high
        || right <= left || bottom <= top)
        return std::nullopt;
    const QRect frame(int(left), int(top), int(right - left), int(bottom - top));
    return frame.isValid() && area.contains(frame) ? std::optional(frame) : std::nullopt;
}
QRectF insetRegion(double fraction)
{
    if (!std::isfinite(fraction) || fraction < 0.1 || fraction > 1)
        return {};
    const double margin = (1 - fraction) / 2;
    return {margin, margin, fraction, fraction};
}
} // namespace QindaQt::WindowManagement
