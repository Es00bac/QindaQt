// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QHash>
#include <QRect>
#include <QSize>
#include <QString>

namespace QindaQt::HybridConstraints {

struct MemberPlacement final
{
    // tileFrame participates in the gap-free split partition. windowFrame is
    // centered within it after applying the client's maximum or fixed size.
    QRect tileFrame;
    QRect windowFrame;
    bool minimumWidthSatisfied = true;
    bool minimumHeightSatisfied = true;

    [[nodiscard]] bool minimumSizeSatisfied() const noexcept
    {
        return minimumWidthSatisfied && minimumHeightSatisfied;
    }

    friend bool operator==(const MemberPlacement &, const MemberPlacement &) = default;
};

struct SplitPlacement final
{
    QRect frame;
    QRect firstTileFrame;
    QRect dividerFrame;
    QRect secondTileFrame;
    // Both ratios describe the first child's share of distributable space;
    // divider pixels are excluded from the denominator.
    double preferredRatio = 0.5;
    double effectiveRatio = 0.5;
    bool primaryMinimumsSatisfied = true;

    friend bool operator==(const SplitPlacement &, const SplitPlacement &) = default;
};

struct OverflowReport final
{
    // Required outer size includes shared chrome and every recursive divider.
    // Maximum-size slack is not overflow because actual windows remain bounded.
    QSize availableOuterSize;
    QSize requiredOuterSize;
    QSize missingSize;

    [[nodiscard]] bool hasOverflow() const noexcept
    {
        return missingSize.width() > 0 || missingSize.height() > 0;
    }

    friend bool operator==(const OverflowReport &, const OverflowReport &) = default;
};

struct ConstraintSolution final
{
    QRect outerFrame;
    QRect contentFrame;
    QSize requiredContentSize;
    OverflowReport overflow;
    QHash<QString, MemberPlacement> members;
    QHash<QString, SplitPlacement> splits;

    [[nodiscard]] bool hasOverflow() const noexcept { return overflow.hasOverflow(); }

    friend bool operator==(const ConstraintSolution &, const ConstraintSolution &) = default;
};

} // namespace QindaQt::HybridConstraints
