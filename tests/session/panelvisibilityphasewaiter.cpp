// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityphasewaiter.h"

#include <QJsonObject>
#include <QRect>

#include <cmath>
#include <limits>
#include <optional>

namespace QindaQt::Test::PanelVisibilityPhase {
namespace {

std::optional<int> exactInteger(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(std::numeric_limits<int>::min())
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(number);
}

std::optional<QRect> exactGeometry(const QJsonObject &item)
{
    const QJsonValue value = item.value(QStringLiteral("geometry"));
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject geometry = value.toObject();
    if (geometry.size() != 4) {
        return std::nullopt;
    }
    const auto x = exactInteger(geometry, QStringLiteral("x"));
    const auto y = exactInteger(geometry, QStringLiteral("y"));
    const auto width = exactInteger(geometry, QStringLiteral("width"));
    const auto height = exactInteger(geometry, QStringLiteral("height"));
    if (!x || !y || !width || !height) {
        return std::nullopt;
    }
    return QRect(*x, *y, *width, *height);
}

} // namespace

bool mappedPanel(const QJsonArray &items, const QString &edge, int thickness,
                 const QSize &output, int zone)
{
    for (const QJsonValue &value : items) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject item = value.toObject();
        const auto geometry = exactGeometry(item);
        if (!geometry) {
            continue;
        }
        const bool position =
            (edge == QStringLiteral("top") && geometry->y() == 0
             && geometry->height() == thickness)
            || (edge == QStringLiteral("left") && geometry->x() == 0
                && geometry->width() == thickness)
            || (edge == QStringLiteral("bottom")
                && geometry->height() == thickness
                && geometry->bottom() + 1 == output.height()
                && geometry->x() >= 0 && geometry->right() < output.width());
        if (position && item.value(QStringLiteral("mapped")).toBool()
            && item.value(QStringLiteral("committed")).toBool()
            && (zone == -2
                || item.value(QStringLiteral("exclusiveZone")).toInt(-999) == zone)) {
            return true;
        }
    }
    return false;
}

bool snapshotGeometryIsSettled(const QJsonArray &items, const QSize &output)
{
    if (output.isEmpty()) {
        return false;
    }
    const QRect framebuffer(QPoint{}, output);
    for (const QJsonValue &value : items) {
        if (!value.isObject()) {
            return false;
        }
        const auto geometry = exactGeometry(value.toObject());
        if (!geometry || geometry->isEmpty() || !framebuffer.contains(*geometry)) {
            return false;
        }
    }
    return true;
}

bool waitForSettledPhase(SurfaceAuthority &authority, PollTimer &timer,
                         const QSize &output, const PhasePredicate &ready,
                         QJsonArray *observed)
{
    if (observed == nullptr || !ready) {
        return false;
    }
    while (!timer.expired()) {
        *observed = authority.snapshot();
        // AGENT-GUARD: A zero-sized mapped layer role is an in-flight teardown,
        // not proof that the panel is hidden. Capture only settled authority.
        if (snapshotGeometryIsSettled(*observed, output) && ready(*observed)) {
            return true;
        }
        timer.waitForNextPoll();
    }
    return false;
}

} // namespace QindaQt::Test::PanelVisibilityPhase
