// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qwayland-input-method-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtension>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>

class QWindow;

namespace QindaQt::Apps::Osk {

// `zwp_input_panel_v1`: the global that gives a surface the input-panel role,
// which is how the compositor knows to park the keyboard along the bottom
// edge instead of treating it as an application window.
class InputPanelV1 final : public QWaylandClientExtensionTemplate<InputPanelV1>,
                           public QtWayland::zwp_input_panel_v1 {
    Q_OBJECT

public:
    InputPanelV1();
    void bindNow() { initialize(); }
};

// Qt's shell integration seam, filled with the input-panel role (ADR-0204).
// Installed on the keyboard window before it is shown, so Qt never asks
// xdg-shell for a toplevel for it.
class InputPanelShellIntegration final : public QtWaylandClient::QWaylandShellIntegration {
public:
    explicit InputPanelShellIntegration(InputPanelV1 &panel);

    [[nodiscard]] bool initialize(QtWaylandClient::QWaylandDisplay *display) override;
    [[nodiscard]] QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;

    // Attaches this integration to `window`'s platform window. Returns false
    // when the window is not a Wayland window or the panel global is absent.
    [[nodiscard]] bool attach(QWindow *window);

private:
    InputPanelV1 &m_panel;
};

} // namespace QindaQt::Apps::Osk
