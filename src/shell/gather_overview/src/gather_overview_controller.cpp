// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/shell/gather_overview/gather_overview_controller.h"

#include <QVariantList>

#include <utility>

namespace QindaQt::ShellGatherOverview {
namespace {

[[nodiscard]] QString laneName(GatherLane lane)
{
    switch (lane) {
    case GatherLane::Icon:
        return QStringLiteral("icon");
    case GatherLane::Card:
        return QStringLiteral("card");
    case GatherLane::Window:
        return QStringLiteral("window");
    }
    // Unreachable for a valid enum; a new lane must be named here, and an
    // empty string would silently draw it as a window tile.
    return QStringLiteral("unknown");
}

} // namespace

QVariantMap gatherOverviewProjectionMap(
    const GatherOverviewProjection &projection,
    const GatherOverviewController::IconNameResolver &iconNameResolver)
{
    QVariantMap map;
    map.insert(QStringLiteral("available"), projection.available);
    map.insert(QStringLiteral("interactive"), projection.interactive);
    map.insert(QStringLiteral("empty"), projection.empty);
    map.insert(QStringLiteral("diagnostic"), projection.diagnostic);
    map.insert(QStringLiteral("field"), projection.field);
    map.insert(QStringLiteral("gridViewport"), projection.gridViewport);
    map.insert(QStringLiteral("gridColumns"), projection.gridColumns);
    map.insert(QStringLiteral("gridRows"), projection.gridRows);
    map.insert(QStringLiteral("gridContentHeight"), projection.gridContentHeight);
    map.insert(QStringLiteral("maximumScrollOffset"),
               projection.maximumScrollOffset);
    map.insert(QStringLiteral("appliedScrollOffset"),
               projection.appliedScrollOffset);
    map.insert(QStringLiteral("iconCount"), projection.iconCount);
    map.insert(QStringLiteral("cardCount"), projection.cardCount);
    map.insert(QStringLiteral("windowCount"), projection.windowCount);
    map.insert(QStringLiteral("windowsHidden"), projection.windowsHidden);
    map.insert(QStringLiteral("sourceOverflowCount"),
               projection.sourceOverflowCount);

    QVariantList items;
    items.reserve(projection.items.size());
    for (const GatherOverviewItem &item : projection.items) {
        QVariantMap entry;
        entry.insert(QStringLiteral("taskId"), item.taskId);
        entry.insert(QStringLiteral("windowId"), item.windowId);
        entry.insert(QStringLiteral("lane"), laneName(item.lane));
        entry.insert(QStringLiteral("frame"), item.frame);
        entry.insert(QStringLiteral("title"), item.title);
        entry.insert(QStringLiteral("applicationId"), item.applicationId);
        entry.insert(QStringLiteral("applicationName"), item.applicationName);
        // A container's glyph is the shell's symbolic container icon, not an
        // application's: the same split the task-list applet's controller
        // makes. Everything else goes through the injected resolver, and a
        // null resolver leaves the name empty so the surface falls back to the
        // one-letter badge.
        entry.insert(QStringLiteral("iconName"),
                     item.lane == GatherLane::Card
                         ? QStringLiteral("window-duplicate-symbolic")
                     : iconNameResolver ? iconNameResolver(item.applicationId)
                                        : QString());
        entry.insert(QStringLiteral("iconText"), item.iconText);
        entry.insert(QStringLiteral("colorHex"), item.colorHex);
        entry.insert(QStringLiteral("windowCount"), item.windowCount);
        entry.insert(QStringLiteral("active"), item.active);
        entry.insert(QStringLiteral("urgent"), item.urgent);
        entry.insert(QStringLiteral("generationRevision"),
                     QVariant::fromValue(item.generationRevision));
        entry.insert(QStringLiteral("accessibleName"), item.accessibleName);
        items.append(entry);
    }
    map.insert(QStringLiteral("items"), items);
    return map;
}

GatherOverviewController::GatherOverviewController(
    IconNameResolver iconNameResolver, QObject *parent)
    : QObject(parent)
    , m_iconNameResolver(std::move(iconNameResolver))
{
}

GatherOverviewController::~GatherOverviewController() = default;

void GatherOverviewController::setSource(
    const ShellTaskListApplet::TaskListAppletProjection &source)
{
    m_source = source;
    // A closed overview projects nothing, so window churn costs no work.
    if (m_open)
        reproject();
}

void GatherOverviewController::setWorkArea(const QRectF &workArea)
{
    if (m_knobs.workArea == workArea)
        return;
    m_knobs.workArea = workArea;
    if (m_open)
        reproject();
}

void GatherOverviewController::setGeometry(const GatherOverviewRequest &knobs)
{
    // The caller's source and scroll position are this object's, not theirs.
    const QRectF workArea = knobs.workArea;
    const qreal scrollOffset = m_knobs.scrollOffset;
    m_knobs = knobs;
    m_knobs.workArea = workArea;
    m_knobs.scrollOffset = scrollOffset;
    m_knobs.source = {};
    if (m_open)
        reproject();
}

QVariantMap GatherOverviewController::projection() const
{
    return m_projectionMap;
}

bool GatherOverviewController::isOpen() const noexcept { return m_open; }

void GatherOverviewController::open()
{
    if (m_open)
        return;
    m_open = true;
    // A fresh overview starts at the top. A remembered offset from the last
    // time it was open is never what the user wants now.
    m_knobs.scrollOffset = 0;
    reproject();
    Q_EMIT openChanged();
}

void GatherOverviewController::close()
{
    if (!m_open)
        return;
    m_open = false;
    m_projection = {};
    m_projectionMap = gatherOverviewProjectionMap(m_projection);
    Q_EMIT projectionChanged();
    Q_EMIT openChanged();
    Q_EMIT dismissed();
}

void GatherOverviewController::toggle()
{
    if (m_open)
        close();
    else
        open();
}

void GatherOverviewController::scrollBy(qreal delta)
{
    if (!m_open || qFuzzyIsNull(delta))
        return;
    // Add and re-project: the planner clamps, and a second clamp here would
    // eventually disagree with it. Start from what the planner actually
    // applied, not from what was last requested, so a delta at the end of the
    // range cannot accumulate an offset the user can never scroll back out of.
    m_knobs.scrollOffset = m_projection.appliedScrollOffset + delta;
    reproject();
}

void GatherOverviewController::activate(const QVariantMap &item)
{
    if (!m_open || !m_projection.available || !m_projection.interactive)
        return;

    const QString taskId = item.value(QStringLiteral("taskId")).toString();
    if (taskId.isEmpty())
        return;

    // The item must be one the current projection actually holds. A stale
    // object held across a re-projection, or a QML mistake, must not act.
    const GatherOverviewItem *match = nullptr;
    const QString windowId = item.value(QStringLiteral("windowId")).toString();
    for (const GatherOverviewItem &candidate : m_projection.items) {
        if (candidate.taskId == taskId && candidate.windowId == windowId) {
            match = &candidate;
            break;
        }
    }
    if (match == nullptr)
        return;

    // The generation is the projection's, never the caller's: echoing a
    // revision the caller supplied would defeat the stale-revision
    // arbitration it exists for.
    Q_EMIT activationRequested(match->taskId, match->windowId,
                               match->generationRevision);
}

void GatherOverviewController::reproject()
{
    GatherOverviewRequest request = m_knobs;
    request.source = m_source;
    m_projection = projectGatherOverview(request);
    // Keep the knob in step with what the planner allowed, so the next
    // `scrollBy` starts from a real position.
    m_knobs.scrollOffset = m_projection.appliedScrollOffset;
    m_projectionMap =
        gatherOverviewProjectionMap(m_projection, m_iconNameResolver);
    Q_EMIT projectionChanged();
}

} // namespace QindaQt::ShellGatherOverview
