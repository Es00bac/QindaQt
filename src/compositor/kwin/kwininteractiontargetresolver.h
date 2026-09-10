// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactiontargetresolver.h"

#include <QRectF>
#include <QSet>
#include <QStringList>

#include <functional>
#include <optional>

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
        const QSet<QString> &excludedWindowIds)>;
    // Committed active-page content frame (global logical coordinates) of a
    // container, or nullopt when it has no committed layout. It bounds the
    // container edge band that turns a drop near the container's own edge
    // into a whole-container target; absent, only member-tile zones apply.
    using ContainerContentFrameResolver =
        std::function<std::optional<QRectF>(const QString &containerId)>;
    // Every window id of one page, for excluding a dragged tab's complete
    // membership from drop discovery: a chrome press raises the source
    // container, so the dragged page's members float above the intended
    // target and would otherwise win the hit-test, docking the tab into (or
    // creating a tab in) its own container instead of the one underneath.
    using DraggedPageMembersResolver = std::function<QStringList(
        const QString &containerId, const QString &pageId)>;

    explicit KWinInteractionTargetResolver(
        const ManagedWindowRegistry &registry,
        const ChromeHitProvider *chrome = nullptr,
        ChromeExposureResolver chromeExposure = {},
        ContainerContentFrameResolver containerContentFrame = {},
        DraggedPageMembersResolver draggedPageMembers = {});

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
        const QPointF &position,
        const QSet<QString> &excludedWindowIds = {}) const;
    [[nodiscard]] bool chromeExposed(
        const HybridInput::HitTarget &hit,
        const QPointF &position,
        const QSet<QString> &excludedWindowIds = {}) const;
    // The dragged source's own exclusions: its member id plus, for a tab
    // drag, every member of its page (see DraggedPageMembersResolver).
    [[nodiscard]] QSet<QString> sourceExclusions(
        const HybridInput::HitTarget &source) const;
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
    ContainerContentFrameResolver m_containerContentFrame;
    DraggedPageMembersResolver m_draggedPageMembers;
};

} // namespace QindaQt::Compositor::KWinIntegration
