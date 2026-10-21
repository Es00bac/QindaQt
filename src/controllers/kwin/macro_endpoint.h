// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller_input_device.h"
#include <QObject>
namespace QindaQt::Controllers {
class MacroEndpoint final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.InputActions1")
public:
    MacroEndpoint(InputActions::MacroStore &store, ControllerInputDevice &input);
    void shortcutStatus(const QString &id, const QString &reason, const QStringList &actual);
public Q_SLOTS:
    QString GetSnapshot() const;
    QString Apply(const QString &id, const QString &config, qulonglong revision);
    QString Reset(const QString &id, qulonglong revision);
    QString Run(const QString &id, qulonglong revision);
    void Cancel();
Q_SIGNALS:
    void Changed(qulonglong revision);
    void libraryChanged(const QString &editedShortcut);
private:
    QString result(bool ok, const QString &reason) const;
    void bump();
    InputActions::MacroStore &m_store;
    ControllerInputDevice &m_input;
    quint64 m_revision = 1;
    bool m_ready = false;
    QString m_error;
    QMap<QString, QJsonObject> m_shortcuts;
};
} // namespace QindaQt::Controllers
