// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecoration.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButtonGroup>

namespace QindaQt::Decoration {

void QindaDecoration::updateGeometry()
{
    const bool maximized = window()->isMaximized();
    // AGENT-CONTRACT: The compositor's member policy owns this process-local
    // marker (qindaqtContainerMember). Grouped members expose no resize grip
    // because their frames change only through container reflow; the veto in
    // KWinMemberPolicyManager is the enforcement side of the same contract.
    const bool member = containerMember();
    const auto chrome = chromeState();
    // Grouped leaves keep a native handlebar for ordinary detach and
    // per-window controls (ADR-0131): tall enough to grab, never a full title.
    // A window's title follows its button style and height option (ADR-0264).
    const qreal titleHeight = member ? DecorationMemberHandleHeight
                                     : decorationTitleHeight(chrome);
    setBorders(maximized ? QMarginsF(0.0, titleHeight, 0.0, 0.0)
                         : QMarginsF(1.0, titleHeight, 1.0, 1.0));
    setResizeOnlyBorders(decorationResizeOnlyBorders(maximized, member));
    setBorderRadius(KDecoration3::BorderRadius(
        maximized ? 0.0
                  : member ? DecorationMemberCornerRadius
                           : decorationFrameRadius(chrome, false)));
    // A translucent title material asks the compositor to blur what shows
    // through it (ADR-0207); opaque documents keep an empty region.
    setBlurRegion(!member && chrome.titleBlur && chrome.titleOpacity < 1.0
                      ? QRegion(QRect(0, 0, qRound(size().width()), qRound(titleHeight)))
                      : QRegion());

    if (member) {
        const bool right = effectiveButtonSide(chrome)
            == DecorationButtonSide::Right;
        const DecorationMemberHandleLayout layout = layoutMemberHandle(chrome, size());
        // AGENT-GUARD: expose only the shared button-free rectangle as native
        // title drag; widening this reintroduces control/drag overlap.
        setTitleBar(layout.dragRegion);
        auto *stoplights = right ? m_rightButtons : m_leftButtons;
        auto *more = right ? m_leftButtons : m_rightButtons;
        const QSizeF cell(DecorationMiniButtonCell, DecorationMiniButtonCell);
        for (auto *group : {stoplights, more}) {
            if (group == nullptr) {
                continue;
            }
            group->setSpacing(DecorationMiniButtonSpacing);
            for (auto *button : group->buttons()) {
                button->setGeometry(QRectF(QPointF(0.0, 0.0), cell));
            }
        }
        QRectF stoplightBounds;
        QRectF moreBounds;
        for (const auto &button : layout.buttons) {
            if (button.kind == DecorationButtonKind::More) {
                moreBounds = button.geometry;
            } else {
                stoplightBounds = stoplightBounds.isNull()
                    ? button.geometry : stoplightBounds.united(button.geometry);
            }
        }
        // Live groups skip unavailable actions. Align the smaller group to the
        // shared cluster edge while retaining the conservative clear region.
        if (stoplights != nullptr) {
            stoplights->setPos(right
                ? QPointF(stoplightBounds.right()
                              - stoplights->geometry().width(),
                          stoplightBounds.top())
                : stoplightBounds.topLeft());
        }
        if (more != nullptr) {
            more->setPos(moreBounds.topLeft());
        }
        updateVisualStyle();
        return;
    }

    setTitleBar(QRectF(0.0, 0.0, size().width(), titleHeight));

    // Button geometry comes from the shared painter's layout so the preview
    // and the live decoration place every cluster identically (ADR-0129).
    const auto layout = layoutDecorationButtons(chrome, size());
    auto *group = m_leftButtons != nullptr ? m_leftButtons : m_rightButtons;
    if (group != nullptr && !layout.isEmpty()) {
        const QSizeF extent = layout.constFirst().geometry.size();
        // The layout's own gap and edge inset (ADR-0264), so the live group
        // sits exactly where the Settings preview draws it.
        group->setSpacing(layout.size() > 1
                              ? layout.at(1).geometry.left() - layout.at(0).geometry.right()
                              : 0.0);
        for (auto *button : group->buttons()) {
            button->setGeometry(QRectF(QPointF(0.0, 0.0), extent));
        }
        if (group == m_rightButtons) {
            // The live group width skips hidden actions, so the cluster stays
            // flush with the right inset.
            const qreal inset = size().width() - layout.constLast().geometry.right();
            group->setPos(QPointF(size().width() - group->geometry().width() - inset,
                                  layout.constFirst().geometry.top()));
        } else {
            group->setPos(layout.constFirst().geometry.topLeft());
        }
    }
    updateVisualStyle();
}

} // namespace QindaQt::Decoration
