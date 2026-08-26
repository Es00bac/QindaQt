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

private:
    [[nodiscard]] KWin::Window *topmostInputOwnerAt(
        const QPointF &position, const QString &excludedWindowId = {}) const;
    [[nodiscard]] bool chromeExposed(
        const HybridInput::HitTarget &hit,
        const QPointF &position,
        const QString &excludedWindowId = {}) const;
    [[nodiscard]] KWin::Window *directionalWindow(
        KWin::Window *source, HybridInput::DockZone zone) const;
    [[nodiscard]] HybridInput::DockTarget targetFor(
        KWin::Window *window, HybridInput::DockZone zone) const;
    [[nodiscard]] static HybridInput::DockZone zoneAt(
        const QRectF &frame, const QPointF &position);

    const ManagedWindowRegistry &m_registry;
    const ChromeHitProvider *m_chrome = nullptr;
    ChromeExposureResolver m_chromeExposure;
};

} // namespace QindaQt::Compositor::KWinIntegration
