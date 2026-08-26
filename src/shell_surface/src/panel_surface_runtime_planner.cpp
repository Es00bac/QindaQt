// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/panel_surface_runtime_planner.h"

#include <QHash>
#include <QSet>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::ShellSurface {
namespace {

constexpr std::size_t EdgeCount = 4;
constexpr qsizetype CarrierPlacementCount = 4;

using CarrierIndexes = std::array<std::optional<qsizetype>, EdgeCount>;
using CarrierRanks = std::array<qint64, EdgeCount>;
using EdgeDepths = std::array<qint64, EdgeCount>;
using KnownPanels = QHash<QString, QSet<QString>>;
using DecisionsByOutput =
    QHash<QString, QHash<QString, PanelSurfaceRuntimeDecision>>;

PanelSurfaceRuntimePlanResult failure(
    PanelSurfaceRuntimePlanErrorCode code, QString message,
    PanelSurfaceIdentity identity = {})
{
    return {{}, {code, std::move(identity), std::move(message)}};
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

bool validMapping(PanelSurfaceMapping mapping)
{
    switch (mapping) {
    case PanelSurfaceMapping::Mapped:
    case PanelSurfaceMapping::Unmapped:
        return true;
    }
    return false;
}

bool layerReservesWorkArea(Profiles::Layer layer)
{
    return layer == Profiles::Layer::Normal || layer == Profiles::Layer::Above;
}

SurfaceAnchor edgeAnchor(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
        return SurfaceAnchor::Top;
    case Profiles::Edge::Bottom:
        return SurfaceAnchor::Bottom;
    case Profiles::Edge::Left:
        return SurfaceAnchor::Left;
    case Profiles::Edge::Right:
        return SurfaceAnchor::Right;
    }
    return static_cast<SurfaceAnchor>(0);
}

bool nonNegativeMargins(const QMargins &margins)
{
    return margins.left() >= 0 && margins.top() >= 0 && margins.right() >= 0 &&
        margins.bottom() >= 0;
}

bool validDesiredSize(const PanelSurfaceConfiguration &surface)
{
    if (surface.desiredSize.width() < 0 || surface.desiredSize.height() < 0) {
        return false;
    }
    if (surface.desiredSize.width() == 0 &&
        !(surface.anchors.testFlag(SurfaceAnchor::Left) &&
          surface.anchors.testFlag(SurfaceAnchor::Right))) {
        return false;
    }
    if (surface.desiredSize.height() == 0 &&
        !(surface.anchors.testFlag(SurfaceAnchor::Top) &&
          surface.anchors.testFlag(SurfaceAnchor::Bottom))) {
        return false;
    }
    return surface.desiredSize.width() != 0 || surface.desiredSize.height() != 0;
}

int thickness(const PanelSurfaceConfiguration &surface)
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

bool validBaseSurface(const PanelSurfaceConfiguration &surface)
{
    const auto edge = edgeIndex(surface.edge);
    const int surfaceThickness = thickness(surface);
    if (surface.identity.panelId.trimmed().isEmpty() ||
        surface.identity.outputId.trimmed().isEmpty() || !edge ||
        !validLayer(surface.layer) || surface.exclusiveEdge != surface.edge ||
        surface.mapping != PanelSurfaceMapping::Mapped ||
        !surface.outputGeometry.isValid() || !surface.geometry.isValid() ||
        !surface.outputGeometry.contains(surface.geometry) ||
        !std::isfinite(surface.outputScale) || surface.outputScale <= 0.0 ||
        surface.outputScale > 16.0 || !nonNegativeMargins(surface.margins) ||
        !validDesiredSize(surface) || surfaceThickness <= 0 ||
        surface.placementOrder < 0 ||
        !surface.anchors.testFlag(edgeAnchor(surface.edge)) ||
        surface.reservesWorkArea != layerReservesWorkArea(surface.layer)) {
        return false;
    }
    if (surface.reservationCarrier) {
        return surface.reservesWorkArea && surface.exclusiveZone == surfaceThickness;
    }
    return surface.exclusiveZone == -1;
}

qint64 depthRank(const PanelSurfaceConfiguration &surface)
{
    switch (surface.edge) {
    case Profiles::Edge::Top:
        return surface.geometry.bottom();
    case Profiles::Edge::Bottom:
        return -static_cast<qint64>(surface.geometry.top());
    case Profiles::Edge::Left:
        return surface.geometry.right();
    case Profiles::Edge::Right:
        return -static_cast<qint64>(surface.geometry.left());
    }
    return std::numeric_limits<qint64>::min();
}

qint64 reservationDepth(const PanelSurfaceConfiguration &surface)
{
    const auto &output = surface.outputGeometry;
    switch (surface.edge) {
    case Profiles::Edge::Top:
        return static_cast<qint64>(surface.geometry.bottom()) - output.top() + 1;
    case Profiles::Edge::Bottom:
        return static_cast<qint64>(output.bottom()) - surface.geometry.top() + 1;
    case Profiles::Edge::Left:
        return static_cast<qint64>(surface.geometry.right()) - output.left() + 1;
    case Profiles::Edge::Right:
        return static_cast<qint64>(output.right()) - surface.geometry.left() + 1;
    }
    return -1;
}

std::optional<int> checkedMargin(qint64 value)
{
    if (value < 0 || value > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

PanelSurfaceRuntimePlanResult invalidBase(
    const PanelSurfaceConfiguration &surface, QString reason)
{
    return failure(PanelSurfaceRuntimePlanErrorCode::InvalidSurface,
                   QStringLiteral("base panel '%1' on '%2' %3")
                       .arg(surface.identity.panelId, surface.identity.outputId,
                            std::move(reason)),
                   surface.identity);
}

std::optional<PanelSurfaceRuntimePlanResult> validateAndIndexBasePlan(
    const PanelSurfacePlan &basePlan, KnownPanels *known)
{
    QHash<QString, QRect> outputGeometries;
    QHash<QString, qreal> outputScales;
    for (const auto &surface : basePlan.surfaces) {
        if (!validBaseSurface(surface)) {
            return invalidBase(surface, QStringLiteral("has invalid static protocol state"));
        }
        auto &panelIds = (*known)[surface.identity.outputId];
        if (panelIds.contains(surface.identity.panelId)) {
            return failure(
                PanelSurfaceRuntimePlanErrorCode::DuplicateBaseSurface,
                QStringLiteral("base plan duplicates panel '%1' on '%2'")
                    .arg(surface.identity.panelId, surface.identity.outputId),
                surface.identity);
        }
        panelIds.insert(surface.identity.panelId);

        const auto geometry = outputGeometries.constFind(surface.identity.outputId);
        if (geometry != outputGeometries.cend() &&
            geometry.value() != surface.outputGeometry) {
            return invalidBase(surface,
                               QStringLiteral("disagrees about its output geometry"));
        }
        const auto scale = outputScales.constFind(surface.identity.outputId);
        if (scale != outputScales.cend() && scale.value() != surface.outputScale) {
            return invalidBase(surface, QStringLiteral("disagrees about its output scale"));
        }
        outputGeometries.insert(surface.identity.outputId, surface.outputGeometry);
        outputScales.insert(surface.identity.outputId, surface.outputScale);
    }
    return std::nullopt;
}

std::optional<PanelSurfaceRuntimePlanResult> validateAndIndexDecisions(
    const QVector<PanelSurfaceRuntimeDecision> &decisions,
    const KnownPanels &known, DecisionsByOutput *byOutput)
{
    for (const auto &decision : decisions) {
        if (decision.identity.panelId.trimmed().isEmpty() ||
            decision.identity.outputId.trimmed().isEmpty()) {
            return failure(PanelSurfaceRuntimePlanErrorCode::InvalidDecisionIdentity,
                           QStringLiteral("a runtime decision has an empty identity"),
                           decision.identity);
        }
        if (!validMapping(decision.mapping)) {
            return failure(PanelSurfaceRuntimePlanErrorCode::InvalidDecisionState,
                           QStringLiteral("panel '%1' has an invalid mapping state")
                               .arg(decision.identity.panelId),
                           decision.identity);
        }
        if (decision.mapping == PanelSurfaceMapping::Unmapped && decision.reserve) {
            return failure(PanelSurfaceRuntimePlanErrorCode::HiddenSurfaceReservation,
                           QStringLiteral("hidden panel '%1' cannot reserve work area")
                               .arg(decision.identity.panelId),
                           decision.identity);
        }
        auto &outputDecisions = (*byOutput)[decision.identity.outputId];
        if (outputDecisions.contains(decision.identity.panelId)) {
            return failure(PanelSurfaceRuntimePlanErrorCode::DuplicateDecision,
                           QStringLiteral("duplicate runtime decision for panel '%1' on '%2'")
                               .arg(decision.identity.panelId,
                                    decision.identity.outputId),
                           decision.identity);
        }
        if (!known.value(decision.identity.outputId)
                 .contains(decision.identity.panelId)) {
            return failure(PanelSurfaceRuntimePlanErrorCode::UnknownDecision,
                           QStringLiteral("runtime decision names unknown panel '%1' on '%2'")
                               .arg(decision.identity.panelId,
                                    decision.identity.outputId),
                           decision.identity);
        }
        outputDecisions.insert(decision.identity.panelId, decision);
    }
    return std::nullopt;
}

} // namespace

PanelSurfaceRuntimePlanResult PanelSurfaceRuntimePlanner::apply(
    const PanelSurfacePlan &basePlan,
    const QVector<PanelSurfaceRuntimeDecision> &decisions)
{
    if (!basePlan.ok()) {
        return failure(PanelSurfaceRuntimePlanErrorCode::InvalidBasePlan,
                       QStringLiteral("cannot apply runtime state to a rejected base plan"));
    }

    // AGENT-GUARD: PanelSurfacePlan is a public value, not an opaque validated
    // token. Revalidate every invariant used below so a forged plan cannot
    // smuggle duplicate roles or invalid exclusive-zone state to the backend.
    KnownPanels known;
    if (const auto rejected = validateAndIndexBasePlan(basePlan, &known)) {
        return *rejected;
    }

    DecisionsByOutput byOutput;
    if (const auto rejected = validateAndIndexDecisions(decisions, known, &byOutput)) {
        return *rejected;
    }

    PanelSurfacePlan candidate = basePlan;
    QHash<QString, CarrierIndexes> carriers;
    QHash<QString, CarrierRanks> carrierRanks;
    for (auto output = known.cbegin(); output != known.cend(); ++output) {
        carriers.insert(output.key(), CarrierIndexes{});
        carrierRanks.insert(output.key(), CarrierRanks{});
    }

    for (qsizetype index = 0; index < candidate.surfaces.size(); ++index) {
        auto &surface = candidate.surfaces[index];
        const auto decisionsForOutput = byOutput.constFind(surface.identity.outputId);
        if (decisionsForOutput == byOutput.cend() ||
            !decisionsForOutput->contains(surface.identity.panelId)) {
            return failure(PanelSurfaceRuntimePlanErrorCode::MissingDecision,
                           QStringLiteral("base panel '%1' on '%2' has no runtime decision")
                               .arg(surface.identity.panelId,
                                    surface.identity.outputId),
                           surface.identity);
        }
        const auto decision = decisionsForOutput->value(surface.identity.panelId);
        if (decision.reserve && !surface.reservesWorkArea) {
            return failure(
                PanelSurfaceRuntimePlanErrorCode::IneligibleSurfaceReservation,
                QStringLiteral("panel '%1' is not eligible to reserve work area")
                    .arg(surface.identity.panelId),
                surface.identity);
        }

        surface.mapping = decision.mapping;
        surface.reservationCarrier = false;
        surface.exclusiveZone = -1;
        if (decision.mapping == PanelSurfaceMapping::Unmapped || !decision.reserve) {
            continue;
        }

        const std::size_t edge = *edgeIndex(surface.edge);
        auto &outputCarriers = carriers[surface.identity.outputId];
        auto &outputRanks = carrierRanks[surface.identity.outputId];
        const qint64 rank = depthRank(surface);
        if (!outputCarriers[edge].has_value() || rank > outputRanks[edge]) {
            outputCarriers[edge] = index;
            outputRanks[edge] = rank;
        }
    }

    for (auto output = carriers.cbegin(); output != carriers.cend(); ++output) {
        for (std::size_t edge = 0; edge < EdgeCount; ++edge) {
            if (!output.value()[edge].has_value()) {
                continue;
            }
            auto &carrier = candidate.surfaces[*output.value()[edge]];
            carrier.reservationCarrier = true;
            carrier.exclusiveZone = thickness(carrier);
        }
    }

    QHash<QString, EdgeDepths> depthsByOutput;
    for (auto output = carriers.cbegin(); output != carriers.cend(); ++output) {
        EdgeDepths depths{};
        for (std::size_t edge = 0; edge < EdgeCount; ++edge) {
            const auto carrierIndex = output.value()[edge];
            if (carrierIndex.has_value()) {
                depths[edge] = reservationDepth(candidate.surfaces[*carrierIndex]);
            }
        }
        depthsByOutput.insert(output.key(), depths);
    }

    // AGENT-CONTRACT: Positive-zone side roles are positioned inside current
    // horizontal reservations, while noncarriers are positioned from the full
    // output. Recompute every side role—not only the new carrier—so demotion
    // cannot move an otherwise unchanged panel rectangle.
    for (auto &surface : candidate.surfaces) {
        if (surface.edge != Profiles::Edge::Left &&
            surface.edge != Profiles::Edge::Right) {
            continue;
        }
        const auto depths = depthsByOutput.value(surface.identity.outputId);
        const qint64 topBase = static_cast<qint64>(surface.outputGeometry.top()) +
            (surface.reservationCarrier ? depths[0] : 0);
        const qint64 bottomBase = static_cast<qint64>(surface.outputGeometry.bottom()) -
            (surface.reservationCarrier ? depths[1] : 0);
        const auto top = checkedMargin(
            static_cast<qint64>(surface.geometry.top()) - topBase);
        if (!top) {
            return failure(PanelSurfaceRuntimePlanErrorCode::InvalidSurface,
                           QStringLiteral("panel '%1' cannot preserve its runtime top margin")
                               .arg(surface.identity.panelId),
                           surface.identity);
        }
        surface.margins.setTop(*top);
        if (surface.anchors.testFlag(SurfaceAnchor::Bottom)) {
            const auto bottom = checkedMargin(
                bottomBase - static_cast<qint64>(surface.geometry.bottom()));
            if (!bottom) {
                return failure(
                    PanelSurfaceRuntimePlanErrorCode::InvalidSurface,
                    QStringLiteral("panel '%1' cannot preserve its runtime bottom margin")
                        .arg(surface.identity.panelId),
                    surface.identity);
            }
            surface.margins.setBottom(*bottom);
        }
    }

    for (qsizetype index = 0; index < candidate.surfaces.size(); ++index) {
        auto &surface = candidate.surfaces[index];
        if (surface.reservationCarrier) {
            surface.placementOrder = static_cast<qsizetype>(*edgeIndex(surface.edge));
            continue;
        }
        if (index > std::numeric_limits<qsizetype>::max() - CarrierPlacementCount) {
            return failure(PanelSurfaceRuntimePlanErrorCode::ArithmeticOverflow,
                           QStringLiteral("runtime panel order overflows"),
                           surface.identity);
        }
        surface.placementOrder = CarrierPlacementCount + index;
    }
    return {std::move(candidate), {}};
}

} // namespace QindaQt::ShellSurface
