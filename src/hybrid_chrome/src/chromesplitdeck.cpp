// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromesplitdeck.h"

#include "qindaqt/hybrid_chrome/chromeidentity.h"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::HybridChrome {
namespace {

// Tabs sit on the body: a small inset above keeps the row's top edge and the
// identity stripe visible over every card.
constexpr qreal TabTopInset = 3.0;
// A deck needs at least this much tab area; below it the row is rejected
// exactly like a classic row that cannot hold its tabs.
constexpr qreal MinimumTabArea = 16.0;
// Each step behind the front card lowers the card top by this share.
constexpr qreal DepthHeightStep = 0.14;
constexpr qreal MinimumCardHeightShare = 0.5;
constexpr qreal CardRadius = 5.0;
constexpr qreal MinimumLabelledCardWidth = 28.0;

bool reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

QString tabTitle(const ChromeLayoutRequest &request, const ChromeTabSpec &tab)
{
    const auto override = request.tabTitleOverrides.constFind(tab.tabId);
    if (override != request.tabTitleOverrides.cend() && !override->isEmpty()) {
        return *override;
    }
    return tab.title;
}

// Ink for text on `fill`: the preferred theme color while it reads at the
// identity text contrast, else the higher-contrast of black and white.
QColor readableInk(const QColor &fill, const QColor &preferred)
{
    return identityContrastRatio(preferred, fill) >= IdentityTextContrast
        ? preferred
        : identityInk(fill, preferred);
}

QColor mixed(const QColor &from, const QColor &to, qreal amount)
{
    const qreal keep = 1.0 - amount;
    return QColor::fromRgbF(static_cast<float>(from.redF() * keep + to.redF() * amount),
                            static_cast<float>(from.greenF() * keep + to.greenF() * amount),
                            static_cast<float>(from.blueF() * keep + to.blueF() * amount));
}

// Cumulative sideways exposure of the first `depth` cards on one side, as a
// share of that side's free width. Normalised over VisibleDepth cards so a
// side with few neighbours keeps the same spacing as a full one.
qreal exposureShare(int depth)
{
    qreal total = 0.0;
    qreal shown = 0.0;
    for (int step = 1; step <= ChromeSplitDeck::VisibleDepth; ++step) {
        const qreal weight = std::pow(ChromeSplitDeck::DepthScale, step);
        total += weight;
        if (step <= depth) {
            shown += weight;
        }
    }
    return total > 0.0 ? shown / total : 0.0;
}

void layoutTitleTab(ChromeRenderPlan *plan, const ChromeLayoutRequest &request)
{
    const auto &metrics = request.metrics;
    const QRectF row = plan->outerTitleBar;
    const qreal clusterLeft = plan->buttons.constFirst().rect.left();
    const qreal clusterWidth = plan->buttons.constLast().rect.right() - clusterLeft;
    const qreal inset = metrics.buttonClusterInset;
    const qreal gap = metrics.titleHorizontalInset;
    const qreal trailing = inset / 2.0;
    const qreal floorWidth = inset + clusterWidth + gap
        + ChromeSplitDeck::MinimumTitleDragWidth + trailing;
    const qreal cap = row.width() * ChromeSplitDeck::TitleShare;
    // Unknown width takes the whole share rather than guessing short.
    const qreal wanted = request.containerTitle.isEmpty() ? 0.0
        : request.containerTitleWidth > 0.0
        ? request.containerTitleWidth + 2.0 * ChromeSplitDeck::TitleTextInset
        : cap;
    const qreal contentWidth = inset + clusterWidth + gap + wanted + trailing;
    const qreal titleWidth = std::max(floorWidth, std::min(contentWidth, cap));

    const bool buttonsLeft = request.style.buttonSide == ButtonSide::Left;
    const qreal clusterX = buttonsLeft ? row.left() + inset
                                       : row.left() + titleWidth - inset - clusterWidth;
    const qreal shift = clusterX - clusterLeft;
    for (auto &button : plan->buttons) {
        button.rect.translate(shift, 0.0);
    }
    const QRectF cluster(clusterX, row.top(), clusterWidth, row.height());
    plan->titleLabelRect = buttonsLeft
        ? QRectF(cluster.right() + gap, row.top(),
                 row.left() + titleWidth - trailing - cluster.right() - gap, row.height())
        : QRectF(row.left() + trailing, row.top(),
                 cluster.left() - gap - row.left() - trailing, row.height());
    plan->titleTab = QRectF(QPointF(plan->outerFrame.left(), plan->outerFrame.top()),
                            QPointF(row.left() + titleWidth, row.bottom()));
    // The whole tab moves the container; the button cells win the hit test
    // because buttons are tested first.
    plan->outerTitleDragRect = QRectF(row.left(), row.top(), titleWidth, row.height());
    // Keyboard selection chip (ADR-0139) at the leading edge of the name.
    if (request.indexBadge > 0 && plan->titleLabelRect.width() > 44.0) {
        constexpr qreal badgeSize = 18.0;
        plan->indexBadgeRect = {plan->titleLabelRect.left(),
                                row.center().y() - badgeSize / 2.0, badgeSize, badgeSize};
        plan->titleLabelRect.setLeft(plan->indexBadgeRect.right() + 4.0);
    }
}

void placeInlineTabs(ChromeRenderPlan *plan, const ChromeLayoutRequest &request,
                     qreal tabsRight, qreal tabWidth)
{
    const auto &metrics = request.metrics;
    const QRectF row = plan->outerTitleBar;
    const auto count = static_cast<qreal>(request.tabs.size());
    const qreal total = count * tabWidth + (count - 1.0) * metrics.tabSpacing;
    const qreal tabsLeft = tabsRight - total;
    const bool leftToRight = request.style.tabDirection == TabVisualDirection::LeftToRight;
    for (qsizetype index = 0; index < request.tabs.size(); ++index) {
        const qreal slot = static_cast<qreal>(leftToRight ? index
                                                          : request.tabs.size() - 1 - index);
        const qreal x = tabsLeft + slot * (tabWidth + metrics.tabSpacing);
        const auto &tab = request.tabs[index];
        plan->tabs.append({tab.tabId, tabTitle(request, tab), index,
                           {x, row.top() + TabTopInset, tabWidth, row.height() - TabTopInset},
                           tab.active, 0});
    }
    plan->tabStrip = QRectF(tabsLeft, row.top(), total, row.height());
}

void placeCarousel(ChromeRenderPlan *plan, const ChromeLayoutRequest &request,
                   qreal tabsLeft, qreal tabsRight)
{
    const auto &metrics = request.metrics;
    const QRectF row = plan->outerTitleBar;
    const qreal available = tabsRight - tabsLeft;
    const qreal preferred = std::min(ChromeSplitDeck::PreferredTabWidth, metrics.tabMaximumWidth);
    const qreal frontWidth = std::min(
        available, std::max(std::min(metrics.tabMinimumWidth, available),
                            std::min(preferred, available * 0.5)));
    const qreal center = tabsLeft + available / 2.0;
    const qreal sideRoom = (available - frontWidth) / 2.0;
    const qreal fullHeight = row.height() - TabTopInset;
    qsizetype activeIndex = 0;
    for (qsizetype index = 0; index < request.tabs.size(); ++index) {
        if (request.tabs[index].active) {
            activeIndex = index;
        }
    }
    const bool leftToRight = request.style.tabDirection == TabVisualDirection::LeftToRight;
    for (qsizetype index = 0; index < request.tabs.size(); ++index) {
        const auto logicalOffset = index - activeIndex;
        const int depth = static_cast<int>(std::abs(logicalOffset));
        const int shown = std::min(depth, ChromeSplitDeck::VisibleDepth);
        // +1 places the card to the right of the front card.
        const qreal side = (logicalOffset > 0) == leftToRight ? 1.0 : -1.0;
        QRectF card;
        if (depth == 0) {
            card = {center - frontWidth / 2.0, row.top() + TabTopInset, frontWidth, fullHeight};
        } else {
            const qreal exposed = sideRoom * exposureShare(shown);
            const qreal ownExposure = sideRoom * (exposureShare(shown) - exposureShare(shown - 1));
            // A card always reaches back under its nearer neighbour so the
            // stack reads as overlapped cards rather than a row of slivers.
            const qreal width = std::min(frontWidth,
                                         std::max(frontWidth * std::pow(ChromeSplitDeck::DepthScale,
                                                                        shown),
                                                  ownExposure + 6.0));
            const qreal height = fullHeight
                * std::max(MinimumCardHeightShare, 1.0 - DepthHeightStep * shown);
            const qreal outerEdge = center + side * (frontWidth / 2.0 + exposed);
            const qreal left = side > 0.0 ? outerEdge - width : outerEdge;
            card = {left, row.bottom() - height, width, height};
        }
        const auto &tab = request.tabs[index];
        plan->tabs.append({tab.tabId, tabTitle(request, tab), index, card, tab.active, depth});
    }
    plan->tabStrip = QRectF(tabsLeft, row.top(), available, row.height());
}

} // namespace

