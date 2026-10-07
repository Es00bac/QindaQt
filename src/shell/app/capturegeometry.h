// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QSize>
#include <cmath>
#include <optional>

namespace QindaQt::Shell::CaptureGeometry {

// AGENT-CONTRACT: CLI admission, pre-window admission and readback share these
// bounds. Checking only grabWindow is too late for the first scene allocation.
inline constexpr int maximumDimension = 16384;
inline constexpr qint64 maximumPixels = 64 * 1024 * 1024;

inline bool acceptsLogicalSize(QSize size)
{
    return !size.isEmpty() && size.width() <= maximumDimension
        && size.height() <= maximumDimension
        && static_cast<qint64>(size.width()) * size.height() <= maximumPixels;
}

inline std::optional<QSize> physicalSize(QSize logicalSize, qreal ratio)
{
    if (!acceptsLogicalSize(logicalSize) || !std::isfinite(ratio) || ratio <= 0) {
        return std::nullopt;
    }
    const double width = static_cast<double>(logicalSize.width()) * ratio;
    const double height = static_cast<double>(logicalSize.height()) * ratio;
    if (!std::isfinite(width) || !std::isfinite(height)
        || width < 1 || height < 1
        || width > maximumDimension || height > maximumDimension) {
        return std::nullopt;
    }
    // Match Qt size rounding, including fractional DPR. No image resize may
    // conceal a mismatch between the native grab and admitted geometry.
    const QSize physicalSize = logicalSize * ratio;
    if (static_cast<qint64>(physicalSize.width()) * physicalSize.height() > maximumPixels) {
        return std::nullopt;
    }
    return physicalSize;
}

} // namespace QindaQt::Shell::CaptureGeometry
