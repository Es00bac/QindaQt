// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/shell/gather_overview/gather_overview_projection.h"

#include "qindaqt/hybrid_gather/gather_layout.h"

namespace QindaQt::ShellGatherOverview {
namespace {

using ShellTaskListApplet::TaskListAppletPhase;
using ShellTaskListApplet::TaskListAppletRow;

struct PhaseVerdict final {
    bool available = false;
    bool interactive = false;
    // Empty when the phase needs no explanation.
    QString diagnostic;
};

[[nodiscard]] PhaseVerdict verdictFor(TaskListAppletPhase phase,
                                      const QString &phaseReason)
{
    switch (phase) {
    case TaskListAppletPhase::Ready:
    case TaskListAppletPhase::Empty:
        return {true, true, QString()};
    case TaskListAppletPhase::Degraded:
        // Drawable but fenced: the retained generation is the truth the user
        // last saw, and refusing to show it would lose more than it protects.
        return {true, false,
                phaseReason.isEmpty() ? QStringLiteral("source-degraded")
                                      : phaseReason};
    case TaskListAppletPhase::Loading:
        // Cold start. There is no generation yet, so there is nothing to
        // arrange; the T1 producer degrades explicitly once discovery
        // resolves, so this cannot persist silently.
        return {false, false,
                phaseReason.isEmpty() ? QStringLiteral("source-loading")
                                      : phaseReason};
    case TaskListAppletPhase::Unavailable:
        // Observation itself is withheld (read capability denied).
        return {false, false,
                phaseReason.isEmpty() ? QStringLiteral("source-unavailable")
                                      : phaseReason};
    }
    return {false, false, QStringLiteral("source-phase-unknown")};
}

[[nodiscard]] GatherLane laneFor(const TaskListAppletRow &row)
{
    // A container is a card whether or not it is really rolled up: gather
    // presents every container rolled up, and presentation is all this does.
    if (row.kind == ShellTaskList::TaskEntryKind::Container)
        return GatherLane::Card;
    // ADR-0203 iconified windows get the round chip lane. A merely minimized
    // window is not iconified and belongs in the grid with the rest.
    if (row.iconified)
        return GatherLane::Icon;
    return GatherLane::Window;
}

[[nodiscard]] GatherOverviewItem itemFor(const TaskListAppletRow &row,
                                         GatherLane lane)
{
    GatherOverviewItem item;
    item.taskId = row.taskId;
    item.windowId = row.windowId;
    item.lane = lane;
    item.title = row.title;
    item.applicationId = row.applicationId;
    item.applicationName = row.applicationName;
    item.iconText = row.iconText;
    item.colorHex = row.colorHex;
    item.windowCount = row.windowCount;
    item.active = row.active;
    item.urgent = row.urgent;
    item.generationRevision = row.generationRevision;
    item.accessibleName = row.accessibleName;
    return item;
}

} // namespace

GatherOverviewProjection
projectGatherOverview(const GatherOverviewRequest &request)
{
    GatherOverviewProjection projection;

    const PhaseVerdict verdict =
        verdictFor(request.source.phase, request.source.phaseReason);
    projection.diagnostic = verdict.diagnostic;
    if (!verdict.available)
        return projection;

    // Classify first, so the counts are the truth about the session even when
    // the planner later refuses the geometry.
    QVector<GatherOverviewItem> icons;
    QVector<GatherOverviewItem> cards;
    QVector<GatherOverviewItem> windows;
    for (const TaskListAppletRow &row : request.source.rows) {
        // A row without identity cannot be activated, so it is not drawn.
        // Dropping it silently would be the wrong kind of quiet, but a task
        // list that produced one is already broken upstream; the count below
        // still reflects only what is drawable.
        if (row.taskId.isEmpty())
            continue;
        const GatherLane lane = laneFor(row);
        switch (lane) {
        case GatherLane::Icon:
            icons.append(itemFor(row, lane));
            break;
        case GatherLane::Card:
            cards.append(itemFor(row, lane));
            break;
        case GatherLane::Window:
            windows.append(itemFor(row, lane));
            break;
        }
    }

    projection.iconCount = int(icons.size());
    projection.cardCount = int(cards.size());
    projection.windowCount = int(windows.size());
    projection.sourceOverflowCount = request.source.overflowCount;

    HybridGather::GatherRequest plan;
    plan.workArea = request.workArea;
    plan.margin = request.margin;
    plan.gap = request.gap;
    plan.iconExtent = request.iconExtent;
    plan.cardSize = request.cardSize;
    plan.cellSize = request.cellSize;
    plan.scrollOffset = request.scrollOffset;
    plan.iconifiedWindowIds.reserve(icons.size());
    for (const GatherOverviewItem &item : icons)
        plan.iconifiedWindowIds.append(item.taskId);
    plan.containerIds.reserve(cards.size());
    for (const GatherOverviewItem &item : cards)
        plan.containerIds.append(item.taskId);
    plan.windows.reserve(windows.size());
    for (const GatherOverviewItem &item : windows) {
        // No source size: this tree has no window-preview renderer yet
        // (ADR-0119), so a tile fills its cell. When previews land the
        // preview's own size belongs here and the planner aspect-fits it
        // without any change to this policy.
        plan.windows.append(HybridGather::GatherTile{item.taskId, QSizeF()});
    }

    const HybridGather::GatherLayout layout = HybridGather::planGather(plan);
    if (!layout.ok) {
        // The geometry, not the session, is unusable. Report it and draw
        // nothing rather than arrange tiles the planner refused to place.
        projection.diagnostic = layout.diagnostic.isEmpty()
                                    ? QStringLiteral("layout-refused")
                                    : layout.diagnostic;
        projection.iconCount = 0;
        projection.cardCount = 0;
        projection.windowCount = 0;
        projection.sourceOverflowCount = 0;
        return projection;
    }

    projection.available = true;
    projection.interactive = verdict.interactive;
    projection.field = layout.field;
    projection.gridViewport = layout.gridViewport;
    projection.gridColumns = layout.gridColumns;
    projection.gridRows = layout.gridRows;
    projection.gridContentHeight = layout.gridContentHeight;
    projection.maximumScrollOffset = layout.maximumScrollOffset;
    projection.appliedScrollOffset = layout.appliedScrollOffset;
    projection.empty =
        icons.isEmpty() && cards.isEmpty() && windows.isEmpty();

    // The planner returns one placement per input, in input order, so the
    // lanes zip by index. Only visible placements are carried through, and the
    // frame comes from the planner - never recomputed here.
    const auto carry = [&projection](const QVector<GatherOverviewItem> &source,
                                     const QVector<HybridGather::GatherPlacement>
                                         &placements) {
        const int count = int(qMin(source.size(), placements.size()));
        for (int i = 0; i < count; ++i) {
            if (!placements.at(i).visible)
                continue;
            GatherOverviewItem item = source.at(i);
            item.frame = placements.at(i).frame;
            projection.items.append(item);
        }
    };

    carry(icons, layout.icons);
    carry(cards, layout.cards);
    carry(windows, layout.windows);

    int windowsDrawn = 0;
    for (const GatherOverviewItem &item : projection.items) {
        if (item.lane == GatherLane::Window)
            ++windowsDrawn;
    }
    // Measured against everything classified, not against what the planner
    // handed back, so a short placement vector reads as hidden rather than
    // vanishing from both numbers.
    projection.windowsHidden = projection.windowCount - windowsDrawn;

    return projection;
}

} // namespace QindaQt::ShellGatherOverview