QFont ChromeSplitDeck::titleFont()
{
    return QFont();
}

qreal ChromeSplitDeck::titleTextWidth(const QString &title, const QFontMetricsF &metrics)
{
    return title.isEmpty() ? 0.0 : std::ceil(metrics.horizontalAdvance(title));
}

bool ChromeSplitDeck::layout(ChromeRenderPlan *plan, const ChromeLayoutRequest &request,
                             QString *error)
{
    const auto &metrics = request.metrics;
    const QRectF row = plan->outerTitleBar;
    if (plan->buttons.isEmpty() || plan->controls.isEmpty()) {
        return reject(error, QStringLiteral("split-deck row needs its buttons and controls"));
    }
    layoutTitleTab(plan, request);
    const qreal titleWidth = plan->titleTab.right() - row.left();

    // Group controls at the far right; the tabs grow leftward from them.
    const qreal extent = metrics.containerControlExtent;
    const qreal spacing = metrics.containerControlSpacing;
    const auto controlCount = static_cast<qreal>(plan->controls.size());
    const qreal controlsWidth = controlCount * extent + (controlCount - 1.0) * spacing;
    qreal controlX = row.right() - metrics.containerControlClusterInset - controlsWidth;
    for (auto &control : plan->controls) {
        control.rect = {controlX, row.center().y() - extent / 2.0, extent, extent};
        controlX += extent + spacing;
    }
    const qreal controlsLeft = plan->controls.constFirst().rect.left();
    const qreal tabsRight = controlsLeft - metrics.tabHorizontalInset;
    // AGENT-GUARD: the deck yields to the gap, never the gap to the deck. A
    // title tab pushed past its share on a narrow container shrinks the deck
    // so at least MinimumGapShare of the row stays input-transparent.
    const qreal deckWidth = std::min(row.width() * DeckShare,
                                     row.width() - titleWidth - row.width() * MinimumGapShare);
    const qreal tabsLimit = row.right() - deckWidth + metrics.tabHorizontalInset;
    const qreal available = tabsRight - tabsLimit;
    if (!request.tabs.isEmpty() && available < MinimumTabArea) {
        return reject(error, QStringLiteral("shared title row is too narrow for a split deck"));
    }
    if (request.tabs.isEmpty()) {
        plan->tabStrip = {};
        plan->deckPiece = QRectF(QPointF(controlsLeft - metrics.tabHorizontalInset,
                                         plan->outerFrame.top()),
                                 QPointF(plan->outerFrame.right(), row.bottom()));
        return true;
    }
    const auto count = static_cast<qreal>(request.tabs.size());
    const qreal evenWidth = (available - metrics.tabSpacing * (count - 1.0)) / count;
    const qreal tabWidth = std::min({evenWidth, PreferredTabWidth, metrics.tabMaximumWidth});
    plan->tabsOverflowed = tabWidth < metrics.tabMinimumWidth;
    if (plan->tabsOverflowed) {
        placeCarousel(plan, request, tabsLimit, tabsRight);
    } else {
        placeInlineTabs(plan, request, tabsRight, tabWidth);
    }
    plan->deckPiece = QRectF(QPointF(plan->tabStrip.left() - metrics.tabHorizontalInset,
                                     plan->outerFrame.top()),
                             QPointF(plan->outerFrame.right(), row.bottom()));
    return true;
}

