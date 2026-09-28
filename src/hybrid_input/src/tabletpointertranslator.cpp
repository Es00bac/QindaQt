// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/tabletpointertranslator.h"

#include "qindaqt/hybrid_input/containerchords.h"

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

std::optional<TabletPointerTranslator::ContactChord> TabletPointerTranslator::mapContact(
    TabletTool tool, Qt::KeyboardModifiers held,
    const std::optional<Qt::KeyboardModifiers> &dockChord) noexcept
{
    const auto modifier = windowManagementModifier(dockChord);
    if (modifier != Qt::NoModifier && dockChord) {
        if (held == *dockChord || (tool == TabletTool::Eraser && held == modifier)) {
            return ContactChord{.button = Qt::LeftButton, .modifiers = *dockChord};
        }
        if (tool == TabletTool::Pen && held == modifier) {
            return ContactChord{.button = Qt::LeftButton, .modifiers = modifier};
        }
        if (tool == TabletTool::Pen && held == (modifier | Qt::ControlModifier)) {
            return ContactChord{.button = Qt::RightButton, .modifiers = modifier};
        }
    }
    if (tool == TabletTool::Eraser) {
        return std::nullopt;
    }
    return ContactChord{.button = Qt::LeftButton, .modifiers = held};
}

std::optional<PointerEvent> TabletPointerTranslator::contact(
    TabletTool tool, bool down, const QPointF &position, Qt::KeyboardModifiers held,
    const std::optional<Qt::KeyboardModifiers> &dockChord)
{
    m_position = position;
    if (!down) {
        if (!m_contact) {
            return std::nullopt;
        }
        const ContactChord ended = *m_contact;
        m_contact.reset();
        return change(ended.button, false, ended.modifiers);
    }
    if (m_contact) {
        // KWin's move/resize filter consumed the previous lift.
        m_buttons.setFlag(m_contact->button, false);
        m_contact.reset();
    }
    const auto mapped = mapContact(tool, held, dockChord);
    if (!mapped) {
        return std::nullopt;
    }
    m_contact = mapped;
    return change(mapped->button, true, mapped->modifiers);
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
    // Motion during a mapped contact speaks the chord the press chose (an
    // eraser drag is a docking drag), not the raw keys still held.
    return {.position = m_position,
            .changedButton = Qt::NoButton,
            .buttons = m_buttons,
            .modifiers = m_contact ? m_contact->modifiers : modifiers};
}

QVector<PointerEvent> TabletPointerTranslator::leaveProximity(Qt::KeyboardModifiers modifiers)
{
    m_contact.reset();
    QVector<PointerEvent> releases;
    for (const auto held : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
        if (m_buttons.testFlag(held)) {
            releases.append(change(held, false, modifiers));
        }
    }
    return releases;
}

} // namespace QindaQt::HybridInput
