// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace QindaQt::HybridChrome {
namespace {

std::optional<ChromeRenderPlan> reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return std::nullopt;
}

bool finiteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
        && std::isfinite(rect.width()) && std::isfinite(rect.height());
}

bool uniqueId(const QString &id, QSet<QString> *seen, QString *error, QStringView kind)
{
    if (id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("%1 ID must not be empty").arg(kind);
        }
        return false;
    }
    if (seen->contains(id)) {
        if (error) {
            *error = QStringLiteral("duplicate %1 ID '%2'").arg(kind, id);
        }
        return false;
    }
    seen->insert(id);
    return true;
}

QVector<WindowAction> actionOrder(const ChromeStyle &style, bool maximized)
{
    const auto sizeAction = maximized ? WindowAction::Restore : WindowAction::Maximize;
    if (style.buttonStyle == ButtonStyle::Symbols && style.buttonSide == ButtonSide::Right) {
        return {WindowAction::Minimize, sizeAction, WindowAction::Close};
    }
    return {WindowAction::Close, WindowAction::Minimize, sizeAction};
}

QString glyph(WindowAction action)
{
    switch (action) {
    case WindowAction::Close:
        return QStringLiteral("x");
    case WindowAction::Minimize:
        return QStringLiteral("_");
    case WindowAction::Maximize:
    case WindowAction::Restore:
        return QStringLiteral("[]");
    }
    Q_UNREACHABLE_RETURN({});
}

QColor buttonColor(const ChromeStyle &style, WindowAction action)
{
    if (style.buttonStyle == ButtonStyle::Symbols) {
        return style.palette.surfaceRaised;
    }
    switch (action) {
    case WindowAction::Close:
        return style.palette.close;
    case WindowAction::Minimize:
        return style.palette.minimize;
    case WindowAction::Maximize:
    case WindowAction::Restore:
        return style.palette.maximize;
    }
    Q_UNREACHABLE_RETURN(style.palette.surfaceRaised);
}

