// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"

#include <QHash>
#include <QSet>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::ShellSurface {
namespace {

constexpr std::size_t edgeCount = 4;

struct OutputState {
    ShellLayout::OutputLayout output;
    std::array<qint64, edgeCount> deepestStack{};
    std::array<qint64, edgeCount> expectedReservation{};
    std::array<qint64, edgeCount> deepestReservation{};
    std::array<std::optional<qsizetype>, edgeCount> reservationCarrier;
};

PanelSurfacePlan failure(PanelSurfacePlanErrorCode code, QString message,
                         PanelSurfaceIdentity identity = {})
{
    PanelSurfacePlan result;
    result.error = {code, std::move(identity), std::move(message)};
    return result;
}

std::optional<std::size_t> edgeIndex(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
        return 0;
    case Profiles::Edge::Bottom:
        return 1;
    case Profiles::Edge::Left:
        return 2;
    case Profiles::Edge::Right:
        return 3;
    }
    return std::nullopt;
}

bool validLayer(Profiles::Layer layer)
{
    switch (layer) {
    case Profiles::Layer::Below:
    case Profiles::Layer::Normal:
    case Profiles::Layer::Above:
    case Profiles::Layer::Overlay:
        return true;
    }
    return false;
}

bool layerReservesWorkArea(Profiles::Layer layer)
{
    return layer == Profiles::Layer::Normal || layer == Profiles::Layer::Above;
}

bool hasSafeExtent(const QRect &geometry)
{
    if (!geometry.isValid()) {
        return false;
    }
    const qint64 width = static_cast<qint64>(geometry.right()) - geometry.left() + 1;
    const qint64 height = static_cast<qint64>(geometry.bottom()) - geometry.top() + 1;
    return width > 0 && width <= std::numeric_limits<int>::max() && height > 0 &&
        height <= std::numeric_limits<int>::max();
}

bool contains(const QRect &outer, const QRect &inner)
{
    return static_cast<qint64>(inner.left()) >= outer.left() &&
        static_cast<qint64>(inner.right()) <= outer.right() &&
        static_cast<qint64>(inner.top()) >= outer.top() &&
        static_cast<qint64>(inner.bottom()) <= outer.bottom();
}

std::array<qint64, edgeCount> reservationDepths(const ShellLayout::OutputLayout &output)
{
    return {
        static_cast<qint64>(output.workArea.top()) - output.geometry.top(),
        static_cast<qint64>(output.geometry.bottom()) - output.workArea.bottom(),
        static_cast<qint64>(output.workArea.left()) - output.geometry.left(),
        static_cast<qint64>(output.geometry.right()) - output.workArea.right(),
    };
}

qint64 depthFromEdge(const ShellLayout::PanelSurface &surface,
                     const ShellLayout::OutputLayout &output)
{
    switch (surface.edge) {
    case Profiles::Edge::Top:
        return static_cast<qint64>(surface.geometry.bottom()) - output.geometry.top() + 1;
    case Profiles::Edge::Bottom:
        return static_cast<qint64>(output.geometry.bottom()) - surface.geometry.top() + 1;
    case Profiles::Edge::Left:
        return static_cast<qint64>(surface.geometry.right()) - output.geometry.left() + 1;
    case Profiles::Edge::Right:
        return static_cast<qint64>(output.geometry.right()) - surface.geometry.left() + 1;
    }
    return -1;
}

int surfaceThickness(const ShellLayout::PanelSurface &surface)
{
    switch (surface.edge) {
    case Profiles::Edge::Top:
    case Profiles::Edge::Bottom:
        return surface.geometry.height();
    case Profiles::Edge::Left:
    case Profiles::Edge::Right:
        return surface.geometry.width();
    }
    return 0;
}

