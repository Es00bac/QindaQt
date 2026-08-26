// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QMargins>
#include <QSize>
#include <QString>

class QQuickWindow;
class QScreen;

namespace QindaQt::ShellSurface {

enum class NotificationSurfaceRole {
    PopupStack,
    Center,
};

// Configures an unshown QQuickWindow as a non-exclusive top-right layer-shell
// notification surface. Every call is GUI-thread confined.
class LayerShellNotificationSurface final {
public:
    [[nodiscard]] static bool configure(
        QQuickWindow &window, QScreen &screen, NotificationSurfaceRole role,
        const QSize &desiredSize, const QMargins &margins,
        QString *error = nullptr);
    [[nodiscard]] static bool resize(QQuickWindow &window,
                                     const QSize &desiredSize,
                                     QString *error = nullptr);
};

} // namespace QindaQt::ShellSurface
