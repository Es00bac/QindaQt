// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_constraints/constraint_solver.h"

#include <QSet>
#include <QtTypes>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QindaQt::HybridConstraints {
namespace {

struct ExtentRequirements final
{
    int minimum = 0;
    std::optional<int> maximum;
};

struct SubtreeRequirements final
{
    ExtentRequirements width;
    ExtentRequirements height;
};

struct AnalysisContext final
{
    const QHash<QString, MemberSizeConstraints> &inputConstraints;
    QHash<QString, MemberSizeConstraints> usedConstraints;
    QHash<QString, SubtreeRequirements> requirements;
    QSet<QString> nodeIds;
    QSet<QString> windowIds;
    int dividerThickness = 0;
};

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

int saturatedAdd(int first, int second, int third = 0) noexcept
{
    const qint64 sum = qint64(first) + qint64(second) + qint64(third);
    return int(std::min(sum, qint64(std::numeric_limits<int>::max())));
}

std::optional<int> addMaximums(const std::optional<int> &first,
                               const std::optional<int> &second,
                               int dividerThickness) noexcept
{
    if (!first.has_value() || !second.has_value()) {
        return std::nullopt;
    }
    return saturatedAdd(*first, *second, dividerThickness);
}

std::optional<int> widestMaximum(const std::optional<int> &first,
                                 const std::optional<int> &second) noexcept
{
    if (!first.has_value() || !second.has_value()) {
        return std::nullopt;
    }
    return std::max(*first, *second);
}

std::optional<SubtreeRequirements> analyzeNode(const Core::LayoutNode &node,
                                               AnalysisContext &context,
                                               QString *error)
{
    if (node.id().isEmpty() || context.nodeIds.contains(node.id())) {
        setError(error, QStringLiteral("layout nodes require unique non-empty IDs"));
        return std::nullopt;
    }
    context.nodeIds.insert(node.id());

    if (node.isLeaf()) {
        if (node.windowId().isEmpty() || context.windowIds.contains(node.windowId())) {
            setError(error, QStringLiteral("layout leaves require unique non-empty window IDs"));
            return std::nullopt;
        }
        context.windowIds.insert(node.windowId());

        const auto constraints = context.inputConstraints.value(node.windowId());
        QString constraintError;
        if (!constraints.isValid(&constraintError)) {
            setError(error, node.windowId() + QStringLiteral(": ") + constraintError);
            return std::nullopt;
        }
        context.usedConstraints.insert(node.windowId(), constraints);

        const auto minimum = constraints.effectiveMinimumSize();
        const auto maximum = constraints.effectiveMaximumSize();
        SubtreeRequirements result{
            .width = {minimum.width(),
                      maximum.has_value()
                          ? std::optional<int>(maximum->width())
                          : std::nullopt},
            .height = {minimum.height(),
                       maximum.has_value()
                           ? std::optional<int>(maximum->height())
                           : std::nullopt},
        };
        context.requirements.insert(node.id(), result);
        return result;
    }

    const auto orientation = node.orientation();
    const auto ratio = node.ratio();
    if (!orientation.has_value() || !ratio.has_value() || !std::isfinite(*ratio)
        || *ratio <= 0.0 || *ratio >= 1.0 || node.firstChild() == nullptr
        || node.secondChild() == nullptr) {
        setError(error,
                 QStringLiteral(
                     "split nodes require two children and a finite ratio in (0, 1)"));
        return std::nullopt;
    }

    const auto first = analyzeNode(*node.firstChild(), context, error);
    if (!first.has_value()) {
        return std::nullopt;
    }
    const auto second = analyzeNode(*node.secondChild(), context, error);
    if (!second.has_value()) {
        return std::nullopt;
    }

    SubtreeRequirements result;
    if (*orientation == Core::SplitOrientation::Horizontal) {
        result.width.minimum = saturatedAdd(first->width.minimum,
                                            second->width.minimum,
                                            context.dividerThickness);
        result.width.maximum = addMaximums(first->width.maximum,
                                           second->width.maximum,
                                           context.dividerThickness);
        result.height.minimum = std::max(first->height.minimum, second->height.minimum);
        result.height.maximum = widestMaximum(first->height.maximum, second->height.maximum);
    } else {
        result.width.minimum = std::max(first->width.minimum, second->width.minimum);
        result.width.maximum = widestMaximum(first->width.maximum, second->width.maximum);
        result.height.minimum = saturatedAdd(first->height.minimum,
                                             second->height.minimum,
                                             context.dividerThickness);
        result.height.maximum = addMaximums(first->height.maximum,
                                            second->height.maximum,
                                            context.dividerThickness);
    }
    context.requirements.insert(node.id(), result);
    return result;
}

int roundedShare(int available, double ratio) noexcept
{
    // AGENT-CONTRACT: Logical pixels are integral at this boundary. Exact .5
    // ties go to the first child so identical inputs are stable across libc and
    // compositor backends; the second child always receives the remainder.
    const long double product = static_cast<long double>(available)
        * static_cast<long double>(ratio);
    const auto rounded = static_cast<qint64>(std::floor(product + 0.5L));
    return int(std::clamp(rounded, qint64(0), qint64(available)));
}

int chooseFirstExtent(int available,
                      const ExtentRequirements &first,
                      const ExtentRequirements &second,
                      double preferredRatio) noexcept
{
    const qint64 required = qint64(first.minimum) + qint64(second.minimum);
    if (required > available) {
        if (required == 0) {
            return roundedShare(available, preferredRatio);
        }
        const long double proportional = static_cast<long double>(available)
            * static_cast<long double>(first.minimum)
            / static_cast<long double>(required);
        return int(std::floor(proportional + 0.5L));
    }

    const int minimumLower = first.minimum;
    const int minimumUpper = available - second.minimum;
    const int preferred = std::clamp(roundedShare(available, preferredRatio),
                                     minimumLower,
                                     minimumUpper);

    int maximumLower = minimumLower;
    if (second.maximum.has_value()) {
        maximumLower = std::max(maximumLower, available - *second.maximum);
    }
    int maximumUpper = minimumUpper;
    if (first.maximum.has_value()) {
        maximumUpper = std::min(maximumUpper, *first.maximum);
    }

    if (maximumLower <= maximumUpper) {
        return std::clamp(preferred, maximumLower, maximumUpper);
    }

    // Both subtrees have finite maxima and cannot consume the full span. Tile
    // slack is unavoidable; retain the nearest preferred boundary while leaf
    // placement below keeps every actual window within its declared maximum.
    const int slackLower = std::clamp(maximumUpper, minimumLower, minimumUpper);
    const int slackUpper = std::clamp(maximumLower, minimumLower, minimumUpper);
    return std::clamp(preferred,
                      std::min(slackLower, slackUpper),
                      std::max(slackLower, slackUpper));
}

QRect centeredWindowFrame(const QRect &tile,
                          const MemberSizeConstraints &constraints,
                          bool *minimumWidthSatisfied,
                          bool *minimumHeightSatisfied)
{
    const QSize minimum = constraints.effectiveMinimumSize();
    const auto maximum = constraints.effectiveMaximumSize();

    *minimumWidthSatisfied = tile.width() >= minimum.width();
    *minimumHeightSatisfied = tile.height() >= minimum.height();

    int width = tile.width();
    int height = tile.height();
    if (maximum.has_value()) {
        width = std::min(width, maximum->width());
        height = std::min(height, maximum->height());
    }

    // Odd slack leaves the trailing/right edge one pixel larger. This fixed
    // bias avoids geometry oscillation when a container is repeatedly solved.
    return QRect(tile.x() + ((tile.width() - width) / 2),
                 tile.y() + ((tile.height() - height) / 2),
                 width,
                 height);
}

void placeNode(const Core::LayoutNode &node,
               const QRect &frame,
               const AnalysisContext &analysis,
               int dividerThickness,
               ConstraintSolution &solution)
{
    if (node.isLeaf()) {
        MemberPlacement placement;
        placement.tileFrame = frame;
        placement.windowFrame = centeredWindowFrame(
            frame,
            analysis.usedConstraints.value(node.windowId()),
            &placement.minimumWidthSatisfied,
            &placement.minimumHeightSatisfied);
        solution.members.insert(node.windowId(), placement);
        return;
    }

    const auto orientation = *node.orientation();
    const auto preferredRatio = *node.ratio();
    const bool horizontal = orientation == Core::SplitOrientation::Horizontal;
    const int primaryExtent = horizontal ? frame.width() : frame.height();
    const int actualDivider = std::min(dividerThickness, primaryExtent);
    const int distributable = primaryExtent - actualDivider;

    const auto &firstRequirements = analysis.requirements.value(node.firstChild()->id());
    const auto &secondRequirements = analysis.requirements.value(node.secondChild()->id());
    const auto &firstPrimary = horizontal ? firstRequirements.width
                                          : firstRequirements.height;
    const auto &secondPrimary = horizontal ? secondRequirements.width
                                           : secondRequirements.height;
    const int firstExtent = chooseFirstExtent(distributable,
                                              firstPrimary,
                                              secondPrimary,
                                              preferredRatio);
    const int secondExtent = distributable - firstExtent;

    QRect firstFrame;
    QRect dividerFrame;
    QRect secondFrame;
    if (horizontal) {
        firstFrame = QRect(frame.x(), frame.y(), firstExtent, frame.height());
        dividerFrame = QRect(frame.x() + firstExtent,
                             frame.y(),
                             actualDivider,
                             frame.height());
        secondFrame = QRect(frame.x() + firstExtent + actualDivider,
                            frame.y(),
                            secondExtent,
                            frame.height());
    } else {
        firstFrame = QRect(frame.x(), frame.y(), frame.width(), firstExtent);
        dividerFrame = QRect(frame.x(),
                             frame.y() + firstExtent,
                             frame.width(),
                             actualDivider);
        secondFrame = QRect(frame.x(),
                            frame.y() + firstExtent + actualDivider,
                            frame.width(),
                            secondExtent);
    }

    const qint64 requiredPrimary = qint64(firstPrimary.minimum)
        + qint64(secondPrimary.minimum);
    const SplitPlacement placement{
        .frame = frame,
        .firstTileFrame = firstFrame,
        .dividerFrame = dividerFrame,
        .secondTileFrame = secondFrame,
        .preferredRatio = preferredRatio,
        .effectiveRatio = distributable > 0
            ? double(firstExtent) / double(distributable)
            : preferredRatio,
        .primaryMinimumsSatisfied = requiredPrimary <= distributable,
    };
    solution.splits.insert(node.id(), placement);

    placeNode(*node.firstChild(), firstFrame, analysis, dividerThickness, solution);
    placeNode(*node.secondChild(), secondFrame, analysis, dividerThickness, solution);
}

std::optional<QRect> contentFrameFor(const QRect &outer,
                                     const QMargins &insets,
                                     QString *error)
{
    const qint64 contentX = qint64(outer.x()) + insets.left();
    const qint64 contentY = qint64(outer.y()) + insets.top();
    const qint64 horizontalInsets = qint64(insets.left()) + insets.right();
    const qint64 verticalInsets = qint64(insets.top()) + insets.bottom();
    const int contentWidth = int(std::max(qint64(0), qint64(outer.width()) - horizontalInsets));
    const int contentHeight = int(std::max(qint64(0), qint64(outer.height()) - verticalInsets));
    if (contentX < std::numeric_limits<int>::min()
        || contentX > std::numeric_limits<int>::max()
        || contentY < std::numeric_limits<int>::min()
        || contentY > std::numeric_limits<int>::max()) {
        setError(error, QStringLiteral("content inset coordinates exceed QRect limits"));
        return std::nullopt;
    }
    return QRect(int(contentX), int(contentY), contentWidth, contentHeight);
}

} // namespace