std::optional<int> checkedMargin(qint64 value)
{
    if (value < 0 || value > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

std::optional<PanelSurfaceConfiguration> configurationFor(
    const ShellLayout::PanelSurface &surface, const OutputState &state,
    bool reservationCarrier, qsizetype placementOrder)
{
    const QRect &output = state.output.geometry;
    const QRect &geometry = surface.geometry;
    const qint64 depth = depthFromEdge(surface, state.output);
    const int thickness = surfaceThickness(surface);
    const auto edgeMargin = checkedMargin(depth - thickness);
    if (!edgeMargin) {
        return std::nullopt;
    }

    PanelSurfaceConfiguration result;
    result.identity = {surface.panelId, surface.outputId};
    result.geometry = geometry;
    result.desiredSize = geometry.size();
    result.edge = surface.edge;
    result.layer = surface.layer;
    result.exclusiveEdge = surface.edge;
    // AGENT-CONTRACT: wlr-layer-shell includes the anchored-edge margin in
    // the effective exclusive region. The carrier therefore publishes its
    // own thickness here; edgeMargin + thickness reproduces the solver's
    // complete reserved depth. Publishing depth here would count the margin
    // twice for stacked panels.
    result.exclusiveZone = reservationCarrier ? thickness : -1;
    result.placementOrder = placementOrder;
    result.reservationCarrier = reservationCarrier;

    switch (surface.edge) {
    case Profiles::Edge::Top: {
        const auto left = checkedMargin(static_cast<qint64>(geometry.left()) - output.left());
        if (!left) {
            return std::nullopt;
        }
        result.anchors = SurfaceAnchor::Top | SurfaceAnchor::Left;
        result.margins = QMargins(*left, *edgeMargin, 0, 0);
        if (geometry.left() == output.left() && geometry.right() == output.right()) {
            // KWin prioritizes horizontally stretched positive-zone surfaces
            // before side reservations, which matches shell_layout's corner
            // ownership. Desired width zero asks the protocol to stretch.
            result.anchors |= SurfaceAnchor::Right;
            result.desiredSize.setWidth(0);
        }
        break;
    }
    case Profiles::Edge::Bottom: {
        const auto left = checkedMargin(static_cast<qint64>(geometry.left()) - output.left());
        if (!left) {
            return std::nullopt;
        }
        result.anchors = SurfaceAnchor::Bottom | SurfaceAnchor::Left;
        result.margins = QMargins(*left, 0, 0, *edgeMargin);
        if (geometry.left() == output.left() && geometry.right() == output.right()) {
            result.anchors |= SurfaceAnchor::Right;
            result.desiredSize.setWidth(0);
        }
        break;
    }
    case Profiles::Edge::Left: {
        // Positive-zone side surfaces are arranged after top/bottom carriers;
        // their placement bounds already begin at the reserved work area.
        const qint64 verticalBase = reservationCarrier ? state.output.workArea.top() : output.top();
        const auto top = checkedMargin(static_cast<qint64>(geometry.top()) - verticalBase);
        if (!top) {
            return std::nullopt;
        }
        result.anchors = SurfaceAnchor::Left | SurfaceAnchor::Top;
        result.margins = QMargins(*edgeMargin, *top, 0, 0);
        const qint64 laneTop = static_cast<qint64>(output.top()) + state.deepestStack[0];
        const qint64 laneBottom =
            static_cast<qint64>(output.bottom()) - state.deepestStack[1];
        if (geometry.top() == laneTop && geometry.bottom() == laneBottom) {
            const qint64 verticalBottom =
                reservationCarrier ? state.output.workArea.bottom() : output.bottom();
            const auto bottom =
                checkedMargin(verticalBottom - static_cast<qint64>(geometry.bottom()));
            if (!bottom) {
                return std::nullopt;
            }
            result.anchors |= SurfaceAnchor::Bottom;
            result.desiredSize.setHeight(0);
            result.margins.setBottom(*bottom);
        }
        break;
    }
    case Profiles::Edge::Right: {
        const qint64 verticalBase = reservationCarrier ? state.output.workArea.top() : output.top();
        const auto top = checkedMargin(static_cast<qint64>(geometry.top()) - verticalBase);
        if (!top) {
            return std::nullopt;
        }
        result.anchors = SurfaceAnchor::Right | SurfaceAnchor::Top;
        result.margins = QMargins(0, *top, *edgeMargin, 0);
        const qint64 laneTop = static_cast<qint64>(output.top()) + state.deepestStack[0];
        const qint64 laneBottom =
            static_cast<qint64>(output.bottom()) - state.deepestStack[1];
        if (geometry.top() == laneTop && geometry.bottom() == laneBottom) {
            const qint64 verticalBottom =
                reservationCarrier ? state.output.workArea.bottom() : output.bottom();
            const auto bottom =
                checkedMargin(verticalBottom - static_cast<qint64>(geometry.bottom()));
            if (!bottom) {
                return std::nullopt;
            }
            result.anchors |= SurfaceAnchor::Bottom;
            result.desiredSize.setHeight(0);
            result.margins.setBottom(*bottom);
        }
        break;
    }
    }
    return result;
}

} // namespace

PanelSurfacePlan PanelSurfaceConfigurationPlanner::plan(
    const ShellLayout::PanelLayoutResult &layout)
{
    if (!layout.ok()) {
        return failure(PanelSurfacePlanErrorCode::RejectedLayout,
                       QStringLiteral("cannot configure surfaces from a rejected layout: %1")
                           .arg(layout.error.message));
    }

    QVector<OutputState> outputs;
    outputs.reserve(layout.outputs.size());
    QHash<QString, qsizetype> outputIndices;
    for (const auto &output : layout.outputs) {
        if (output.outputId.trimmed().isEmpty() || !hasSafeExtent(output.geometry) ||
            !hasSafeExtent(output.workArea) || !contains(output.geometry, output.workArea)) {
            return failure(PanelSurfacePlanErrorCode::InvalidOutput,
                           QStringLiteral("output '%1' has inconsistent solved geometry")
                               .arg(output.outputId),
                           {{}, output.outputId});
        }
        if (!std::isfinite(output.scale) || output.scale <= 0.0) {
            return failure(PanelSurfacePlanErrorCode::InvalidOutput,
                           QStringLiteral("output '%1' has invalid scale metadata")
                               .arg(output.outputId),
                           {{}, output.outputId});
        }
        if (outputIndices.contains(output.outputId)) {
            return failure(PanelSurfacePlanErrorCode::DuplicateOutput,
                           QStringLiteral("duplicate solved output '%1'").arg(output.outputId),
                           {{}, output.outputId});
        }
        OutputState state;
        state.output = output;
        state.expectedReservation = reservationDepths(output);
        outputIndices.insert(output.outputId, outputs.size());
        outputs.push_back(std::move(state));
    }
    if (outputs.isEmpty()) {
        return failure(PanelSurfacePlanErrorCode::InvalidOutput,
                       QStringLiteral("the solved layout has no outputs"));
    }

    QHash<QString, QSet<QString>> panelIdsByOutput;
    for (qsizetype surfaceIndex = 0; surfaceIndex < layout.surfaces.size(); ++surfaceIndex) {
        const auto &surface = layout.surfaces[surfaceIndex];
        const PanelSurfaceIdentity identity{surface.panelId, surface.outputId};
        const auto outputIt = outputIndices.constFind(surface.outputId);
        if (outputIt == outputIndices.cend()) {
            return failure(PanelSurfacePlanErrorCode::MissingOutput,
                           QStringLiteral("panel '%1' names missing solved output '%2'")
                               .arg(surface.panelId, surface.outputId),
                           identity);
        }
        auto &ids = panelIdsByOutput[surface.outputId];
        if (surface.panelId.trimmed().isEmpty() || ids.contains(surface.panelId)) {
            return failure(PanelSurfacePlanErrorCode::DuplicateSurface,
                           QStringLiteral("duplicate or empty panel identity on output '%1'")
                               .arg(surface.outputId),
                           identity);
        }
        ids.insert(surface.panelId);

        const auto edge = edgeIndex(surface.edge);
        const auto &output = outputs[outputIt.value()].output;
        if (!edge || !validLayer(surface.layer) || !hasSafeExtent(surface.geometry) ||
            !contains(output.geometry, surface.geometry) ||
            surface.reservesWorkArea != layerReservesWorkArea(surface.layer)) {
            return failure(PanelSurfacePlanErrorCode::InvalidSurface,
                           QStringLiteral("panel '%1' has inconsistent solved surface values")
                               .arg(surface.panelId),
                           identity);
        }
        auto &state = outputs[outputIt.value()];
        const qint64 depth = depthFromEdge(surface, state.output);
        state.deepestStack[*edge] = qMax(state.deepestStack[*edge], depth);
        if (!surface.reservesWorkArea) {
            continue;
        }
        if (depth <= state.deepestReservation[*edge]) {
            return failure(PanelSurfacePlanErrorCode::InvalidSurface,
                           QStringLiteral("panel '%1' does not advance its reserved edge")
                               .arg(surface.panelId),
                           identity);
        }
        state.deepestReservation[*edge] = depth;
        state.reservationCarrier[*edge] = surfaceIndex;
    }

    for (const auto &state : std::as_const(outputs)) {
        for (std::size_t edge = 0; edge < edgeCount; ++edge) {
            if (state.expectedReservation[edge] != state.deepestReservation[edge]) {
                return failure(
                    PanelSurfacePlanErrorCode::ReservationMismatch,
                    QStringLiteral("surface reservations do not reproduce work area for output '%1'")
                        .arg(state.output.outputId),
                    {{}, state.output.outputId});
            }
        }
    }

    PanelSurfacePlan result;
    result.surfaces.reserve(layout.surfaces.size());
    for (qsizetype surfaceIndex = 0; surfaceIndex < layout.surfaces.size(); ++surfaceIndex) {
        const auto &surface = layout.surfaces[surfaceIndex];
        const auto outputIndex = outputIndices.value(surface.outputId);
        const auto edge = *edgeIndex(surface.edge);
        const bool carrier = outputs[outputIndex].reservationCarrier[edge] == surfaceIndex;
        if (surfaceIndex > std::numeric_limits<qsizetype>::max() -
                static_cast<qsizetype>(edgeCount)) {
            return failure(PanelSurfacePlanErrorCode::ArithmeticOverflow,
                           QStringLiteral("panel surface placement order overflows"),
                           {surface.panelId, surface.outputId});
        }
        const qsizetype placementOrder = carrier
            ? static_cast<qsizetype>(edge)
            : static_cast<qsizetype>(edgeCount) + surfaceIndex;
        const auto configuration =
            configurationFor(surface, outputs[outputIndex], carrier, placementOrder);
        if (!configuration) {
            return failure(PanelSurfacePlanErrorCode::ArithmeticOverflow,
                           QStringLiteral("panel '%1' margins cannot be represented")
                               .arg(surface.panelId),
                           {surface.panelId, surface.outputId});
        }
        result.surfaces.push_back(*configuration);
    }
    return result;
}

} // namespace QindaQt::ShellSurface