QPainterPath ChromeSplitDeck::silhouette(const ChromeRenderPlan &plan, qreal cornerRadius)
{
    const QRectF frame = plan.outerFrame;
    const qreal rowBottom = plan.outerTitleBar.bottom();
    const QRectF bodyRect(frame.left(), rowBottom, frame.width(), frame.bottom() - rowBottom);
    QPainterPath outline;
    if (bodyRect.height() > 0.0) {
        const qreal bodyRadius = std::min(cornerRadius, bodyRect.height() / 2.0);
        outline.addRoundedRect(bodyRect, bodyRadius, bodyRadius);
        // Square the body's top corners: the row pieces sit on them.
        QPainterPath top;
        top.addRect(QRectF(bodyRect.left(), bodyRect.top(), bodyRect.width(),
                           std::min(bodyRadius, bodyRect.height())));
        outline = outline.united(top);
    }
    for (const auto &piece : {plan.titleTab, plan.deckPiece}) {
        if (!piece.isValid()) {
            continue;
        }
        const qreal radius = std::min(cornerRadius, piece.height() / 3.0);
        QPainterPath tab;
        // Extended down by its radius so its lower corners vanish into the
        // body and only the top corners round.
        tab.addRoundedRect(piece.adjusted(0.0, 0.0, 0.0, radius), radius, radius);
        outline = outline.united(tab);
    }
    return outline.simplified();
}

