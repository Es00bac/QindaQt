// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactiontargetresolver.h"

#include <QRectF>

#include <functional>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

class ChromeHitProvider
{
public:
    virtual ~ChromeHitProvider() = default;
    [[nodiscard]] virtual HybridInput::HitTarget hitTestChrome(
        const QPointF &position) const = 0;
};

class KWinInteractionTargetResolver final : public HybridInput::InteractionTargetResolver
{
public:
    using ChromeExposureResolver = std::function<bool(
        const QString &containerId,
        const QPointF &position,
        const QString &excludedWindowId)>;

    explicit KWinInteractionTargetResolver(
        const ManagedWindowRegistry &registry,
        const ChromeHitProvider *chrome = nullptr,
        ChromeExposureResolver chromeExposure = {});

    [[nodiscard]] HybridInput::HitTarget hitTest(
        const QPointF &position) const override;
    [[nodiscard]] HybridInput::DockTarget pointerDockTarget(
        const HybridInput::HitTarget &source,
        const QPointF &position) const override;
    [[nodiscard]] HybridInput::DockTarget keyboardDockTarget(
        const HybridInput::HitTarget &source,
        HybridInput::DockZone zone) const override;
    // Reactive enforcement seam: resolves the nearest *same-container* sibling
    // in a direction, for routing a native per-member geometry action (e.g. a
    // bare Meta+Arrow quick-tile request intercepted before it lands) through
    // the existing within-container dock commands instead of native KWin
    // geometry. Deliberately narrower than keyboardDockTarget, which may cross
    // container/independent-window boundaries.
    [[nodiscard]] HybridInput::DockTarget containerDirectionalTarget(
        const QString &containerId,
        const QString &sourceWindowId,
        HybridInput::DockZone zone) const;

private:
    [[nodiscard]] KWin::Window *topmostInputOwnerAt(
        const QPointF &position, const QString &excludedWindowId = {}) const;
    [[nodiscard]] bool chromeExposed(
        const HybridInput::HitTarget &hit,
        const QPointF &position,
        const QString &excludedWindowId = {}) const;
    [[nodiscard]] KWin::Window *directionalWindow(
        KWin::Window *source, HybridInput::DockZone zone,
        const QString &restrictToContainerId = {}) const;
    [[nodiscard]] HybridInput::DockTarget targetFor(
        KWin::Window *window, HybridInput::DockZone zone) const;
    [[nodiscard]] static HybridInput::DockZone zoneAt(
        const QRectF &frame, const QPointF &position);

    const ManagedWindowRegistry &m_registry;
    const ChromeHitProvider *m_chrome = nullptr;
    ChromeExposureResolver m_chromeExposure;
};

} // namespace QindaQt::Compositor::KWinIntegration
