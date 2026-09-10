// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybriddocktargetrouting.h"

#include <algorithm>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr qreal ContainerEdgeBandFraction = 0.15;
constexpr qreal MinimumContainerEdgeBand = 16.0;
constexpr qreal MaximumContainerEdgeBand = 64.0;
// AGENT-GUARD: The band must stay well under half of the shorter side, or a
// small container would classify every point as its edge and its member
// tiles could never receive a nested split or tab drop.
constexpr qreal MaximumContainerEdgeBandShare = 0.25;

} // namespace

qreal containerEdgeBand(const QRectF &contentFrame)
{
    if (!contentFrame.isValid()) {
        return 0.0;
    }
    const qreal shorter = std::min(contentFrame.width(), contentFrame.height());
    const qreal band = std::clamp(shorter * ContainerEdgeBandFraction,
                                  MinimumContainerEdgeBand,
                                  MaximumContainerEdgeBand);
    return std::min(band, shorter * MaximumContainerEdgeBandShare);
}

HybridInput::DockZone containerEdgeDockZone(const QRectF &contentFrame,
                                            const QPointF &position)
{
    if (!contentFrame.isValid() || !contentFrame.contains(position)) {
        return HybridInput::DockZone::None;
    }
    const qreal band = containerEdgeBand(contentFrame);
    const qreal left = position.x() - contentFrame.left();
    const qreal right = contentFrame.right() - position.x();
    const qreal top = position.y() - contentFrame.top();
    const qreal bottom = contentFrame.bottom() - position.y();
    const qreal nearest = std::min(std::min(left, right), std::min(top, bottom));
    if (nearest > band) {
        return HybridInput::DockZone::None;
    }
    if (nearest == left) {
        return HybridInput::DockZone::Left;
    }
    if (nearest == right) {
        return HybridInput::DockZone::Right;
    }
    if (nearest == top) {
        return HybridInput::DockZone::Top;
    }
    return HybridInput::DockZone::Bottom;
}

HybridInput::DockTarget tabDockTargetFromChromeHit(
    const HybridInput::HitTarget &hit)
{
    if (hit.kind != HybridInput::HitKind::Tab
        || hit.containerId.isEmpty() || hit.memberId.isEmpty()) {
        return {};
    }
    return {hit.containerId, hit.memberId, HybridInput::DockZone::Tab};
}

HybridInput::HitTarget sourceHitRespectingChromeExposure(
    bool chromeExposed,
    bool nativeIsChromeMember,
    const HybridInput::HitTarget &nativeTitle,
    const HybridInput::HitTarget &chromeHit)
{
    if (!chromeHit.isValid()) {
        return nativeTitle;
    }
    if (chromeExposed && !nativeIsChromeMember) {
        return chromeHit;
    }
    return nativeTitle.isValid() ? nativeTitle : HybridInput::HitTarget{};
}

HybridInput::DockTarget dockTargetRespectingChromeExposure(
    bool chromeExposed,
    bool nativeIsChromeMember,
    const HybridInput::DockTarget &chromeTarget,
    const HybridInput::DockTarget &nativeTarget)
{
    if (chromeTarget.isValid() && chromeExposed && !nativeIsChromeMember) {
        return chromeTarget;
    }
    return nativeTarget;
}

} // namespace QindaQt::Compositor::KWinIntegration
