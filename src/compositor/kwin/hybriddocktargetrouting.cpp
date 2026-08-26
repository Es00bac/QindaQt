// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybriddocktargetrouting.h"

namespace QindaQt::Compositor::KWinIntegration {

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
