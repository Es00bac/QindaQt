// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeplanbuilder.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QRectF>
#include <QStringList>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using HybridChrome::ChromeDividerSpec;
using HybridChrome::ChromeLayoutRequest;
using HybridChrome::ChromeMemberSpec;
using HybridChrome::ChromeTabSpec;
using HybridChrome::DividerOrientation;

std::optional<HybridChrome::ChromeRenderPlan> reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return std::nullopt;
}

QStringList leafWindowIds(const Core::LayoutNode &node)
{
    if (node.isLeaf()) {
        return {node.windowId()};
    }
    auto result = leafWindowIds(*node.firstChild());
    result.append(leafWindowIds(*node.secondChild()));
    return result;
}

QString resolvedTitle(const QString &windowId, const HybridWindowTitleLookup &lookup)
{
    const auto title = lookup ? lookup(windowId).trimmed() : QString{};
    return title.isEmpty() ? windowId : title;
}

QString pageTitle(const Core::ContainerPage &page,
                  const HybridWindowTitleLookup &titleLookup)
{
    const auto ids = leafWindowIds(page.root());
    const auto firstTitle = resolvedTitle(ids.constFirst(), titleLookup);
    if (ids.size() == 1) {
        return firstTitle;
    }
    return QStringLiteral("%1 +%2").arg(firstTitle).arg(ids.size() - 1);
}

bool appendNodeGeometry(
    const Core::LayoutNode &node,
    const HybridConstraints::ConstraintSolution &solution,
    const HybridWindowTitleLookup &titleLookup,
    QVector<ChromeMemberSpec> *members,
    QVector<ChromeDividerSpec> *dividers,
    QString *error)
{
    if (node.isLeaf()) {
        const auto placement = solution.members.constFind(node.windowId());
        if (placement == solution.members.cend()) {
            if (error) {
                *error = QStringLiteral("active member '%1' has no committed placement")
                             .arg(node.windowId());
            }
            return false;
        }
        members->append({node.windowId(),
                         resolvedTitle(node.windowId(), titleLookup),
                         QRectF(placement->windowFrame)});
        return true;
    }

    const auto placement = solution.splits.constFind(node.id());
    if (placement == solution.splits.cend()) {
        if (error) {
            *error = QStringLiteral("active split '%1' has no committed placement")
                         .arg(node.id());
        }
        return false;
    }
    const QRectF dividerFrame(placement->dividerFrame);
    if (!dividerFrame.isValid()) {
        if (error) {
            *error = QStringLiteral("active split '%1' has no visible divider")
                         .arg(node.id());
        }
        return false;
    }

    const bool horizontal = *node.orientation() == Core::SplitOrientation::Horizontal;
    dividers->append(
        {.dividerId = node.id(),
         .orientation = horizontal ? DividerOrientation::Vertical
                                   : DividerOrientation::Horizontal,
         .position = horizontal ? dividerFrame.center().x() : dividerFrame.center().y(),
         .spanStart = horizontal ? dividerFrame.top() : dividerFrame.left(),
         .spanEnd = horizontal ? dividerFrame.bottom() : dividerFrame.right()});

    // AGENT-GUARD: Tree traversal, not QHash iteration, defines stable paint
    // and hit-test ordering. The constraint maps intentionally have no order.
    return appendNodeGeometry(*node.firstChild(), solution, titleLookup,
                              members, dividers, error)
        && appendNodeGeometry(*node.secondChild(), solution, titleLookup,
                              members, dividers, error);
}

bool sameLogicalRect(const QRectF &first, const QRectF &second)
{
    constexpr qreal tolerance = 0.01;
    return qAbs(first.x() - second.x()) <= tolerance
        && qAbs(first.y() - second.y()) <= tolerance
        && qAbs(first.width() - second.width()) <= tolerance
        && qAbs(first.height() - second.height()) <= tolerance;
}

QRectF expectedContentRect(const ChromeLayoutRequest &request)
{
    const auto inner = request.outerRect.adjusted(
        request.metrics.outerBorder, request.metrics.outerBorder,
        -request.metrics.outerBorder, -request.metrics.outerBorder);
    const qreal chromeHeight = request.metrics.titleBarHeight
        + (request.tabs.isEmpty() ? 0.0 : request.metrics.tabStripHeight);
    return {inner.left(), inner.top() + chromeHeight,
            inner.width(), inner.height() - chromeHeight};
}

} // namespace

std::optional<HybridChrome::ChromeRenderPlan> HybridChromePlanBuilder::build(
    const Core::WindowContainer &container,
    const HybridConstraints::ConstraintSolution &solution,
    const HybridChromePlanOptions &options,
    const HybridWindowTitleLookup &titleLookup,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const auto state = container.validate();
    if (!state.valid) {
        return reject(error, QStringLiteral("invalid container: %1").arg(state.message));
    }
    const auto *activePage = container.page(container.activePageId());
    if (!activePage) {
        return reject(error, QStringLiteral("container has no active page"));
    }
    if (solution.outerFrame.isEmpty() || solution.outerFrame != solution.outerFrame.normalized()) {
        return reject(error, QStringLiteral("committed outer frame is invalid"));
    }

    ChromeLayoutRequest request{
        .containerId = container.id(),
        .outerRect = QRectF(solution.outerFrame),
        .devicePixelRatio = options.devicePixelRatio,
        .maximized = options.maximized,
        .metrics = options.metrics,
        .style = options.style,
        .tabs = {},
        .members = {},
        .dividers = {},
    };
    for (const auto &page : container.pages()) {
        request.tabs.append({page.id(),
                             pageTitle(page, titleLookup),
                             page.id() == container.activePageId()});
    }
    if (!sameLogicalRect(expectedContentRect(request), QRectF(solution.contentFrame))) {
        return reject(error,
                      QStringLiteral("scene and chrome content frames do not match"));
    }
    QString geometryError;
    if (!appendNodeGeometry(activePage->root(), solution, titleLookup,
                            &request.members, &request.dividers, &geometryError)) {
        return reject(error, std::move(geometryError));
    }
    if (request.members.size() != solution.members.size()
        || request.dividers.size() != solution.splits.size()) {
        return reject(error,
                      QStringLiteral("committed solution contains stale active-page geometry"));
    }

    QString planError;
    auto plan = HybridChrome::ChromeLayoutEngine::build(request, &planError);
    if (!plan) {
        return reject(error, QStringLiteral("chrome layout failed: %1").arg(planError));
    }
    if (!sameLogicalRect(plan->contentRect, QRectF(solution.contentFrame))) {
        return reject(error,
                      QStringLiteral("scene and chrome content frames do not match"));
    }
    return plan;
}

} // namespace QindaQt::Compositor::KWinIntegration
