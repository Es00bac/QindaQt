// SPDX-License-Identifier: GPL-3.0-or-later
#include "held_input_state_p.h"

namespace QindaQt::Controllers {
QList<InputChange> HeldInputState::press(const QString &token, QList<quint32> keys, quint32 mouse) {
    return press(token, std::move(keys), mouse ? QList<quint32>{mouse} : QList<quint32>{});
}
QList<InputChange> HeldInputState::press(const QString &token, QList<quint32> keys, QList<quint32> mouseButtons) {
    if (m_held.contains(token)) return {};
    QList<quint32> uniqueKeys, uniqueButtons;
    for (const auto key : keys) if (!uniqueKeys.contains(key)) uniqueKeys.append(key);
    for (const auto button : mouseButtons) if (!uniqueButtons.contains(button)) uniqueButtons.append(button);
    keys = uniqueKeys; mouseButtons = uniqueButtons;
    m_held[token] = {keys, mouseButtons};
    QList<InputChange> changes;
    for (quint32 key : keys)
        if (++m_keys[key] == 1) changes.append({InputChange::Key, key, true});
    for (const auto mouse : mouseButtons)
        if (++m_buttons[mouse] == 1) changes.append({InputChange::Mouse, mouse, true});
    return changes;
}
QList<InputChange> HeldInputState::release(const QString &token) {
    if (!m_held.contains(token)) return {};
    const auto held = m_held.take(token);
    QList<InputChange> changes;
    for (auto it = held.mouseButtons.crbegin(); it != held.mouseButtons.crend(); ++it)
        if (--m_buttons[*it] == 0) changes.append({InputChange::Mouse, *it, false});
    for (auto it = held.keys.crbegin(); it != held.keys.crend(); ++it)
        if (--m_keys[*it] == 0) changes.append({InputChange::Key, *it, false});
    return changes;
}
} // namespace QindaQt::Controllers
