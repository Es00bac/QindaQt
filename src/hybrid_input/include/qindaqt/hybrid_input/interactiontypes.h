// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QString>
#include <QVector>
#include <Qt>

namespace QindaQt::HybridInput {

enum class HitKind {
    None,
    OuterTitle,
    MemberTitle,
    Tab,
    Divider,
    OuterResize,
};

enum class DockZone {
    None,
    Left,
    Right,
    Top,
    Bottom,
    Tab,
};

enum class InteractionKind {
    None,
    MemberDock,
    ContainerMove,
    DividerResize,
    ContainerResize,
};

enum class IntentPhase {
    Begin,
    Update,
    Commit,
    Cancel,
};

struct HitTarget
{
    HitTarget() = default;
    HitTarget(HitKind kind,
              QString containerId,
              QString memberId,
              QString dividerId,
              Qt::Edges edges = {},
              QString pageId = {});

    HitKind kind = HitKind::None;
    QString containerId;
    QString memberId;
    QString dividerId;
    // OuterResize carries one horizontal edge, one vertical edge, or both.
    // Other hit kinds leave this empty.
    Qt::Edges edges;
    // Tab sources carry the stable page identity in addition to a member
    // representative used by spatial target resolution. Pointer/member-title
    // sources leave this empty.
    QString pageId;

    [[nodiscard]] bool isValid() const;
    friend bool operator==(const HitTarget &, const HitTarget &) = default;
};

struct DockTarget
{
    QString containerId;
    QString memberId;
    DockZone zone = DockZone::None;

    [[nodiscard]] bool isValid() const;
    friend bool operator==(const DockTarget &, const DockTarget &) = default;
};

struct PointerEvent
{
    QPointF position;
    Qt::MouseButton changedButton = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
};

struct KeyEvent
{
    Qt::Key key = Qt::Key_unknown;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    bool pressed = true;
    bool autoRepeat = false;
};

struct InteractionIntent
{
    InteractionKind kind = InteractionKind::None;
    IntentPhase phase = IntentPhase::Update;
    HitTarget source;
    DockTarget target;
    QPointF position;
    // AGENT-CONTRACT: Geometry adapters apply every phase against the Begin
    // baseline, so this is cumulative logical displacement, not event delta.
    QPointF delta;
};

struct InteractionDecision
{
    bool consumed = false;
    QVector<InteractionIntent> intents;
};

struct InteractionBindings
{
    Qt::KeyboardModifiers pointerModifiers = Qt::MetaModifier | Qt::ShiftModifier;
    Qt::MouseButton pointerButton = Qt::LeftButton;
    qreal dragThreshold = 8.0;
    // One press or auto-repeat advances exactly this many logical pixels.
    qreal keyboardStep = 10.0;
};

} // namespace QindaQt::HybridInput
