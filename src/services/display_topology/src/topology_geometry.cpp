// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_topology/topology.h>

#include <qindaqt/services/display_protocol/display_limits.h>

#include "topology_validation_p.h"

#include <cmath>
#include <limits>

namespace QindaQt::DisplayTopology
{

bool transposesDimensions(const Display::Transform transform) noexcept
{
    return transform == Display::Transform::Rotate90
        || transform == Display::Transform::Rotate270
        || transform == Display::Transform::FlipX90
        || transform == Display::Transform::FlipX270;
}

QSize logicalSizeForMode(const Display::Mode &mode, const double scale,
                         const Display::Transform transform)
{
    if (!std::isfinite(scale) || scale <= 0.0) {
        return {};
    }
    const int pixelWidth = transposesDimensions(transform) ? mode.pixelSize.height()
                                                           : mode.pixelSize.width();
    const int pixelHeight = transposesDimensions(transform) ? mode.pixelSize.width()
                                                            : mode.pixelSize.height();
    const double logicalWidth = static_cast<double>(pixelWidth) / scale;
    const double logicalHeight = static_cast<double>(pixelHeight) / scale;
    if (logicalWidth > static_cast<double>(std::numeric_limits<int>::max())
        || logicalHeight > static_cast<double>(std::numeric_limits<int>::max())) {
        return {};
    }
    return {static_cast<int>(std::floor(logicalWidth + 0.5)),
            static_cast<int>(std::floor(logicalHeight + 0.5))};
}

bool hasIntegralLogicalExtent(const Display::Mode &mode, const double scale,
                              const Display::Transform transform)
{
    if (!std::isfinite(scale) || scale <= 0.0) {
        return false;
    }
    const int pixelWidth = transposesDimensions(transform) ? mode.pixelSize.height()
                                                           : mode.pixelSize.width();
    const int pixelHeight = transposesDimensions(transform) ? mode.pixelSize.width()
                                                            : mode.pixelSize.height();
    const double width = static_cast<double>(pixelWidth) / scale;
    const double height = static_cast<double>(pixelHeight) / scale;
    constexpr double tolerance = 1e-9;
    return std::abs(width - std::round(width)) <= tolerance
        && std::abs(height - std::round(height)) <= tolerance;
}

namespace Private
{

ValidationResult failure(const TopologyError error, const char *reason,
                         const QString &stableId)
{
    return {.normalizedCandidate = {},
            .geometries = {},
            .warnings = {},
            .differences = {},
            .fingerprint = {},
            .error = error,
            .reasonCode = QString::fromLatin1(reason),
            .offendingStableId = stableId,
            .noOp = false};
}

const Display::Output *findOutput(const Display::Snapshot &snapshot, const QString &stableId)
{
    for (const Display::Output &output : snapshot.outputs) {
        if (output.stableId == stableId) {
            return &output;
        }
    }
    return nullptr;
}

const Display::Mode *findMode(const Display::Output &output, const QString &modeId)
{
    for (const Display::Mode &mode : output.modes) {
        if (mode.id == modeId) {
            return &mode;
        }
    }
    return nullptr;
}

bool checkedRect(const QPoint &position, const QSize &size, QRect &rect)
{
    const qint64 right = static_cast<qint64>(position.x()) + size.width();
    const qint64 bottom = static_cast<qint64>(position.y()) + size.height();
    if (size.width() <= 0 || size.height() <= 0 || position.x() < 0 || position.y() < 0
        || right > Display::kCoordinateBound || bottom > Display::kCoordinateBound) {
        return false;
    }
    rect = QRect(position, size);
    return true;
}

} // namespace Private
} // namespace QindaQt::DisplayTopology
