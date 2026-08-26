// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interactiontypes.h"

namespace QindaQt::HybridInput {

class InteractionTargetResolver
{
public:
    virtual ~InteractionTargetResolver() = default;

    [[nodiscard]] virtual HitTarget hitTest(const QPointF &position) const = 0;
    [[nodiscard]] virtual DockTarget pointerDockTarget(
        const HitTarget &source, const QPointF &position) const = 0;
    [[nodiscard]] virtual DockTarget keyboardDockTarget(
        const HitTarget &source, DockZone zone) const = 0;
};

} // namespace QindaQt::HybridInput
