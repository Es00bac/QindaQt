// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelreservationinsets.h"

#include <algorithm>
#include <limits>

namespace QindaQt::Shell {
namespace {

// The carrier's margin on the edge it is anchored to; wlr-layer-shell adds it
// to the exclusive zone, so the reserved depth is margin + zone.
int anchoredEdgeMargin(const ShellSurface::PanelSurfaceConfiguration &surface)
{
    switch (surface.edge) {
    case Profiles::Edge::Top:
        return surface.margins.top();
    case Profiles::Edge::Bottom:
        return surface.margins.bottom();
    case Profiles::Edge::Left:
        return surface.margins.left();
    case Profiles::Edge::Right:
        return surface.margins.right();
    }
    return 0;
}

int reservedDepth(const ShellSurface::PanelSurfaceConfiguration &surface)
{
    const qint64 depth = static_cast<qint64>(surface.exclusiveZone)
        + std::max(0, anchoredEdgeMargin(surface));
    return static_cast<int>(std::min<qint64>(depth, std::numeric_limits<int>::max()));
}

} // namespace

QHash<QString, QMargins> PanelReservationInsets::fromPlan(
    const ShellSurface::PanelSurfacePlan &plan)
{
    QHash<QString, QMargins> insets;
    if (!plan.ok()) {
        return insets;
    }
    for (const auto &surface : plan.surfaces) {
        if (surface.mapping != ShellSurface::PanelSurfaceMapping::Mapped
            || !surface.reservationCarrier || surface.exclusiveZone <= 0) {
            continue;
        }
        const int depth = reservedDepth(surface);
        // The planners elect one carrier per output edge; taking the deepest
        // keeps a malformed plan from under-reserving rather than trusting
        // whichever carrier happened to come last.
        QMargins &output = insets[surface.identity.outputId];
        switch (surface.edge) {
        case Profiles::Edge::Top:
            output.setTop(std::max(output.top(), depth));
            break;
        case Profiles::Edge::Bottom:
            output.setBottom(std::max(output.bottom(), depth));
            break;
        case Profiles::Edge::Left:
            output.setLeft(std::max(output.left(), depth));
            break;
        case Profiles::Edge::Right:
            output.setRight(std::max(output.right(), depth));
            break;
        }
    }
    return insets;
}

} // namespace QindaQt::Shell
