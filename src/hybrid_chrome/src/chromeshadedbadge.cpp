// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"

#include "qindaqt/hybrid_chrome/chromeidentity.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>

namespace QindaQt::HybridChrome {
namespace {

constexpr qreal BadgeClusterGap = 8.0;
// The label rect paints its text inset by this much on each side, so the
// reserved width must include it or a measured label is elided by its own
// padding.
constexpr qreal BadgeLabelTextInset = 4.0;
constexpr qreal BadgePillHeight = 10.0;
constexpr qreal BadgePillWidth = 14.0;
constexpr qreal BadgePillSpacing = 4.0;
constexpr qreal BadgeOverflowWidth = 24.0;

QString overriddenTitle(const ChromeLayoutRequest &request, const ChromeTabSpec &tab)
{
    const auto override = request.tabTitleOverrides.constFind(tab.tabId);
    if (override != request.tabTitleOverrides.cend() && !override->isEmpty()) {
        return *override;
    }
    return tab.title;
}

qreal pillsWidth(qsizetype pillCount, qsizetype overflowCount)
{
    qreal width = 0.0;
    if (pillCount > 0) {
        width = static_cast<qreal>(pillCount) * BadgePillWidth
            + static_cast<qreal>(pillCount - 1) * BadgePillSpacing;
    }
    if (overflowCount > 0) {
        width += BadgeClusterGap + BadgeOverflowWidth;
    }
    return width;
}

} // namespace

QString ChromeShadedBadge::resolveLabel(const QString &containerTitle,
                                        const bool containerTitleIsGenerated,
                                        const QString &foremostTitle)
{
    // AGENT-GUARD (ADR-0168): never prefix a GENERATED name. Prefixing
    // "Container 7 · " consumed most of the label and elided away the title
    // the user could still read on the unrolled row — the reported "name
    // disappears when rolled up".
    if (containerTitle.isEmpty()) {
        return foremostTitle;
    }
    if (containerTitleIsGenerated) {
        return foremostTitle.isEmpty() ? containerTitle : foremostTitle;
    }
    return foremostTitle.isEmpty()
        ? containerTitle
        : containerTitle + QStringLiteral(" · ") + foremostTitle;
}

QFont ChromeShadedBadge::labelFont()
{
    return QFont();
}

qreal ChromeShadedBadge::labelWidthFor(const QString &label,
                                       const QFontMetricsF &metrics)
{
    if (label.isEmpty()) {
        return LabelMinimumWidth;
    }
    const qreal advance = metrics.horizontalAdvance(label);
    return qBound(LabelMinimumWidth, advance + 2.0 * BadgeLabelTextInset,
                  LabelMaximumWidth);
}

void ChromeShadedBadge::layout(ChromeRenderPlan *plan, const ChromeLayoutRequest &request)
{
    const auto &metrics = plan->metrics;
    const auto row = QRectF(plan->outerFrame.left() + metrics.outerBorder,
                            plan->outerFrame.top() + metrics.outerBorder,
                            plan->outerFrame.width() - 2.0 * metrics.outerBorder,
                            metrics.titleBarHeight);
    plan->outerTitleBar = row;
    plan->tabStrip = {};
    plan->contentRect = {};
    plan->buttons.clear();
    plan->tabs.clear();
    plan->tabsOverflowed = false;
    plan->badgeOverflowCount = 0;
    plan->badgeLabelRect = {};
    plan->outerTitleDragRect = row;

    const qreal inset = metrics.containerControlClusterInset;
    const qreal spacing = metrics.containerControlSpacing;
    const qreal extent = metrics.containerControlExtent;
    const qreal controlsWidth = static_cast<qreal>(plan->controls.size()) * extent
        + static_cast<qreal>(qMax<qsizetype>(0, plan->controls.size() - 1)) * spacing;
    // The badge keeps the engine's controls convention: the cluster sits on
    // the side opposite the window buttons, which for a badge (no window
    // buttons) reads as the leading edge for qinda macOS and the trailing
    // edge otherwise.
    const bool controlsLeading = plan->style.buttonSide != ButtonSide::Left;
    const qreal controlsY = row.center().y() - extent / 2.0;
    for (qsizetype index = 0; index < plan->controls.size(); ++index) {
        auto &control = plan->controls[index];
        const qreal offset = static_cast<qreal>(index) * (extent + spacing);
        const qreal x = controlsLeading
            ? row.left() + inset + offset
            : row.right() - inset - controlsWidth + offset;
        control.rect = {x, controlsY, extent, extent};
    }
    const qreal contentLeft = controlsLeading
        ? row.left() + inset + controlsWidth + BadgeClusterGap
        : row.left() + inset;
    const qreal contentRight = controlsLeading
        ? row.right() - inset
        : row.right() - inset - controlsWidth - BadgeClusterGap;

    // Fit the label and pills into the remaining width: shrink the label to
    // its minimum first, then drop pills into the overflow counter. A strip
    // narrower than promised by the strip geometry degrades gracefully
    // instead of overflowing.
    const qreal available = contentRight - contentLeft;
    const qsizetype tabCount = request.tabs.size();
    qsizetype pillCount = std::min(tabCount, MaxPills);
    qsizetype overflowCount = tabCount - pillCount;
    // ADR-0189: the label's desired width is measured by the caller and
    // carried on the request, so the strip geometry and this layout reserve
    // the same number. A request that carries none (an old caller, or an
    // unshaded request reaching here) falls back to the minimum rather than to
    // a fixed guess, because a guess is what used to elide real titles.
    const qreal desiredLabelWidth = request.badgeLabelWidth > 0.0
        ? qBound(LabelMinimumWidth, request.badgeLabelWidth, LabelMaximumWidth)
        : LabelMinimumWidth;
    qreal labelWidth = qMin(desiredLabelWidth,
                            qMax(LabelMinimumWidth,
                                 available - pillsWidth(pillCount, overflowCount)
                                     - BadgeClusterGap));
    // AGENT-GUARD: pills yield before the label does. The label is the only
    // thing on a rolled-up badge that says which page this is, so a strip that
    // cannot hold both drops pills into "+N" first and keeps the label at its
    // measured width for as long as the minimum allows.
    while (pillCount > 1
           && labelWidth + pillsWidth(pillCount, overflowCount) > available) {
        --pillCount;
        ++overflowCount;
    }

    plan->badgeLabelText = request.badgeLabelText;
    const qreal pillY = row.center().y() - BadgePillHeight / 2.0;
    qreal cursor = contentLeft;
    plan->badgeLabelRect = {cursor, row.top(), qMax(0.0, labelWidth), row.height()};
    cursor = plan->badgeLabelRect.right() + BadgeClusterGap;
    // AGENT-GUARD: Pills are plan tabs in request order. Reversing or
    // reordering them corrupts hit-test identity and accessibility names.
    for (qsizetype index = 0; index < pillCount; ++index) {
        const auto &tab = request.tabs.at(index);
        plan->tabs.append({tab.tabId, overriddenTitle(request, tab), index,
                           {cursor, pillY, BadgePillWidth, BadgePillHeight},
                           tab.active});
        cursor += BadgePillWidth + BadgePillSpacing;
    }
    plan->badgeOverflowCount = static_cast<int>(overflowCount);
    plan->tabsOverflowed = overflowCount > 0;
}

void ChromeShadedBadge::paint(QPainter &painter, const ChromeRenderPlan &plan)
{
    if (!plan.badgeLabelRect.isValid()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QFontMetricsF metrics(painter.font());

    // ADR-0189: the label was resolved once by the caller, measured there, and
    // carried on the plan. Re-deriving it here is what let the strip be sized
    // for one string and painted with another. The fallback keeps an old plan
    // (or a fixture that sets no label text) rendering the same text it always
    // did.
    QString label = plan.badgeLabelText;
    if (label.isEmpty()) {
        QString foremost;
        for (const auto &tab : plan.tabs) {
            if (tab.active) {
                foremost = tab.title;
                break;
            }
        }
        if (foremost.isEmpty() && !plan.tabs.isEmpty()) {
            foremost = plan.tabs.constFirst().title;
        }
        label = resolveLabel(plan.containerTitle, plan.containerTitleIsGenerated,
                             foremost);
    }
    if (!label.isEmpty() && plan.badgeLabelRect.width() > 12.0) {
        const auto elided = metrics.elidedText(
            label, Qt::ElideRight,
            qRound(plan.badgeLabelRect.width() - 2.0 * BadgeLabelTextInset));
        painter.setPen(plan.identity.badgeInk);
        painter.drawText(plan.badgeLabelRect.adjusted(BadgeLabelTextInset, 0.0,
                                                      -BadgeLabelTextInset, 0.0),
                         Qt::AlignVCenter | Qt::AlignLeft, elided);
    }

    // Pills: one tinted cap per shown tab, the active page strongest, then
    // the "+N" counter for the rest.
    const auto &surface = plan.style.palette.surface;
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        const auto &pill = plan.tabs.at(index);
        painter.setPen(Qt::NoPen);
        painter.setBrush(identityPillTint(plan.identity.base, surface,
                                          static_cast<int>(index), pill.active));
        painter.drawRoundedRect(pill.rect, BadgePillHeight / 2.0, BadgePillHeight / 2.0);
    }
    if (plan.badgeOverflowCount > 0 && !plan.tabs.isEmpty()) {
        const QRectF overflowRect(plan.tabs.constLast().rect.right() + BadgePillSpacing,
                                  plan.tabs.constLast().rect.top(),
                                  BadgeOverflowWidth, BadgePillHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(identityPillTint(plan.identity.base, surface,
                                          static_cast<int>(plan.tabs.size()), false));
        painter.drawRoundedRect(overflowRect, BadgePillHeight / 2.0, BadgePillHeight / 2.0);
        painter.setPen(plan.identity.badgeInk);
        painter.drawText(overflowRect, Qt::AlignCenter,
                         QStringLiteral("+%1").arg(plan.badgeOverflowCount));
    }
    painter.restore();
}

qreal ChromeShadedBadge::badgeWidth(const ChromeMetrics &metrics,
                                    const qsizetype tabCount,
                                    const qreal labelWidth)
{
    const qreal inset = metrics.containerControlClusterInset;
    const qreal controlsWidth = 3.0 * metrics.containerControlExtent
        + 2.0 * metrics.containerControlSpacing;
    const qreal pillCount = std::min<qreal>(static_cast<qreal>(qMax<qsizetype>(tabCount, 0)),
                                            static_cast<qreal>(MaxPills));
    const qreal overflowCount = qMax<qreal>(static_cast<qreal>(tabCount) - MaxPills, 0.0);
    const qreal label = qBound(LabelMinimumWidth, labelWidth, LabelMaximumWidth);
    return 2.0 * inset + controlsWidth + BadgeClusterGap + label
        + BadgeClusterGap + pillsWidth(static_cast<qsizetype>(pillCount),
                                       static_cast<qsizetype>(overflowCount));
}

} // namespace QindaQt::HybridChrome
