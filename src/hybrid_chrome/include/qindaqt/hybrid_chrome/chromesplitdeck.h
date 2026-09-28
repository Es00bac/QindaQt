// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainterPath>

#include <optional>

class QPainter;

namespace QindaQt::HybridChrome {

// The split-deck container title row (ADR-0281), the BeOS-like Corner Bar
// arrangement. Left: a title tab with the container's own name and its window
// buttons, sized to its content but never wider than TitleShare of the row
// (the name elides first). Right: a deck anchored to the right edge with the
// page tabs and the group controls, growing leftward, never wider than
// DeckShare. Between them at least MinimumGapShare of the row stays
// unpainted, and the hit tester returns nothing there, so a press in the gap
// reaches the window underneath (container chrome is a paint-only scene item;
// see ADR-0005).
//
// When the tabs do not fit side by side the deck becomes a card carousel: the
// active tab is a full-size front card in the middle, and its neighbours
// shrink and overlap behind it toward both ends, like cards on a wheel.
//
// Layout is a pure transformation of an engine-built plan; paint reads only
// the plan. Every tab stays a real ChromeRenderPlan::tabs entry in logical
// order, so accessibility and keyboard traversal see all of them.
class ChromeSplitDeck final
{
public:
    static constexpr qreal TitleShare = 0.20;
    static constexpr qreal DeckShare = 0.75;
    static constexpr qreal MinimumGapShare = 0.05;
    // A deck tab's width when the row has room; the metrics' tab maximum
    // still caps it.
    static constexpr qreal PreferredTabWidth = 168.0;
    // Cards painted on each side of the front card; deeper ones hide behind.
    static constexpr int VisibleDepth = 3;
    // Each step away from the front card scales the card by this much.
    static constexpr qreal DepthScale = 0.84;
    // The title tab always keeps this much drag surface beside its buttons,
    // even when that pushes it past TitleShare on a narrow container.
    static constexpr qreal MinimumTitleDragWidth = 24.0;
    // Padding either side of the painted name inside its label rect.
    static constexpr qreal TitleTextInset = 6.0;

    // The font the renderer paints the title with.
    //
    // AGENT-CONTRACT: like ChromeShadedBadge::labelFont(), this must match
    // the font the compositor's chrome painter carries (the application
    // default), or the title tab is sized for one font and painted in another.
    [[nodiscard]] static QFont titleFont();
    // The measured advance the caller passes as containerTitleWidth.
    [[nodiscard]] static qreal titleTextWidth(const QString &title,
                                              const QFontMetricsF &metrics);

    // Rearranges an engine-built, unshaded plan whose buttons and controls
    // were laid out classically. Returns false with *error when the row
    // cannot hold the buttons, the controls, and a usable tab area.
    [[nodiscard]] static bool layout(ChromeRenderPlan *plan,
                                     const ChromeLayoutRequest &request,
                                     QString *error);

    // The painted outline: the body below the title row plus the two title
    // pieces. The renderer fills, clips and strokes this instead of the full
    // outer rectangle so the gap stays transparent.
    [[nodiscard]] static QPainterPath silhouette(const ChromeRenderPlan &plan,
                                                 qreal cornerRadius);

    // True when `point` lies in an unshaded split-deck row's gap: inside the
    // outer frame and above the body, but on neither painted piece.
    //
    // AGENT-CONTRACT: callers that treat a container's outer frame as opaque
    // for stacking (KWinChromeManager::pointerHitAt stops at the topmost
    // frame under the pointer) must treat the gap as not part of the frame,
    // or chrome of a container below would be unreachable through it.
    [[nodiscard]] static bool inGap(const ChromeRenderPlan &plan, const QPointF &point);

    // The frontmost tab under `point` (smallest deckDepth), in logical order
    // index, or nothing.
    [[nodiscard]] static std::optional<qsizetype> tabAt(const ChromeRenderPlan &plan,
                                                        const QPointF &point);

    // The next tab a wheel step lands on: `step` < 0 moves toward logical
    // index 0. Stops at either end rather than wrapping, like a Qt tab bar.
    // Nothing when there is no active tab or the step would not move.
    [[nodiscard]] static std::optional<qsizetype> steppedTab(const ChromeRenderPlan &plan,
                                                             int step);

    // Title tab fill and name, deck background, and the tab cards (deepest
    // first). Called inside the renderer's clip; window buttons, controls
    // and the identity frame stay with the renderer.
    static void paint(QPainter &painter, const ChromeRenderPlan &plan,
                      const ChromePaintState &state);

    // Paint-only glide between two plans of the same deck (ADR-0281): tab
    // rects move linearly from `from` to `to` by `progress` in [0, 1]; every
    // other field, including depth order, is `to`'s. canAnimate() says
    // whether `from` and `to` are the same deck (same tabs in the same order
    // and the same frame size) with a different active tab.
    [[nodiscard]] static bool canAnimate(const ChromeRenderPlan &from,
                                         const ChromeRenderPlan &to);
    [[nodiscard]] static ChromeRenderPlan interpolate(const ChromeRenderPlan &from,
                                                      const ChromeRenderPlan &to,
                                                      qreal progress);
};

} // namespace QindaQt::HybridChrome
