// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_configuration.h"

#include <QHash>
#include <QMargins>
#include <QString>

namespace QindaQt::Shell {

// Reads, from an accepted panel surface plan, how deep the shell's own panels
// reserve into each output: the desktop work area's input (ADR-0261).
//
// AGENT-CONTRACT (ADR-0261): this is the runtime half of the desktop
// work-area contract. ShellRuntimeApplication::reconcileSurfaces() passes
// every plan PanelSurfaceController accepted through fromPlan() and hands the
// result to DesktopSurfaceController::setOutputReservations(), which is the
// only consumer. The desktop surface never sees PanelSurfacePlan, and this
// adapter never sees desktop placement.
//
// AGENT-GUARD: follow the exclusive zones the compositor was actually asked
// for, never the solver's static work area or QScreen::availableGeometry()
// (which layer-shell zones do not reach). A hidden panel, an auto-hiding or
// overlay panel, and every non-carrier publish -1 and so reserve nothing here
// either; reading any other field would make desktop icons avoid a region
// that ordinary windows are allowed to cover.
class PanelReservationInsets final {
public:
    // Output id (QScreen::name(), as PanelSurfaceIdentity::outputId) -> depth
    // reserved from each output edge, in logical pixels. A depth is the
    // edge's mapped reservation carrier's exclusiveZone plus its margin on the
    // anchored edge, because wlr-layer-shell counts that margin inside the
    // zone (see PanelSurfaceConfigurationPlanner). Outputs on which nothing
    // reserves are absent, which the consumer reads as zero on every edge. A
    // rejected plan yields an empty map. Pure: it reads no Qt GUI state.
    [[nodiscard]] static QHash<QString, QMargins> fromPlan(
        const ShellSurface::PanelSurfacePlan &plan);
};

} // namespace QindaQt::Shell
