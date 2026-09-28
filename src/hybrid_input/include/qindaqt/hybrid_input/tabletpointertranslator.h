// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interactiontypes.h"

#include <optional>

namespace QindaQt::HybridInput {

// ADR-0282: a stylus speaks the same window-management chords as a mouse.
// KWin delivers tablet tools as their own events (tip, barrel buttons, axis
// motion, proximity) that never pass through the pointer path, so this turns
// them into the PointerEvent sequence InteractionController already knows:
// the tip is the left button and the barrel button the right button. The
// caller supplies the keyboard modifiers (tablet events carry none).
//
// AGENT-CONTRACT: this class only translates. Whether a press is claimed is
// the controller's decision; the KWin filter forwards tablet events to the
// client untouched unless the controller consumed the press that began the
// gesture, so the barrel button's ordinary right-click and every drawing
// stroke are unchanged without the modifier.
class TabletPointerTranslator final
{
public:
    // Linux input codes for the stylus buttons (linux/input-event-codes.h).
    static constexpr quint32 ButtonStylus = 0x14b;
    static constexpr quint32 ButtonStylus2 = 0x14c;

    [[nodiscard]] static Qt::MouseButton mouseButtonForStylus(quint32 code) noexcept;

    [[nodiscard]] PointerEvent tip(bool down, const QPointF &position,
                                   Qt::KeyboardModifiers modifiers);
    // nullopt for a stylus button this translation does not map.
    [[nodiscard]] std::optional<PointerEvent> button(quint32 code, bool pressed,
                                                     Qt::KeyboardModifiers modifiers);
    [[nodiscard]] PointerEvent motion(const QPointF &position, Qt::KeyboardModifiers modifiers);
    // Leaving proximity releases whatever is still held: a lost tip-up or
    // button-up must never leave a gesture running (the same rule as a lost
    // mouse release). Returns the releases to deliver, in order.
    [[nodiscard]] QVector<PointerEvent> leaveProximity(Qt::KeyboardModifiers modifiers);

    [[nodiscard]] Qt::MouseButtons buttons() const noexcept { return m_buttons; }
    [[nodiscard]] QPointF position() const noexcept { return m_position; }

private:
    [[nodiscard]] PointerEvent change(Qt::MouseButton button, bool pressed,
                                      Qt::KeyboardModifiers modifiers);

    QPointF m_position;
    Qt::MouseButtons m_buttons;
};

} // namespace QindaQt::HybridInput
