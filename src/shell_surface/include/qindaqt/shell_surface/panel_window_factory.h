// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_configuration.h"

#include <QString>

#include <memory>

class QQuickWindow;

namespace QindaQt::ShellSurface {

class PanelWindowFactory {
public:
    virtual ~PanelWindowFactory() = default;

    // The shell owns profile/theme/catalog selection. Implementations may load
    // any QML component and inject its panel/theme values here; the returned
    // window must be hidden and unparented so the backend can assign its
    // Wayland role before the first map.
    [[nodiscard]] virtual std::unique_ptr<QQuickWindow> createWindow(
        const PanelSurfaceConfiguration &configuration,
        QString *error = nullptr) = 0;
};

} // namespace QindaQt::ShellSurface
