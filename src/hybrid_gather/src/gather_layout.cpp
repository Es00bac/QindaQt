// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_gather/gather_layout.h"

#include <algorithm>
#include <cmath>

namespace QindaQt::HybridGather {
namespace {

[[nodiscard]] bool finite(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

[[nodiscard]] bool finite(const QSizeF &size)
{
    return finite(size.width()) && finite(size.height());
}

[[nodiscard]] bool finite(const QRectF &rect)
{
    return finite(rect.x()) && finite(rect.y()) && finite(rect.width())
        && finite(rect.height());
}

[[nodiscard]] GatherLayout refuse(const QString &diagnostic)
{
    GatherLayout layout;
    layout.ok = false;
    layout.diagnostic = diagnostic;
    return layout;
}

// One lane of identically sized items, stacked down the field and wrapping
// into a new column to the right when a column is full.
//
// AGENT-GUARD: `columnCapacity` is at least one even when the item is taller
// than the field. Zero capacity would divide by zero and, worse, silently drop
// every item in the lane; one over-tall item clipped by the field edge is the
// visible, debuggable outcome.
struct Lane final
{
    QVector<GatherPlacement> placements;
    qreal width = 0;
};

[[nodiscard]] Lane stackLane(const QVector<QString> &ids, const QSizeF &itemSize,
                             qreal laneLeft, const QRectF &field, qreal gap)
{
    Lane lane;
    if (ids.isEmpty()) {
        return lane;
    }
    const qreal pitch = itemSize.height() + gap;
    const auto capacity = std::max<qsizetype>(
        1, static_cast<qsizetype>(std::floor((field.height() + gap) / pitch)));
    lane.placements.reserve(ids.size());
    for (qsizetype index = 0; index < ids.size(); ++index) {
        const qsizetype column = index / capacity;
        const qsizetype row = index % capacity;
        const qreal x = laneLeft + static_cast<qreal>(column) * (itemSize.width() + gap);
        const qreal y = field.top() + static_cast<qreal>(row) * pitch;
        lane.placements.append(GatherPlacement{
            ids.at(index), QRectF(QPointF(x, y), itemSize),
            x + itemSize.width() <= field.right() + 0.5});
    }
    const qsizetype columns = (ids.size() + capacity - 1) / capacity;
    lane.width = static_cast<qreal>(columns) * itemSize.width()
        + static_cast<qreal>(columns - 1) * gap;
    return lane;
}

// Fit `source` inside `cell` without distorting it and without magnifying it:
// a 3840x2160 window shrinks to fit, a 200x100 one keeps its size. Upscaling a
// small window to fill its cell only produces a blurry preview.
[[nodiscard]] QRectF fitInside(const QSizeF &source, const QRectF &cell)
{
    if (!(source.width() > 0) || !(source.height() > 0) || !finite(source)) {
        return cell;
    }
    const qreal scale = std::min({qreal(1), cell.width() / source.width(),
                                  cell.height() / source.height()});
    const QSizeF fitted(source.width() * scale, source.height() * scale);
    return QRectF(QPointF(cell.x() + (cell.width() - fitted.width()) / 2,
                          cell.y() + (cell.height() - fitted.height()) / 2),
                  fitted);
}

} // namespace

GatherLayout planGather(const GatherRequest &request)
{
    if (!finite(request.workArea) || request.workArea.isEmpty()) {
        return refuse(QStringLiteral("work area must be a finite, non-empty rectangle"));
    }
    if (!finite(request.margin) || request.margin < 0 || !finite(request.gap)
        || request.gap < 0) {
        return refuse(QStringLiteral("margin and gap must be finite and non-negative"));
    }
    if (!finite(request.iconExtent) || !(request.iconExtent > 0)) {
        return refuse(QStringLiteral("icon extent must be finite and positive"));
    }
    if (!finite(request.cardSize) || !(request.cardSize.width() > 0)
        || !(request.cardSize.height() > 0) || !finite(request.cellSize)
        || !(request.cellSize.width() > 0) || !(request.cellSize.height() > 0)) {
        return refuse(QStringLiteral("card and cell sizes must be finite and positive"));
    }
    if (!finite(request.scrollOffset)) {
        return refuse(QStringLiteral("scroll offset must be finite"));
    }

    GatherLayout layout;
    layout.field = request.workArea.adjusted(request.margin, request.margin,
                                             -request.margin, -request.margin);
    if (layout.field.isEmpty()) {
        return refuse(QStringLiteral("margin leaves no room inside the work area"));
    }
    layout.ok = true;

    const QSizeF iconSize(request.iconExtent, request.iconExtent);
    const Lane iconLane = stackLane(request.iconifiedWindowIds, iconSize,
                                    layout.field.left(), layout.field, request.gap);
    layout.icons = iconLane.placements;

    const qreal cardLeft = layout.field.left() + iconLane.width
        + (iconLane.width > 0 ? request.gap : qreal(0));
    const Lane cardLane = stackLane(request.containerIds, request.cardSize, cardLeft,
                                    layout.field, request.gap);
    layout.cards = cardLane.placements;

    const qreal gridLeft = cardLeft + cardLane.width
        + (cardLane.width > 0 ? request.gap : qreal(0));
    const qreal gridWidth = layout.field.right() - gridLeft;

    // AGENT-GUARD: a viewport narrower than one cell yields zero columns and
    // no visible window, reported honestly, rather than one column of clipped
    // tiles overlapping the card lane.
    if (gridWidth + 0.5 < request.cellSize.width()) {
        layout.gridViewport = QRectF();
        layout.windows.reserve(request.windows.size());
        for (const GatherTile &tile : request.windows) {
            layout.windows.append(GatherPlacement{tile.id, QRectF(), false});
        }
        layout.diagnostic =
            QStringLiteral("the icon and card lanes leave less than one cell of width");
        return layout;
    }

    layout.gridViewport = QRectF(gridLeft, layout.field.top(), gridWidth,
                                 layout.field.height());
    layout.gridColumns = static_cast<int>(std::max<qsizetype>(
        1, static_cast<qsizetype>(std::floor((gridWidth + request.gap)
                                             / (request.cellSize.width() + request.gap)))));

    const auto columns = static_cast<qsizetype>(layout.gridColumns);
    const qsizetype rows = request.windows.isEmpty()
        ? 0
        : (request.windows.size() + columns - 1) / columns;
    layout.gridRows = static_cast<int>(rows);
    const qreal rowPitch = request.cellSize.height() + request.gap;
    layout.gridContentHeight = rows > 0
        ? static_cast<qreal>(rows) * rowPitch - request.gap
        : qreal(0);
    layout.maximumScrollOffset =
        std::max(qreal(0), layout.gridContentHeight - layout.gridViewport.height());
    layout.appliedScrollOffset =
        std::clamp(request.scrollOffset, qreal(0), layout.maximumScrollOffset);

    layout.windows.reserve(request.windows.size());
    for (qsizetype index = 0; index < request.windows.size(); ++index) {
        const GatherTile &tile = request.windows.at(index);
        const qsizetype column = index % columns;
        const qsizetype row = index / columns;
        const QRectF cell(
            QPointF(layout.gridViewport.left()
                        + static_cast<qreal>(column)
                            * (request.cellSize.width() + request.gap),
                    layout.gridViewport.top() + static_cast<qreal>(row) * rowPitch
                        - layout.appliedScrollOffset),
            request.cellSize);
        layout.windows.append(GatherPlacement{
            tile.id, fitInside(tile.sourceSize, cell),
            cell.intersects(layout.gridViewport)});
    }
    return layout;
}

} // namespace QindaQt::HybridGather

namespace QindaQt::HybridGather {
namespace {

[[nodiscard]] qsizetype laneCapacity(qreal fieldExtent, qreal itemExtent, qreal gap)
{
    return std::max<qsizetype>(1, static_cast<qsizetype>(
        std::floor((fieldExtent + gap) / (itemExtent + gap))));
}

[[nodiscard]] Lane stackFixedLane(const QVector<QString> &ids,
                                  const QSizeF &size,
                                  const QRectF &field,
                                  qreal left,
                                  qreal maxRight,
                                  qreal gap)
{
    Lane lane;
    if (ids.isEmpty()) {
        return lane;
    }
    const qsizetype rows = laneCapacity(field.height(), size.height(), gap);
    const qsizetype columns = std::max<qsizetype>(
        0, static_cast<qsizetype>(std::floor(
               (maxRight - left + gap) / (size.width() + gap))));
    const qsizetype capacity = rows * columns;
    const qsizetype count = std::min(ids.size(), capacity);
    lane.placements.reserve(count);
    for (qsizetype index = 0; index < count; ++index) {
        const qsizetype column = index / rows;
        const qsizetype row = index % rows;
        const qreal x = left + static_cast<qreal>(column) * (size.width() + gap);
        const qreal y = field.top() + static_cast<qreal>(row) * (size.height() + gap);
        lane.placements.append({ids.at(index), QRectF(QPointF(x, y), size), true});
    }
    const qsizetype usedColumns = count == 0 ? 0 : (count + rows - 1) / rows;
    lane.width = usedColumns == 0 ? 0
        : static_cast<qreal>(usedColumns) * size.width()
            + static_cast<qreal>(usedColumns - 1) * gap;
    return lane;
}

[[nodiscard]] bool validMinimizedRequest(const MinimizedGatherRequest &request)
{
    if (!finite(request.workArea) || request.workArea.isEmpty()
        || !finite(request.margin) || request.margin < 0
        || !finite(request.gap) || request.gap < 0
        || !finite(request.iconExtent) || request.iconExtent <= 0) {
        return false;
    }
    return std::all_of(request.shadedContainers.cbegin(),
                       request.shadedContainers.cend(), [](const GatherTile &tile) {
        return finite(tile.sourceSize) && tile.sourceSize.width() > 0
            && tile.sourceSize.height() > 0;
    });
}

} // namespace

MinimizedGatherLayout planMinimizedGather(const MinimizedGatherRequest &request,
                                          const int requestedPage)
{
    MinimizedGatherLayout result;
    if (!validMinimizedRequest(request)) {
        result.diagnostic = QStringLiteral("minimized gather geometry is invalid");
        return result;
    }
    result.field = request.workArea.adjusted(request.margin, request.margin,
                                              -request.margin, -request.margin);
    if (result.field.isEmpty()) {
        result.diagnostic = QStringLiteral("margin leaves no minimized gather field");
        return result;
    }

    qreal containerWidth = 0;
    qreal containerHeight = 0;
    for (const GatherTile &tile : request.shadedContainers) {
        containerWidth = std::max(containerWidth, tile.sourceSize.width());
        containerHeight = std::max(containerHeight, tile.sourceSize.height());
    }
    const QSizeF iconSize(request.iconExtent, request.iconExtent);
    const QSizeF containerSize(containerWidth, containerHeight);
    if ((request.iconifiedWindowIds.size() > 0
         && (iconSize.width() > result.field.width()
             || iconSize.height() > result.field.height()))
        || (request.shadedContainers.size() > 0
            && (containerWidth > result.field.width()
                || containerHeight > result.field.height()))) {
        result.diagnostic = QStringLiteral("one minimized item cannot fit in the work area");
        return result;
    }
    const bool haveIcons = !request.iconifiedWindowIds.isEmpty();
    const bool haveContainers = !request.shadedContainers.isEmpty();
    const qreal laneExtent = result.field.width();
    const qreal reserve = haveIcons && haveContainers
        ? containerWidth + request.gap : 0;
    const qreal iconWidth = std::max(qreal(0), laneExtent - reserve);
    const qsizetype iconRows = laneCapacity(result.field.height(),
                                             iconSize.height(), request.gap);
    const qsizetype iconColumns = haveIcons
        ? static_cast<qsizetype>(std::floor(
              (iconWidth + request.gap) / (iconSize.width() + request.gap))) : 0;
    const qsizetype iconCapacity = iconRows * iconColumns;

    qreal containerLeft = result.field.left();
    qreal containerAvailableWidth = laneExtent;
    if (haveIcons && haveContainers && iconColumns > 0) {
        const qsizetype columns = std::min(
            iconColumns,
            static_cast<qsizetype>(std::floor(
                (iconWidth + request.gap) / (iconSize.width() + request.gap))));
        const qreal usedIconLane = columns > 0
            ? static_cast<qreal>(columns) * iconSize.width()
                + static_cast<qreal>(columns - 1) * request.gap : 0;
        containerLeft += usedIconLane + request.gap;
        containerAvailableWidth -= usedIconLane + request.gap;
    }
    const qsizetype containerRows = haveContainers
        ? laneCapacity(result.field.height(), containerSize.height(), request.gap) : 0;
    const qsizetype containerColumns = haveContainers
        ? static_cast<qsizetype>(std::floor(
              (containerAvailableWidth + request.gap)
                  / (containerSize.width() + request.gap))) : 0;
    const qsizetype containerCapacity = containerRows * containerColumns;

    // On a narrow output, show each kind on its own page instead of letting
    // one lane overlap or pushing the other beyond the reserved work area.
    const bool separateLanes = haveIcons && haveContainers
        && (iconCapacity == 0 || containerCapacity == 0);
    qsizetype iconPageCapacity = iconCapacity;
    qsizetype containerPageCapacity = containerCapacity;
    qsizetype iconPages = 0;
    qsizetype containerPages = 0;
    if (separateLanes) {
        iconPageCapacity = iconRows * laneCapacity(
            result.field.width(), iconSize.width(), request.gap);
        containerPageCapacity = containerRows * laneCapacity(
            result.field.width(), containerSize.width(), request.gap);
        if (iconPageCapacity == 0 || containerPageCapacity == 0) {
            result.diagnostic = QStringLiteral("one minimized item cannot fit in the work area");
            return result;
        }
        iconPages = (request.iconifiedWindowIds.size() + iconPageCapacity - 1)
            / iconPageCapacity;
        containerPages = (request.shadedContainers.size() + containerPageCapacity - 1)
            / containerPageCapacity;
    } else {
        if ((haveIcons && iconCapacity == 0)
            || (haveContainers && containerCapacity == 0)) {
            result.diagnostic = QStringLiteral("one minimized item cannot fit beside its lane");
            return result;
        }
        iconPages = haveIcons
            ? (request.iconifiedWindowIds.size() + iconPageCapacity - 1) / iconPageCapacity
            : 0;
        containerPages = haveContainers
            ? (request.shadedContainers.size() + containerPageCapacity - 1)
                / containerPageCapacity
            : 0;
    }
    const qsizetype pageCount = separateLanes
        ? iconPages + containerPages : std::max(iconPages, containerPages);
    result.pageCount = static_cast<int>(std::max<qsizetype>(1, pageCount));
    result.appliedPage = std::clamp(requestedPage, 0, result.pageCount - 1);
    result.ok = true;

    if (separateLanes) {
        if (result.appliedPage < iconPages) {
            const qsizetype first = result.appliedPage * iconPageCapacity;
            const auto ids = request.iconifiedWindowIds.mid(
                first, std::min(iconPageCapacity,
                                request.iconifiedWindowIds.size() - first));
            const Lane lane = stackFixedLane(ids, iconSize, result.field,
                                              result.field.left(), result.field.right(),
                                              request.gap);
            result.icons = lane.placements;
        } else {
            const qsizetype page = result.appliedPage - iconPages;
            const qsizetype first = page * containerPageCapacity;
            const qsizetype count = std::min(containerPageCapacity,
                request.shadedContainers.size() - first);
            QVector<QString> ids;
            ids.reserve(count);
            for (qsizetype index = 0; index < count; ++index) {
                ids.append(request.shadedContainers.at(first + index).id);
            }
            const Lane lane = stackFixedLane(ids, containerSize, result.field,
                                              result.field.left(), result.field.right(),
                                              request.gap);
            result.containers = lane.placements;
        }
        return result;
    }

    if (haveIcons) {
        const qsizetype first = result.appliedPage * iconPageCapacity;
        const auto ids = request.iconifiedWindowIds.mid(
            first, std::min(iconPageCapacity,
                            request.iconifiedWindowIds.size() - first));
        const Lane lane = stackFixedLane(ids, iconSize, result.field,
                                          result.field.left(),
                                          result.field.left() + iconWidth,
                                          request.gap);
        result.icons = lane.placements;
    }
    if (haveContainers) {
        const qsizetype first = result.appliedPage * containerPageCapacity;
        const qsizetype count = std::min(containerPageCapacity,
            request.shadedContainers.size() - first);
        QVector<QString> ids;
        ids.reserve(count);
        for (qsizetype index = 0; index < count; ++index) {
            ids.append(request.shadedContainers.at(first + index).id);
        }
        const Lane lane = stackFixedLane(ids, containerSize, result.field,
                                          containerLeft,
                                          result.field.right(), request.gap);
        result.containers = lane.placements;
    }
    return result;
}

} // namespace QindaQt::HybridGather
