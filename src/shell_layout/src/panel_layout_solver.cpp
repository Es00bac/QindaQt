// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_layout/panel_layout_solver.h"

#include "qindaqt/profiles/profile_validation.h"

#include <QHash>
#include <QSet>
#include <QtMath>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::ShellLayout {
namespace {

constexpr std::size_t edgeCount = 4;

struct OutputAccumulator {
    LogicalOutput output;
    std::array<qint64, edgeCount> stackDepth{};
    std::array<qint64, edgeCount> reservedDepth{};
    std::array<qsizetype, edgeCount> stackCount{};
};

struct PlannedSurface {
    qsizetype panelIndex = 0;
    qsizetype outputIndex = 0;
    qint64 stackOffset = 0;
    qint64 depth = 0;
    qsizetype stackIndex = 0;
    bool reservesWorkArea = false;
};

PanelLayoutResult failure(PanelLayoutErrorCode code, QString message, QString panelId = {},
                          QString outputId = {})
{
    PanelLayoutResult result;
    result.error = {code, std::move(panelId), std::move(outputId), std::move(message)};
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

bool reservesWorkArea(Profiles::Layer layer)
{
    return layer == Profiles::Layer::Normal || layer == Profiles::Layer::Above;
}

template <typename Integer> bool checkedAdd(Integer left, Integer right, Integer *result)
{
    static_assert(std::numeric_limits<Integer>::is_integer);
    if (right > 0 && left > std::numeric_limits<Integer>::max() - right) {
        return false;
    }
    if (right < 0 && left < std::numeric_limits<Integer>::min() - right) {
        return false;
    }
    *result = left + right;
    return true;
}

bool checkedPanelDepth(const Profiles::PanelSpec &panel, qint64 *depth)
{
    const auto rows = static_cast<qint64>(panel.rows);
    const auto thickness = static_cast<qint64>(panel.thickness);
    if (rows <= 0 || thickness <= 0 || rows > std::numeric_limits<qint64>::max() / thickness) {
        return false;
    }
    *depth = rows * thickness;
    return true;
}

bool hasSafeExtent(const QRect &geometry)
{
    if (!geometry.isValid()) {
        return false;
    }

    // QRect can store endpoint coordinates whose inclusive extent exceeds an
    // int. Derive the size in qint64 before calling width()/height(), because
    // those accessors narrow the endpoint difference to int.
    const qint64 width = static_cast<qint64>(geometry.right()) - geometry.left() + 1;
    const qint64 height = static_cast<qint64>(geometry.bottom()) - geometry.top() + 1;
    return width > 0 && width <= std::numeric_limits<int>::max() && height > 0 &&
           height <= std::numeric_limits<int>::max();
}

bool validPanelSpec(const Profiles::PanelSpec &panel)
{
    // AGENT-CONTRACT: Profiles owns typed persistence-value validation;
    // shell_layout owns only expansion and geometry policy. Keep this adapter
    // as the single dependency point if the validator result evolves.
    return Profiles::ProfileValidator::validatePanelLayout(panel).succeeded();
}

qint64 alongExtent(const Profiles::PanelSpec &panel, qint64 available)
{
    if (panel.alignment == Profiles::Alignment::Fill) {
        return available;
    }
    const qint64 rounded = qRound64(static_cast<qreal>(available) * panel.length);
    return qBound<qint64>(1, rounded, available);
}

qint64 alongOffset(Profiles::Alignment alignment, qint64 available, qint64 extent)
{
    switch (alignment) {
    case Profiles::Alignment::Start:
    case Profiles::Alignment::Fill:
        return 0;
    case Profiles::Alignment::Center:
        return (available - extent) / 2;
    case Profiles::Alignment::End:
        return available - extent;
    }
    return 0;
}

std::optional<QRect> checkedRect(qint64 x, qint64 y, qint64 width, qint64 height)
{
    if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() ||
        height > std::numeric_limits<int>::max() || x < std::numeric_limits<int>::min() ||
        x > std::numeric_limits<int>::max() || y < std::numeric_limits<int>::min() ||
        y > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }

    qint64 rightExclusive = 0;
    qint64 bottomExclusive = 0;
    if (!checkedAdd(x, width, &rightExclusive) || !checkedAdd(y, height, &bottomExclusive) ||
        rightExclusive > static_cast<qint64>(std::numeric_limits<int>::max()) + 1 ||
        bottomExclusive > static_cast<qint64>(std::numeric_limits<int>::max()) + 1) {
        return std::nullopt;
    }
    // The QPoint-pair constructor avoids QRect's checked intermediate
    // (origin + extent) overflowing when the inclusive endpoint is INT_MAX.
    return QRect(
        QPoint(static_cast<int>(x), static_cast<int>(y)),
        QPoint(static_cast<int>(rightExclusive - 1), static_cast<int>(bottomExclusive - 1)));
}

bool rectanglesIntersect(const QRect &first, const QRect &second)
{
    return static_cast<qint64>(first.left()) <= second.right() &&
           static_cast<qint64>(second.left()) <= first.right() &&
           static_cast<qint64>(first.top()) <= second.bottom() &&
           static_cast<qint64>(second.top()) <= first.bottom();
}

std::optional<QRect> surfaceGeometry(const Profiles::PanelSpec &panel,
                                     const OutputAccumulator &state, qint64 stackOffset,
                                     qint64 crossExtent)
{
    const QRect &output = state.output.geometry;
    const bool horizontal =
        panel.edge == Profiles::Edge::Top || panel.edge == Profiles::Edge::Bottom;
    qint64 available = output.width();
    qint64 laneStart = output.y();
    if (!horizontal) {
        available =
            static_cast<qint64>(output.height()) - state.stackDepth[0] - state.stackDepth[1];
        laneStart = static_cast<qint64>(output.y()) + state.stackDepth[0];
    }
    if (available <= 0) {
        return std::nullopt;
    }
    const qint64 extent = alongExtent(panel, available);
    const qint64 offset = alongOffset(panel.alignment, available, extent);

    switch (panel.edge) {
    case Profiles::Edge::Top:
        return checkedRect(static_cast<qint64>(output.x()) + offset,
                           static_cast<qint64>(output.y()) + stackOffset, extent, crossExtent);
    case Profiles::Edge::Bottom:
        return checkedRect(static_cast<qint64>(output.x()) + offset,
                           static_cast<qint64>(output.y()) + output.height() - stackOffset -
                               crossExtent,
                           extent, crossExtent);
    case Profiles::Edge::Left:
        return checkedRect(static_cast<qint64>(output.x()) + stackOffset, laneStart + offset,
                           crossExtent, extent);
    case Profiles::Edge::Right:
        return checkedRect(static_cast<qint64>(output.x()) + output.width() - stackOffset -
                               crossExtent,
                           laneStart + offset, crossExtent, extent);
    }
    return std::nullopt;
}

std::optional<bool> outputIsOverConstrained(const OutputAccumulator &state)
{
    qint64 verticalSurfaces = 0;
    qint64 horizontalSurfaces = 0;
    qint64 verticalReservation = 0;
    qint64 horizontalReservation = 0;
    if (!checkedAdd(state.stackDepth[0], state.stackDepth[1], &verticalSurfaces) ||
        !checkedAdd(state.stackDepth[2], state.stackDepth[3], &horizontalSurfaces) ||
        !checkedAdd(state.reservedDepth[0], state.reservedDepth[1], &verticalReservation) ||
        !checkedAdd(state.reservedDepth[2], state.reservedDepth[3], &horizontalReservation)) {
        return std::nullopt;
    }
    return verticalSurfaces >= state.output.geometry.height() ||
           horizontalSurfaces >= state.output.geometry.width() ||
           verticalReservation >= state.output.geometry.height() ||
           horizontalReservation >= state.output.geometry.width();
}

std::optional<QRect> workArea(const OutputAccumulator &state)
{
    const qint64 top = state.reservedDepth[0];
    const qint64 bottom = state.reservedDepth[1];
    const qint64 left = state.reservedDepth[2];
    const qint64 right = state.reservedDepth[3];
    return checkedRect(static_cast<qint64>(state.output.geometry.x()) + left,
                       static_cast<qint64>(state.output.geometry.y()) + top,
                       static_cast<qint64>(state.output.geometry.width()) - left - right,
                       static_cast<qint64>(state.output.geometry.height()) - top - bottom);
}

std::optional<qsizetype> expandedSurfaceCount(const QVector<Profiles::PanelSpec> &panels,
                                              qsizetype outputCount)
{
    qsizetype count = 0;
    for (const auto &panel : panels) {
        const qsizetype targetCount = panel.output == QStringLiteral("*") ? outputCount : 1;
        if (!checkedAdd(count, targetCount, &count)) {
            return std::nullopt;
        }
    }
    return count;
}

} // namespace

