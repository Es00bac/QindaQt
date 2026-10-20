// SPDX-License-Identifier: GPL-3.0-or-later
#include "held_input_state_p.h"

namespace QindaQt::Controllers {
QList<InputChange> HeldInputState::press(const QString &token, QList<quint32> keys, quint32 mouse) {
    if (m_held.contains(token)) return {};
    m_held[token] = {keys, mouse};
    QList<InputChange> changes;
    for (quint32 key : keys)
        if (++m_keys[key] == 1) changes.append({InputChange::Key, key, true});
    if (mouse && ++m_buttons[mouse] == 1) changes.append({InputChange::Mouse, mouse, true});
    return changes;
}
QList<InputChange> HeldInputState::release(const QString &token) {
    if (!m_held.contains(token)) return {};
    const auto held = m_held.take(token);
    QList<InputChange> changes;
    if (held.mouse && --m_buttons[held.mouse] == 0) changes.append({InputChange::Mouse, held.mouse, false});
    for (auto it = held.keys.crbegin(); it != held.keys.crend(); ++it)
        if (--m_keys[*it] == 0) changes.append({InputChange::Key, *it, false});
    return changes;
}
} // namespace QindaQt::Controllers
