// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace QindaQt::Controllers {
struct InputChange {
    enum Kind { Key, Mouse };
    Kind kind;
    quint32 code;
    bool down;
    friend bool operator==(const InputChange &, const InputChange &) = default;
};

// GUI-thread state owned by the compositor sink. Codes are opaque here; the
// sink maps bindings to platform codes and delivers the returned events.
// AGENT-GUARD: modifier presses precede mouse-down; mouse-up precedes modifier
// releases. Shared keys/buttons stay down until their last controller token
// releases, including suppression/disconnect cleanup (ADR-0426).
class HeldInputState {
public:
    bool contains(const QString &token) const { return m_held.contains(token); }
    QStringList tokens() const { return m_held.keys(); }
    QList<InputChange> press(const QString &token, QList<quint32> keys, quint32 mouse = 0);
    QList<InputChange> release(const QString &token);
private:
    struct Held { QList<quint32> keys; quint32 mouse; };
    QMap<QString, Held> m_held;
    QMap<quint32, int> m_keys, m_buttons;
};
} // namespace QindaQt::Controllers
