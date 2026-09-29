// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/authentication_overlay/overlay_surface.h>
namespace QindaQt::Apps::PolkitAgent {
// Polkit policy supplies only its scope; public platform module owns the role.
class PolkitOverlaySurface final {
public:
    static bool configure(QQuickWindow &window, QString *error = nullptr) {
        return AuthenticationOverlay::OverlaySurface::configure(window, QStringLiteral("polkit-agent"), error);
    }
};
}
