// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_panel_integration.h"

#include <QScreen>
#include <QWindow>
#include <QtGui/qscreen_platform.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <wayland-client-core.h>

namespace QindaQt::Apps::Osk {
namespace {

class InputPanelSurface final : public QtWaylandClient::QWaylandShellSurface,
                                public QtWayland::zwp_input_panel_surface_v1 {
public:
    InputPanelSurface(InputPanelV1 &panel, QtWaylandClient::QWaylandWindow *window)
        : QtWaylandClient::QWaylandShellSurface(window),
          QtWayland::zwp_input_panel_surface_v1(panel.get_input_panel_surface(window->wlSurface()))
    {
        struct ::wl_output *output = nullptr;
        if (QScreen *screen = window->window()->screen()) {
            if (auto *native = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
                output = native->output();
            }
        }
        set_toplevel(output, position_center_bottom);
    }

    ~InputPanelSurface() override
    {
        // The role interface has no destructor request; drop the proxy.
        if (isInitialized()) {
            wl_proxy_destroy(reinterpret_cast<struct ::wl_proxy *>(object()));
        }
    }
};

} // namespace

InputPanelV1::InputPanelV1() : QWaylandClientExtensionTemplate<InputPanelV1>(1) {}

InputPanelShellIntegration::InputPanelShellIntegration(InputPanelV1 &panel) : m_panel(panel) {}

bool InputPanelShellIntegration::initialize(QtWaylandClient::QWaylandDisplay *display)
{
    Q_UNUSED(display);
    return m_panel.isActive();
}

QtWaylandClient::QWaylandShellSurface *InputPanelShellIntegration::createShellSurface(QtWaylandClient::QWaylandWindow *window)
{
    return new InputPanelSurface(m_panel, window);
}

bool InputPanelShellIntegration::attach(QWindow *window)
{
    if (window == nullptr || !m_panel.isActive()) {
        return false;
    }
    window->create();
    auto *platformWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    if (platformWindow == nullptr) {
        return false;
    }
    platformWindow->setShellIntegration(this);
    return true;
}

} // namespace QindaQt::Apps::Osk