std::optional<ConstraintSolution> ConstraintSolver::solve(
    const Core::LayoutNode &root,
    const QRect &outerFrame,
    const QHash<QString, MemberSizeConstraints> &memberConstraints,
    const LayoutMetrics &metrics,
    QString *error)
{
    if (outerFrame.width() < 0 || outerFrame.height() < 0) {
        setError(error, QStringLiteral("outer frame must have a non-negative size"));
        return std::nullopt;
    }
    if (qint64(outerFrame.x()) + outerFrame.width() > std::numeric_limits<int>::max()
        || qint64(outerFrame.y()) + outerFrame.height() > std::numeric_limits<int>::max()) {
        setError(error, QStringLiteral("outer frame exceeds QRect coordinate limits"));
        return std::nullopt;
    }
    if (!metrics.isValid(error)) {
        return std::nullopt;
    }

    AnalysisContext analysis{
        .inputConstraints = memberConstraints,
        .usedConstraints = {},
        .requirements = {},
        .nodeIds = {},
        .windowIds = {},
        .dividerThickness = metrics.dividerThickness,
    };
    const auto rootRequirements = analyzeNode(root, analysis, error);
    if (!rootRequirements.has_value()) {
        return std::nullopt;
    }
    const auto contentFrame = contentFrameFor(outerFrame, metrics.contentInsets, error);
    if (!contentFrame.has_value()) {
        return std::nullopt;
    }

    const QSize requiredContent(rootRequirements->width.minimum,
                                rootRequirements->height.minimum);
    const QSize requiredOuter(
        saturatedAdd(requiredContent.width(),
                     metrics.contentInsets.left(),
                     metrics.contentInsets.right()),
        saturatedAdd(requiredContent.height(),
                     metrics.contentInsets.top(),
                     metrics.contentInsets.bottom()));
    const QSize missing(std::max(0, requiredOuter.width() - outerFrame.width()),
                        std::max(0, requiredOuter.height() - outerFrame.height()));

    ConstraintSolution solution{
        .outerFrame = outerFrame,
        .contentFrame = *contentFrame,
        .requiredContentSize = requiredContent,
        .overflow = {.availableOuterSize = outerFrame.size(),
                     .requiredOuterSize = requiredOuter,
                     .missingSize = missing},
        .members = {},
        .splits = {},
    };
    // AGENT-GUARD: Analyze the complete tree before emitting any placement.
    // Callers publish one atomic scene update and cannot safely consume a
    // partial layout after discovering an invalid descendant.
    placeNode(root, *contentFrame, analysis, metrics.dividerThickness, solution);
    return solution;
}

} // namespace QindaQt::HybridConstraints
