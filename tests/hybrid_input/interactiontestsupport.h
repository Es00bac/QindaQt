// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactioncontroller.h"

#include <QHash>
#include <QVector>

namespace QindaQt::HybridInput::TestSupport {

class RecordingResolver final : public InteractionTargetResolver
{
public:
    HitTarget hit;
    DockTarget pointerTarget;
    QHash<DockZone, DockTarget> keyboardTargets;
    mutable QVector<QPointF> pointerQueries;
    mutable QVector<DockZone> keyboardQueries;

    HitTarget hitTest(const QPointF &) const override { return hit; }

    DockTarget pointerDockTarget(const HitTarget &, const QPointF &position) const override
    {
        pointerQueries.append(position);
        return pointerTarget;
    }

    DockTarget keyboardDockTarget(const HitTarget &, DockZone zone) const override
    {
        keyboardQueries.append(zone);
        return keyboardTargets.value(zone);
    }
};

inline PointerEvent pressAt(QPointF position, Qt::KeyboardModifiers modifiers)
{
    return {.position = position,
            .changedButton = Qt::LeftButton,
            .buttons = Qt::LeftButton,
            .modifiers = modifiers};
}

inline PointerEvent releaseAt(QPointF position)
{
    return {.position = position,
            .changedButton = Qt::LeftButton,
            .buttons = Qt::NoButton,
            .modifiers = Qt::NoModifier};
}

} // namespace QindaQt::HybridInput::TestSupport
