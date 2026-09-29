// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
class QQuickWindow;
namespace QindaQt::AuthenticationOverlay {
// Platform-only borrowed-window boundary; no requester/authentication policy.
// initializePlatform MUST precede QGuiApplication. configure runs on GUI thread,
// before any native creation/show, retains no pointer, and fails closed on
// unsupported platform/role. Success permits caller to show; caller owns all
// hide/destruction and presentation lifetime. No ordinary-toplevel fallback.
// Pointer-output selection is best effort until native output policy qualifies.
class OverlaySurface final {
public:
    static bool initializePlatform();
    [[nodiscard]] static bool configure(QQuickWindow &window, const QString &scope,
                                        QString *error = nullptr);
};
}
