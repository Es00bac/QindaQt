// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QMargins>
#include <QSize>
#include <QString>

#include <optional>

namespace QindaQt::ShellSurface {

struct NotificationSurfaceLayout final {
    QSize popupSize;
    QSize centerSize;
    int visiblePopupCount = 0;

    bool operator==(const NotificationSurfaceLayout &) const = default;
};

struct NotificationSurfaceLayoutResult final {
    std::optional<NotificationSurfaceLayout> layout;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return layout.has_value(); }
};

// Plans logical-pixel surface sizes without touching a display server. The
// caller supplies QScreen logical geometry, not physical device pixels.
class NotificationSurfaceLayoutPlanner final {
public:
    [[nodiscard]] static NotificationSurfaceLayoutResult plan(
        const QSize &logicalOutputSize, const QMargins &popupMargins,
        const QMargins &centerMargins, int popupCount);
};

} // namespace QindaQt::ShellSurface
