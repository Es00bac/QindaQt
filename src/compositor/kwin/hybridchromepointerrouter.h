// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QPointF>
#include <QVector>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct ChromePointerHit final
{
    QString containerId;
    HybridChrome::ChromeHitTarget target;

    [[nodiscard]] bool isValid() const noexcept;
    friend bool operator==(const ChromePointerHit &,
                           const ChromePointerHit &) = default;
};

struct RoutedChromeDrag final
{
    QString containerId;
    HybridChrome::ChromeDragEvent event;

    friend bool operator==(const RoutedChromeDrag &,
                           const RoutedChromeDrag &) = default;
};

struct ChromeContextMenuRequest final
{
    QString containerId;
    QPointF globalPosition;

    friend bool operator==(const ChromeContextMenuRequest &,
                           const ChromeContextMenuRequest &) = default;
};

// One normalized KWin event can clear hover and terminate an interrupted drag,
// so routing returns an ordered value batch instead of invoking policy inline.
struct ChromePointerDecision final
{
    bool consumed = false;
    bool hoverChanged = false;
    std::optional<ChromePointerHit> hovered;
    // A consumed shared-chrome press raises the collapsed group immediately;
    // activation/click policy still occurs only on a matching release.
    QVector<QString> containerRaiseRequests;
    QVector<ChromePointerHit> activations;
    QVector<RoutedChromeDrag> drags;
    QVector<ChromeContextMenuRequest> contextMenus;
};

// AGENT-CONTRACT: A consumed decision can carry a raise request without
// changing hover or starting an activation/drag. The KWin input adapter must
// still publish that batch before returning the consumed result to KWin.
[[nodiscard]] bool hasChromeDecisionOutput(
    const ChromePointerDecision &decision) noexcept;

// Owns the ordinary, modifier-free pointer grab for compositor-painted shared
// chrome. It has no KWin, QWidget, topology, or placement dependencies. The
// injected resolver is borrowed by value and must remain callable on the input
// thread for the router's lifetime.
class HybridChromePointerRouter final
{
public:
    using HitResolver =
        std::function<std::optional<ChromePointerHit>(const QPointF &)>;

    explicit HybridChromePointerRouter(HitResolver resolver,
                                       qreal dragThreshold = 8.0);

    [[nodiscard]] ChromePointerDecision pointerMove(
        const HybridInput::PointerEvent &event);
    [[nodiscard]] ChromePointerDecision pointerPress(
        const HybridInput::PointerEvent &event);
    [[nodiscard]] ChromePointerDecision pointerRelease(
        const HybridInput::PointerEvent &event);
    [[nodiscard]] ChromePointerDecision cancel();
    // Cancels a grab and forgets hover identity when a published topology or
    // overlay set is replaced. Unlike cancel(), this forces a paint clear.
    [[nodiscard]] ChromePointerDecision invalidateTargets();

    [[nodiscard]] bool active() const noexcept { return m_pressed.has_value(); }

private:
    [[nodiscard]] std::optional<ChromePointerHit> hitAt(
        const QPointF &position) const;
    void updateHover(const QPointF &position, ChromePointerDecision *decision);
    void appendDrag(HybridChrome::DragPhase phase,
                    const QPointF &position,
                    ChromePointerDecision *decision) const;
    void cancelPointer(ChromePointerDecision *decision);
    void resetPointer() noexcept;

    [[nodiscard]] static bool ownsOrdinaryInput(
        const HybridChrome::ChromeHitTarget &target) noexcept;
    [[nodiscard]] static bool ownsContextMenuInput(
        const HybridChrome::ChromeHitTarget &target) noexcept;
    [[nodiscard]] static bool isDragTarget(
        const HybridChrome::ChromeHitTarget &target) noexcept;
    [[nodiscard]] static bool isActivationTarget(
        const HybridChrome::ChromeHitTarget &target) noexcept;

    HitResolver m_resolver;
    qreal m_dragThreshold = 8.0;
    std::optional<ChromePointerHit> m_hovered;
    std::optional<ChromePointerHit> m_pressed;
    QPointF m_pressPosition;
    QPointF m_lastPosition;
    Qt::MouseButton m_pressedButton = Qt::NoButton;
    bool m_dragActive = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
