// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interactiontypes.h"

#include <QRectF>

#include <optional>

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
    // The container's committed outer frame, for the modifier + right-button
    // resize chord's nearest-edge choice (ADR-0282). A resolver without
    // geometry returns nullopt and the chord resizes from the bottom-right.
    [[nodiscard]] virtual std::optional<QRectF> containerFrame(
        const QString &containerId) const
    {
        static_cast<void>(containerId);
        return std::nullopt;
    }
};

} // namespace QindaQt::HybridInput
