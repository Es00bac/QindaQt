// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input_actions.h"
#include <QMap>
namespace QindaQt::InputActions {
// GUI-thread user configuration. Validates whole documents before adoption;
// atomic owner-only saves never replace a malformed or oversized library.
class MacroStore {
public:
    explicit MacroStore(QString path);
    bool load(QString &reason);
    bool set(const Macro &macro, QString &reason);
    bool remove(const QString &id, QString &reason);
    QList<Macro> values() const { return m_macros.values(); }
    Macro value(const QString &id) const { return m_macros.value(id); }
    bool contains(const QString &id) const { return m_macros.contains(id); }
private:
    bool save(const QMap<QString, Macro> &macros, QString &reason) const;
    QString m_path;
    QMap<QString, Macro> m_macros;
};
} // namespace QindaQt::InputActions