bool validateRequest(const ChromeLayoutRequest &request, QString *error)
{
    if (request.containerId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("container ID must not be empty");
        }
        return false;
    }
    if (!finiteRect(request.outerRect) || !request.outerRect.isValid()) {
        if (error) {
            *error = QStringLiteral("outer frame must be a finite positive rectangle");
        }
        return false;
    }
    if (!std::isfinite(request.devicePixelRatio) || request.devicePixelRatio < 0.5
        || request.devicePixelRatio > 8.0) {
        if (error) {
            *error = QStringLiteral("device pixel ratio must be within 0.5 through 8.0");
        }
        return false;
    }
    if (!request.metrics.isValid(error) || !request.style.palette.isValid(error)) {
        return false;
    }
    QSet<QString> tabIds;
    qsizetype activeTabs = 0;
    for (const auto &tab : request.tabs) {
        if (!uniqueId(tab.tabId, &tabIds, error, QStringLiteral("tab"))) {
            return false;
        }
        activeTabs += tab.active ? 1 : 0;
    }
    if (!request.tabs.isEmpty() && activeTabs != 1) {
        if (error) {
            *error = QStringLiteral("exactly one tab must be active");
        }
        return false;
    }
    QSet<QString> memberIds;
    for (const auto &member : request.members) {
        if (!uniqueId(member.memberId, &memberIds, error, QStringLiteral("member"))
            || !finiteRect(member.windowRect) || !member.windowRect.isValid()) {
            if (error && error->isEmpty()) {
                *error = QStringLiteral("member '%1' has invalid window geometry")
                             .arg(member.memberId);
            }
            return false;
        }
    }
    QSet<QString> dividerIds;
    for (const auto &divider : request.dividers) {
        if (!uniqueId(divider.dividerId, &dividerIds, error, QStringLiteral("divider"))
            || !std::isfinite(divider.position) || !std::isfinite(divider.spanStart)
            || !std::isfinite(divider.spanEnd) || divider.spanEnd <= divider.spanStart) {
            if (error && error->isEmpty()) {
                *error = QStringLiteral("divider '%1' has invalid geometry")
                             .arg(divider.dividerId);
            }
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<ChromeRenderPlan> ChromeLayoutEngine::build(const ChromeLayoutRequest &request,
                                                           QString *error)
{
    if (error) {
        error->clear();
    }
    if (!validateRequest(request, error)) {
        return std::nullopt;
    }

    ChromeRenderPlan plan;
    plan.containerId = request.containerId;
    plan.devicePixelRatio = request.devicePixelRatio;
    plan.borderHairline = request.metrics.physicalHairline(request.devicePixelRatio);
    plan.maximized = request.maximized;
    plan.metrics = request.metrics;
    plan.style = request.style;
    plan.outerFrame = request.outerRect;

    const auto &metrics = request.metrics;
    const auto inner = request.outerRect.adjusted(metrics.outerBorder, metrics.outerBorder,
                                                  -metrics.outerBorder, -metrics.outerBorder);
    const qreal tabHeight = request.tabs.isEmpty() ? 0.0 : metrics.tabStripHeight;
    if (inner.width() <= 0.0
        || inner.height() <= metrics.titleBarHeight + tabHeight) {
        return reject(error, QStringLiteral("outer frame is too small for chrome metrics"));
    }
    plan.outerTitleBar = {inner.left(), inner.top(), inner.width(), metrics.titleBarHeight};
    plan.tabStrip = {inner.left(), plan.outerTitleBar.bottom(), inner.width(), tabHeight};
    plan.contentRect = {inner.left(), plan.tabStrip.bottom(), inner.width(),
                        inner.bottom() - plan.tabStrip.bottom()};

    const auto actions = actionOrder(request.style, request.maximized);
    const auto actionCount = static_cast<qreal>(actions.size());
    const qreal clusterWidth = actionCount * metrics.buttonExtent
        + (actionCount - 1.0) * metrics.buttonSpacing;
    qreal buttonX = request.style.buttonSide == ButtonSide::Left
        ? plan.outerTitleBar.left() + metrics.buttonClusterInset
        : plan.outerTitleBar.right() - metrics.buttonClusterInset - clusterWidth;
    const qreal buttonY = plan.outerTitleBar.center().y() - metrics.buttonExtent / 2.0;
    for (const auto action : actions) {
        plan.buttons.append({action,
                             {buttonX, buttonY, metrics.buttonExtent, metrics.buttonExtent},
                             buttonColor(request.style, action),
                             glyph(action),
                             !request.style.hoverGlyphs});
        buttonX += metrics.buttonExtent + metrics.buttonSpacing;
    }
    if (request.style.buttonSide == ButtonSide::Left) {
        const qreal left = plan.buttons.constLast().rect.right() + metrics.titleHorizontalInset;
        plan.outerTitleDragRect = {left, plan.outerTitleBar.top(),
                                   plan.outerTitleBar.right() - metrics.titleHorizontalInset - left,
                                   plan.outerTitleBar.height()};
    } else {
        const qreal left = plan.outerTitleBar.left() + metrics.titleHorizontalInset;
        const qreal right = plan.buttons.constFirst().rect.left() - metrics.titleHorizontalInset;
        plan.outerTitleDragRect = {left, plan.outerTitleBar.top(), right - left,
                                   plan.outerTitleBar.height()};
    }
    if (!plan.outerTitleDragRect.isValid()) {
        return reject(error, QStringLiteral("outer frame is too narrow for window controls"));
    }

    if (!request.tabs.isEmpty()) {
        const auto tabCount = static_cast<qreal>(request.tabs.size());
        const qreal availableWidth = plan.tabStrip.width() - 2.0 * metrics.tabHorizontalInset
            - metrics.tabSpacing * (tabCount - 1.0);
        if (availableWidth <= 0.0) {
            return reject(error, QStringLiteral("tab strip is too narrow for configured spacing"));
        }
        const qreal evenWidth = availableWidth / tabCount;
        plan.tabsOverflowed = evenWidth < metrics.tabMinimumWidth;
        const qreal tabWidth = plan.tabsOverflowed
            ? evenWidth
            : std::min(evenWidth, metrics.tabMaximumWidth);
        qreal tabX = request.style.tabDirection == TabVisualDirection::LeftToRight
            ? plan.tabStrip.left() + metrics.tabHorizontalInset
            : plan.tabStrip.right() - metrics.tabHorizontalInset - tabWidth;
        for (qsizetype index = 0; index < request.tabs.size(); ++index) {
            const auto &tab = request.tabs[index];
            plan.tabs.append({tab.tabId, tab.title, index,
                              {tabX, plan.tabStrip.top(), tabWidth, plan.tabStrip.height()},
                              tab.active});
            const qreal advance = tabWidth + metrics.tabSpacing;
            tabX += request.style.tabDirection == TabVisualDirection::LeftToRight
                ? advance
                : -advance;
        }
    }

    for (const auto &member : request.members) {
        if (!plan.contentRect.contains(member.windowRect)) {
            return reject(error, QStringLiteral("member '%1' lies outside chrome content")
                                     .arg(member.memberId));
        }
        const qreal titleHeight = std::min(member.windowRect.height(), metrics.memberTitleHeight);
        plan.members.append({member.memberId, member.title, member.windowRect,
                             {member.windowRect.left(), member.windowRect.top(),
                              member.windowRect.width(), titleHeight}});
    }

    for (const auto &divider : request.dividers) {
        QRectF visual;
        QRectF hit;
        if (divider.orientation == DividerOrientation::Vertical) {
            if (divider.position <= plan.contentRect.left()
                || divider.position >= plan.contentRect.right()
                || divider.spanStart < plan.contentRect.top()
                || divider.spanEnd > plan.contentRect.bottom()) {
                return reject(error, QStringLiteral("divider '%1' lies outside chrome content")
                                         .arg(divider.dividerId));
            }
            visual = {divider.position - metrics.dividerVisualThickness / 2.0,
                      divider.spanStart, metrics.dividerVisualThickness,
                      divider.spanEnd - divider.spanStart};
            hit = {divider.position - metrics.dividerHitThickness / 2.0,
                   divider.spanStart, metrics.dividerHitThickness,
                   divider.spanEnd - divider.spanStart};
        } else {
            if (divider.position <= plan.contentRect.top()
                || divider.position >= plan.contentRect.bottom()
                || divider.spanStart < plan.contentRect.left()
                || divider.spanEnd > plan.contentRect.right()) {
                return reject(error, QStringLiteral("divider '%1' lies outside chrome content")
                                         .arg(divider.dividerId));
            }
            visual = {divider.spanStart,
                      divider.position - metrics.dividerVisualThickness / 2.0,
                      divider.spanEnd - divider.spanStart,
                      metrics.dividerVisualThickness};
            hit = {divider.spanStart,
                   divider.position - metrics.dividerHitThickness / 2.0,
                   divider.spanEnd - divider.spanStart,
                   metrics.dividerHitThickness};
        }
        visual = visual.intersected(plan.contentRect);
        hit = hit.intersected(plan.contentRect);
        if (!visual.isValid() || !hit.isValid()) {
            return reject(error, QStringLiteral("divider '%1' lies outside chrome content")
                                     .arg(divider.dividerId));
        }
        plan.dividers.append({divider.dividerId, divider.orientation, visual, hit});
    }
    return plan;
}

} // namespace QindaQt::HybridChrome
