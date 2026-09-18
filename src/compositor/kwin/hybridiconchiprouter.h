// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chromeiconchip.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct IconChipPointerHit final
{
    QString windowId;
    HybridChrome::IconChipHitKind target = HybridChrome::IconChipHitKind::None;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !windowId.isEmpty() && target != HybridChrome::IconChipHitKind::None;
    }
    friend bool operator==(const IconChipPointerHit &, const IconChipPointerHit &) = default;
};

struct IconChipDrag final
{
    QString windowId;
    HybridChrome::DragPhase phase = HybridChrome::DragPhase::Update;
    QPointF globalPosition;
    // Cumulative logical displacement from the original press.
    QPointF delta;

    friend bool operator==(const IconChipDrag &, const IconChipDrag &) = default;
};

struct IconChipContextMenuRequest final
{
    QString windowId;
    QPointF globalPosition;

    friend bool operator==(const IconChipContextMenuRequest &,
                           const IconChipContextMenuRequest &) = default;
};

// One normalized KWin event can change hover and terminate an interrupted
// drag, so routing returns an ordered value batch instead of acting inline.
struct IconChipPointerDecision final
{
    bool consumed = false;
    bool hoverChanged = false;
    std::optional<IconChipPointerHit> hovered;
    // A consumed body press raises the iconified window (and its chip)
    // immediately; the unroll happens only on a matching double-click.
    QVector<QString> raiseRequests;
    QVector<QString> unrollRequests;
    QVector<QString> closeRequests;
    QVector<IconChipDrag> drags;
    QVector<IconChipContextMenuRequest> contextMenus;
};

[[nodiscard]] bool hasIconChipDecisionOutput(
    const IconChipPointerDecision &decision) noexcept;

// Sibling of HybridChromePointerRouter for iconified-window chips
// (ADR-0203): owns the ordinary, modifier-free pointer sequence over a chip.
// Same modifier semantics as container chrome: any held modifier passes the
// event through, so the exact Meta+Shift+Left dock chord reaches the
// InteractionController with the chip as its source. No KWin, scene, or
// topology dependency; the resolver is borrowed and called on the input
// thread for the router's lifetime.
class HybridIconChipRouter final
{
public:
    using HitResolver =
        std::function<std::optional<IconChipPointerHit>(const QPointF &)>;

    explicit HybridIconChipRouter(HitResolver resolver,
                                  qreal dragThreshold = 8.0,
                                  qreal doubleClickIntervalMs = 400.0);

    [[nodiscard]] IconChipPointerDecision pointerMove(
        const HybridInput::PointerEvent &event);
    [[nodiscard]] IconChipPointerDecision pointerPress(
        const HybridInput::PointerEvent &event);
    [[nodiscard]] IconChipPointerDecision pointerRelease(
        const HybridInput::PointerEvent &event);
    // AGENT-CONTRACT: a modifier-free vertical wheel over a chip is consumed;
    // turned toward the user (negative awayFromUser) it requests the unroll.
    // A held grab, modifiers, and zero deltas pass through.
    [[nodiscard]] IconChipPointerDecision pointerWheel(const QPointF &position,
                                                       Qt::KeyboardModifiers modifiers,
                                                       qreal awayFromUser);
    [[nodiscard]] IconChipPointerDecision cancel();
    // Cancels a grab and forgets hover when the chip set is replaced.
    [[nodiscard]] IconChipPointerDecision invalidateTargets();

    [[nodiscard]] bool active() const noexcept { return m_pressed.has_value(); }

private:
    [[nodiscard]] std::optional<IconChipPointerHit> hitAt(const QPointF &position) const;
    void updateHover(const QPointF &position, IconChipPointerDecision *decision);
    void appendDrag(HybridChrome::DragPhase phase,
                    const QPointF &position,
                    IconChipPointerDecision *decision) const;
    void cancelPointer(IconChipPointerDecision *decision);
    void resetPointer() noexcept;
    void noteClick(const IconChipPointerHit &hit);
    [[nodiscard]] bool isDoubleClick(const IconChipPointerHit &hit) const;

    HitResolver m_resolver;
    qreal m_dragThreshold = 8.0;
    qreal m_doubleClickIntervalMs = 400.0;
    QElapsedTimer m_clickClock;
    qint64 m_lastClickMs = -1;
    QString m_lastClickWindow;
    std::optional<IconChipPointerHit> m_hovered;
    std::optional<IconChipPointerHit> m_pressed;
    QPointF m_pressPosition;
    QPointF m_lastPosition;
    Qt::MouseButton m_pressedButton = Qt::NoButton;
    bool m_dragActive = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