bool ChromeSplitDeck::inGap(const ChromeRenderPlan &plan, const QPointF &point)
{
    return !plan.shaded && plan.style.titleLayout == ContainerTitleLayout::SplitDeck
        && plan.outerFrame.contains(point) && point.y() < plan.outerTitleBar.bottom()
        && !plan.titleTab.contains(point) && !plan.deckPiece.contains(point);
}

std::optional<qsizetype> ChromeSplitDeck::tabAt(const ChromeRenderPlan &plan,
                                                const QPointF &point)
{
    std::optional<qsizetype> found;
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        const auto &tab = plan.tabs[index];
        if (tab.deckDepth > VisibleDepth || !tab.rect.contains(point)) {
            continue;
        }
        if (!found || tab.deckDepth < plan.tabs[*found].deckDepth) {
            found = index;
        }
    }
    return found;
}

std::optional<qsizetype> ChromeSplitDeck::steppedTab(const ChromeRenderPlan &plan, int step)
{
    const auto active = std::find_if(plan.tabs.cbegin(), plan.tabs.cend(),
                                     [](const TabGeometry &tab) { return tab.active; });
    if (active == plan.tabs.cend() || step == 0) {
        return std::nullopt;
    }
    const auto current = std::distance(plan.tabs.cbegin(), active);
    const auto next = std::clamp<qsizetype>(current + (step < 0 ? -1 : 1), 0,
                                            plan.tabs.size() - 1);
    return next == current ? std::nullopt : std::optional<qsizetype>(next);
}

