// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/tabletpointertranslator.h"

namespace QindaQt::HybridInput {

Qt::MouseButton TabletPointerTranslator::mouseButtonForStylus(quint32 code) noexcept
{
    switch (code) {
    case ButtonStylus:
        return Qt::RightButton;
    case ButtonStylus2:
        return Qt::MiddleButton;
    default:
        return Qt::NoButton;
    }
}

PointerEvent TabletPointerTranslator::change(Qt::MouseButton button, bool pressed,
                                             Qt::KeyboardModifiers modifiers)
{
    m_buttons.setFlag(button, pressed);
    return {.position = m_position,
            .changedButton = button,
            .buttons = m_buttons,
            .modifiers = modifiers};
}

PointerEvent TabletPointerTranslator::tip(bool down, const QPointF &position,
                                          Qt::KeyboardModifiers modifiers)
{
    m_position = position;
    return change(Qt::LeftButton, down, modifiers);
}

std::optional<PointerEvent> TabletPointerTranslator::button(quint32 code, bool pressed,
                                                            Qt::KeyboardModifiers modifiers)
{
    const auto mapped = mouseButtonForStylus(code);
    if (mapped == Qt::NoButton) {
        return std::nullopt;
    }
    return change(mapped, pressed, modifiers);
}

PointerEvent TabletPointerTranslator::motion(const QPointF &position,
                                             Qt::KeyboardModifiers modifiers)
{
    m_position = position;
    return {.position = m_position,
            .changedButton = Qt::NoButton,
            .buttons = m_buttons,
            .modifiers = modifiers};
}

QVector<PointerEvent> TabletPointerTranslator::leaveProximity(Qt::KeyboardModifiers modifiers)
{
    QVector<PointerEvent> releases;
    for (const auto held : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
        if (m_buttons.testFlag(held)) {
            releases.append(change(held, false, modifiers));
        }
    }
    return releases;
}

} // namespace QindaQt::HybridInput
