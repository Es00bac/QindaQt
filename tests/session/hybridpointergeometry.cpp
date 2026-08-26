// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointergeometry.h"

#include "compositordevelopmentworkflow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace QindaQt::Test {
namespace {

bool pointIsClear(const QPointF &point,
                  const WindowInventory &inventory,
                  const QStringList &ignoredTitles)
{
    return std::none_of(inventory.cbegin(), inventory.cend(),
                        [&](const ObservedWindow &window) {
                            return !ignoredTitles.contains(window.title)
                                && window.frame.contains(point);
                        });
}

std::optional<QPointF> titlePoint(const ObservedWindow &source,
                                  const QRectF &output)
{
    constexpr std::array<qreal, 7> horizontalFractions{
        0.50, 0.72, 0.28, 0.86, 0.14, 0.94, 0.06};
    constexpr std::array<qreal, 3> verticalOffsets{10.0, 18.0, 25.0};
    for (const auto offset : verticalOffsets) {
        for (const auto fraction : horizontalFractions) {
            const QPointF candidate(source.frame.left() + source.frame.width() * fraction,
                                    source.frame.top() + offset);
            if (source.frame.contains(candidate) && output.contains(candidate)) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

struct DropCandidate final
{
    QString zone;
    QPointF point;
};

std::optional<DropCandidate> edgePoint(const ObservedWindow &target,
                                       const QString &sourceTitle,
                                       const WindowInventory &inventory,
                                       const QRectF &output)
{
    constexpr std::array<qreal, 5> alongEdge{0.50, 0.68, 0.32, 0.82, 0.18};
    constexpr qreal inset = 0.08;
    for (const auto fraction : alongEdge) {
        const std::array<DropCandidate, 4> candidates{{
            {QStringLiteral("left"),
             {target.frame.left() + target.frame.width() * inset,
              target.frame.top() + target.frame.height() * fraction}},
            {QStringLiteral("right"),
             {target.frame.right() - target.frame.width() * inset,
              target.frame.top() + target.frame.height() * fraction}},
            {QStringLiteral("top"),
             {target.frame.left() + target.frame.width() * fraction,
              target.frame.top() + target.frame.height() * inset}},
            {QStringLiteral("bottom"),
             {target.frame.left() + target.frame.width() * fraction,
              target.frame.bottom() - target.frame.height() * inset}},
        }};
        for (const auto &candidate : candidates) {
            if (target.frame.contains(candidate.point) && output.contains(candidate.point)
                // The resolver deliberately excludes the dragged source, so
                // source overlap is valid. Any third client would win the hit.
                && pointIsClear(candidate.point, inventory,
                                {target.title, sourceTitle})) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<DockGestureGeometry> chooseDockGesture(const WindowInventory &inventory,
                                                     const ProbeWindowTitles &titles,
                                                     const QRectF &output,
                                                     QString *error)
{
    // sessionprobe explicitly raises `page` immediately before this workflow.
    // Trying it first makes the press deterministic even when KWin's virtual
    // placement policy initially stacks all three top-level windows at (0,0).
    for (const auto &pair : {std::pair{titles.page, titles.primary},
                             std::pair{titles.page, titles.secondary},
                             std::pair{titles.primary, titles.secondary},
                             std::pair{titles.secondary, titles.primary}}) {
        const auto source = inventory.constFind(pair.first);
        const auto target = inventory.constFind(pair.second);
        if (source == inventory.cend() || target == inventory.cend()) {
            continue;
        }
        const auto sourcePoint = titlePoint(source.value(), output);
        const auto drop = edgePoint(target.value(), pair.first, inventory, output);
        if (sourcePoint && drop) {
            return DockGestureGeometry{pair.first, pair.second, drop->zone,
                                       *sourcePoint, drop->point};
        }
    }
    *error = QStringLiteral("painted probe geometry has no title-to-edge path");
    return std::nullopt;
}

QString bystanderTitle(const ProbeWindowTitles &titles,
                       const DockGestureGeometry &gesture)
{
    for (const auto &title : {titles.primary, titles.secondary, titles.page}) {
        if (title != gesture.sourceTitle && title != gesture.targetTitle) {
            return title;
        }
    }
    return {};
}

std::optional<QPointF> emptyDesktopPoint(const WindowInventory &inventory,
                                         const QStringList &groupedTitles,
                                         const QRectF &output,
                                         QString *error)
{
    QRectF groupedBounds;
    for (const auto &title : groupedTitles) {
        const auto found = inventory.constFind(title);
        if (found == inventory.cend()) {
            continue;
        }
        groupedBounds = groupedBounds.isValid()
            ? groupedBounds.united(found->targetFrame) : found->targetFrame;
    }
    // Shared chrome begins above the member frames. Keep a generous exclusion
    // around the inferred outer frame so a detach cannot accidentally land on
    // a tab, outer title, resize margin, or the unrelated third probe.
    const auto chromeExclusion = groupedBounds.adjusted(-32.0, -112.0, 32.0, 32.0);
    constexpr std::array<qreal, 8> fractions{0.08, 0.92, 0.50, 0.20,
                                             0.80, 0.35, 0.65, 0.12};
    for (const auto y : fractions) {
        for (const auto x : fractions) {
            const QPointF candidate(output.left() + output.width() * x,
                                    output.top() + output.height() * y);
            if (!chromeExclusion.contains(candidate)
                && pointIsClear(candidate, inventory, {})) {
                return candidate;
            }
        }
    }
    *error = QStringLiteral("single-output desktop has no safe empty detach point");
    return std::nullopt;
}

SplitEvidence splitEvidence(const QRectF &first, const QRectF &second)
{
    constexpr qreal tolerance = 1.0;
    constexpr qreal maximumDividerGap = 12.0;
    const auto near = [](qreal left, qreal right) {
        return std::abs(left - right) <= tolerance;
    };
    const auto gap = [](qreal trailing, qreal leading) {
        return leading - trailing;
    };
    if (first.isValid() && second.isValid()
        && near(first.y(), second.y()) && near(first.height(), second.height())) {
        const qreal forwardGap = gap(first.x() + first.width(), second.x());
        const qreal reverseGap = gap(second.x() + second.width(), first.x());
        const qreal dividerGap = std::max(forwardGap, reverseGap);
        if (dividerGap >= -tolerance && dividerGap <= maximumDividerGap) {
            return {true, QStringLiteral("horizontal"), std::max<qreal>(0.0, dividerGap)};
        }
    }
    if (first.isValid() && second.isValid()
        && near(first.x(), second.x()) && near(first.width(), second.width())) {
        const qreal forwardGap = gap(first.y() + first.height(), second.y());
        const qreal reverseGap = gap(second.y() + second.height(), first.y());
        const qreal dividerGap = std::max(forwardGap, reverseGap);
        if (dividerGap >= -tolerance && dividerGap <= maximumDividerGap) {
            return {true, QStringLiteral("vertical"), std::max<qreal>(0.0, dividerGap)};
        }
    }
    return {};
}

} // namespace QindaQt::Test