PanelLayoutResult PanelLayoutSolver::solve(const QVector<Profiles::PanelSpec> &panels,
                                           const QVector<LogicalOutput> &outputs)
{
    if (outputs.isEmpty()) {
        return failure(PanelLayoutErrorCode::EmptyOutputInventory,
                       QStringLiteral("the logical output inventory is empty"));
    }

    QVector<OutputAccumulator> stagedOutputs;
    stagedOutputs.reserve(outputs.size());
    QHash<QString, qsizetype> outputIndices;
    for (const auto &output : outputs) {
        if (output.id.trimmed().isEmpty()) {
            return failure(PanelLayoutErrorCode::InvalidOutputId,
                           QStringLiteral("an output has an empty id"));
        }
        if (outputIndices.contains(output.id)) {
            return failure(PanelLayoutErrorCode::DuplicateOutputId,
                           QStringLiteral("duplicate output id '%1'").arg(output.id), {},
                           output.id);
        }
        if (!hasSafeExtent(output.geometry)) {
            return failure(
                PanelLayoutErrorCode::InvalidOutputGeometry,
                QStringLiteral("output '%1' has invalid logical geometry").arg(output.id), {},
                output.id);
        }
        if (!std::isfinite(output.scale) || output.scale <= 0.0) {
            return failure(PanelLayoutErrorCode::InvalidOutputScale,
                           QStringLiteral("output '%1' has invalid scale").arg(output.id), {},
                           output.id);
        }
        outputIndices.insert(output.id, stagedOutputs.size());
        stagedOutputs.push_back({output});
    }

    const auto expandedCount = expandedSurfaceCount(panels, stagedOutputs.size());
    if (!expandedCount) {
        return failure(PanelLayoutErrorCode::ArithmeticOverflow,
                       QStringLiteral("expanded panel surface count overflows"));
    }

    QVector<PlannedSurface> plannedSurfaces;
    plannedSurfaces.reserve(*expandedCount);
    QHash<QString, QSet<QString>> expandedPanelIds;
    for (qsizetype panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
        const auto &panel = panels[panelIndex];
        if (!validPanelSpec(panel)) {
            return failure(PanelLayoutErrorCode::InvalidPanel,
                           QStringLiteral("panel '%1' has invalid layout values").arg(panel.id),
                           panel.id);
        }

        QVector<qsizetype> targets;
        if (panel.output == QStringLiteral("*")) {
            targets.reserve(stagedOutputs.size());
            for (qsizetype index = 0; index < stagedOutputs.size(); ++index) {
                targets.push_back(index);
            }
        } else {
            const auto outputIt = outputIndices.constFind(panel.output);
            if (outputIt == outputIndices.cend()) {
                return failure(PanelLayoutErrorCode::MissingOutput,
                               QStringLiteral("panel '%1' names missing output '%2'")
                                   .arg(panel.id, panel.output),
                               panel.id, panel.output);
            }
            targets.push_back(outputIt.value());
        }

        for (const qsizetype target : targets) {
            auto &state = stagedOutputs[target];
            auto &idsOnOutput = expandedPanelIds[state.output.id];
            if (idsOnOutput.contains(panel.id)) {
                return failure(PanelLayoutErrorCode::DuplicatePanelInstance,
                               QStringLiteral("panel '%1' expands more than once on output '%2'")
                                   .arg(panel.id, state.output.id),
                               panel.id, state.output.id);
            }
            idsOnOutput.insert(panel.id);

            const auto index = edgeIndex(panel.edge);
            // AGENT-GUARD: Profiles are untrusted persisted values. Recheck at
            // the indexing site even though validPanelSpec() already did so; this
            // keeps optimized builds from turning an invalid enum sentinel
            // into an out-of-bounds std::array access.
            if (!index) {
                return failure(PanelLayoutErrorCode::InvalidPanel,
                               QStringLiteral("panel '%1' has an invalid edge").arg(panel.id),
                               panel.id, state.output.id);
            }
            qint64 depth = 0;
            qint64 nextDepth = 0;
            if (!checkedPanelDepth(panel, &depth) ||
                !checkedAdd(state.stackDepth[*index], depth, &nextDepth)) {
                return failure(PanelLayoutErrorCode::ArithmeticOverflow,
                               QStringLiteral("panel '%1' overflows an edge stack on output '%2'")
                                   .arg(panel.id, state.output.id),
                               panel.id, state.output.id);
            }
            const bool reserved = reservesWorkArea(panel.layer);
            plannedSurfaces.push_back({panelIndex, target, state.stackDepth[*index], depth,
                                       state.stackCount[*index], reserved});
            state.stackDepth[*index] = nextDepth;
            if (state.stackCount[*index] == std::numeric_limits<qsizetype>::max()) {
                return failure(
                    PanelLayoutErrorCode::ArithmeticOverflow,
                    QStringLiteral("panel count overflows on output '%1'").arg(state.output.id),
                    panel.id, state.output.id);
            }
            ++state.stackCount[*index];
            if (reserved) {
                // AGENT-GUARD: Reservation reaches the panel's actual inner
                // boundary. Earlier overlay/below surfaces share this stacking
                // lane, so summing only reserving thicknesses would let normal
                // windows overlap an Above/Normal surface.
                state.reservedDepth[*index] = state.stackDepth[*index];
            }
        }
    }

    for (const auto &state : stagedOutputs) {
        const auto overConstrained = outputIsOverConstrained(state);
        if (!overConstrained) {
            return failure(
                PanelLayoutErrorCode::ArithmeticOverflow,
                QStringLiteral("panel depths overflow on output '%1'").arg(state.output.id), {},
                state.output.id);
        }
        if (*overConstrained) {
            return failure(PanelLayoutErrorCode::OverConstrainedOutput,
                           QStringLiteral("panels leave no usable logical space on output '%1'")
                               .arg(state.output.id),
                           {}, state.output.id);
        }
    }

    QVector<PanelSurface> stagedSurfaces;
    stagedSurfaces.reserve(*expandedCount);
    for (const auto &planned : plannedSurfaces) {
        const auto &panel = panels[planned.panelIndex];
        const auto &state = stagedOutputs[planned.outputIndex];
        const auto geometry = surfaceGeometry(panel, state, planned.stackOffset, planned.depth);
        if (!geometry) {
            return failure(
                PanelLayoutErrorCode::ArithmeticOverflow,
                QStringLiteral("panel '%1' geometry cannot be represented").arg(panel.id), panel.id,
                state.output.id);
        }
        for (const auto &existing : std::as_const(stagedSurfaces)) {
            if (existing.outputId == state.output.id &&
                rectanglesIntersect(existing.geometry, *geometry)) {
                return failure(PanelLayoutErrorCode::OverConstrainedOutput,
                               QStringLiteral("panels '%1' and '%2' overlap on output '%3'")
                                   .arg(existing.panelId, panel.id, state.output.id),
                               panel.id, state.output.id);
            }
        }
        stagedSurfaces.push_back({panel.id, state.output.id, *geometry, panel.edge, panel.layer,
                                  planned.stackIndex, planned.reservesWorkArea});
    }

    PanelLayoutResult result;
    result.surfaces = std::move(stagedSurfaces);
    result.outputs.reserve(stagedOutputs.size());
    for (const auto &state : stagedOutputs) {
        const auto availableWorkArea = workArea(state);
        if (!availableWorkArea) {
            return failure(PanelLayoutErrorCode::ArithmeticOverflow,
                           QStringLiteral("work area cannot be represented on output '%1'")
                               .arg(state.output.id),
                           {}, state.output.id);
        }
        result.outputs.push_back(
            {state.output.id, state.output.geometry, *availableWorkArea, state.output.scale});
    }
    // AGENT-CONTRACT: Profiles supply compositor-logical geometry and this
    // module never multiplies it by output scale. The platform surface adapter
    // is responsible for the single logical-to-buffer conversion.
    return result;
}

} // namespace QindaQt::ShellLayout