void ChromeSplitDeck::paint(QPainter &painter, const ChromeRenderPlan &plan,
                            const ChromePaintState &state)
{
    const auto &palette = plan.style.palette;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Title tab: the theme's signature tab color, dimmed while unfocused.
    const QColor authored = plan.containerFocused ? palette.titleBar : palette.titleBarInactive;
    const QColor tabFill = authored.isValid() ? authored : palette.surfaceRaised;
    const QRectF titleRow(plan.titleTab.left(), plan.outerTitleBar.top(),
                          plan.titleTab.width(), plan.outerTitleBar.height());
    painter.fillRect(titleRow, tabFill);
    if (!plan.containerTitle.isEmpty() && plan.titleLabelRect.width() > 2.0 * TitleTextInset) {
        const QColor ink = readableInk(tabFill, plan.containerTitleIsGenerated ? palette.textMuted
                                                                               : palette.text);
        const QFontMetricsF metrics(painter.font());
        const QString elided = metrics.elidedText(
            plan.containerTitle, Qt::ElideRight,
            plan.titleLabelRect.width() - 2.0 * TitleTextInset);
        painter.setPen(ink);
        painter.drawText(plan.titleLabelRect.adjusted(TitleTextInset, 0.0, -TitleTextInset, 0.0),
                         Qt::AlignVCenter | Qt::AlignLeft, elided);
    }
    // Deck background, then cards deepest first so nearer cards overlap.
    painter.fillRect(QRectF(plan.deckPiece.left(), plan.outerTitleBar.top(),
                            plan.deckPiece.width(), plan.outerTitleBar.height()),
                     palette.surface);
    QVector<qsizetype> order;
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        if (plan.tabs[index].deckDepth <= VisibleDepth) {
            order.append(index);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&plan](qsizetype left, qsizetype right) {
        return plan.tabs[left].deckDepth > plan.tabs[right].deckDepth;
    });
    const QFontMetricsF metrics(painter.font());
    for (const auto index : order) {
        const auto &tab = plan.tabs[index];
        const bool hovered = state.hoveredTarget.kind == HitKind::Tab
            && state.hoveredTarget.stableId == tab.tabId;
        const qreal recede = std::min(1.0, tab.deckDepth / qreal(VisibleDepth + 1));
        const QColor fill = tab.active ? plan.identity.tabTint
            : hovered                  ? palette.surfaceRaised
                                       : mixed(palette.surfaceRaised, palette.surface,
                                               0.35 + 0.5 * recede);
        QPainterPath card;
        card.addRoundedRect(tab.rect.adjusted(0.0, 0.0, 0.0, CardRadius), CardRadius, CardRadius);
        card = card.intersected([&tab] {
            QPainterPath clip;
            clip.addRect(tab.rect);
            return clip;
        }());
        painter.setPen(QPen(palette.border, plan.borderHairline));
        painter.setBrush(fill);
        painter.drawPath(card);
        if (tab.rect.width() >= MinimumLabelledCardWidth) {
            const QColor ink = tab.active ? plan.identity.textOnFill
                                          : readableInk(fill, palette.textMuted);
            painter.setPen(ink);
            const QRectF text = tab.rect.adjusted(TitleTextInset, 0.0, -TitleTextInset, 0.0);
            painter.drawText(text, Qt::AlignCenter,
                             metrics.elidedText(tab.title, Qt::ElideRight, text.width()));
        }
        if (tab.active) {
            // ADR-0139 active-page underline, inside the card.
            const qreal inset = std::min(4.0, tab.rect.width() / 4.0);
            const qreal thickness = std::min(3.0, tab.rect.height());
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setPen(Qt::NoPen);
            painter.setBrush(plan.identity.border);
            painter.drawRect(QRectF(tab.rect.left() + inset, tab.rect.bottom() - thickness,
                                    tab.rect.width() - inset * 2.0, thickness));
            painter.restore();
        }
    }
    painter.restore();
}

bool ChromeSplitDeck::canAnimate(const ChromeRenderPlan &from, const ChromeRenderPlan &to)
{
    if (to.style.titleLayout != ContainerTitleLayout::SplitDeck
        || from.style.titleLayout != ContainerTitleLayout::SplitDeck || from.shaded || to.shaded
        || from.containerId != to.containerId || from.outerFrame.size() != to.outerFrame.size()
        || from.tabs.size() != to.tabs.size() || to.tabs.isEmpty()) {
        return false;
    }
    bool activeChanged = false;
    for (qsizetype index = 0; index < to.tabs.size(); ++index) {
        if (from.tabs[index].tabId != to.tabs[index].tabId) {
            return false;
        }
        activeChanged = activeChanged || from.tabs[index].active != to.tabs[index].active;
    }
    return activeChanged;
}

ChromeRenderPlan ChromeSplitDeck::interpolate(const ChromeRenderPlan &from,
                                              const ChromeRenderPlan &to, qreal progress)
{
    ChromeRenderPlan result = to;
    if (!canAnimate(from, to)) {
        return result;
    }
    const qreal t = std::clamp(progress, 0.0, 1.0);
    const auto lerp = [t](qreal a, qreal b) { return a + (b - a) * t; };
    for (qsizetype index = 0; index < result.tabs.size(); ++index) {
        const QRectF &a = from.tabs[index].rect;
        const QRectF &b = to.tabs[index].rect;
        result.tabs[index].rect = QRectF(QPointF(lerp(a.left(), b.left()), lerp(a.top(), b.top())),
                                         QPointF(lerp(a.right(), b.right()),
                                                 lerp(a.bottom(), b.bottom())));
    }
    return result;
}

} // namespace QindaQt::HybridChrome
