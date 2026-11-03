// SPDX-License-Identifier: GPL-3.0-or-later
// A fake org.qindaqt.InputActions1 (GetSnapshot + Changed) for QindaLink tests.
// Registered on a private test connection under a caller-chosen service name.
#pragma once

#include <qindaqt/input_actions/input_actions.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Link::Test {

class FakeInputActions : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.InputActions1")
public:
    explicit FakeInputActions(QString serviceName)
        : m_service(std::move(serviceName)),
          m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-input-actions")))
    {
        m_ok = m_bus.isConnected()
            && m_bus.registerObject(QString::fromLatin1(InputActions::Object), this, QDBusConnection::ExportScriptableSlots)
            && m_bus.registerService(m_service);
    }
    ~FakeInputActions() override
    {
        m_bus.unregisterObject(QString::fromLatin1(InputActions::Object));
        m_bus.unregisterService(m_service);
        QDBusConnection::disconnectFromBus(QStringLiteral("fake-input-actions"));
    }
    [[nodiscard]] bool ok() const { return m_ok; }
    [[nodiscard]] const QString &service() const { return m_service; }

    // Replaces the library and emits Changed like MacroEndpoint::bump().
    void setMacros(QList<InputActions::Macro> macros)
    {
        m_macros = std::move(macros);
        ++m_revision;
        m_bus.send(QDBusMessage::createSignal(QString::fromLatin1(InputActions::Object),
                                              QString::fromLatin1(InputActions::Interface), QStringLiteral("Changed"))
                   << qulonglong(m_revision));
    }
    bool ready = true;

public Q_SLOTS:
    Q_SCRIPTABLE QString GetSnapshot() const
    {
        QJsonArray rows;
        for (const auto &m : m_macros) rows.append(QJsonObject{{QStringLiteral("config"), InputActions::macroJson(m)}});
        return QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1},
                                                           {QStringLiteral("revision"), QString::number(m_revision)},
                                                           {QStringLiteral("ready"), ready},
                                                           {QStringLiteral("error"), QString()},
                                                           {QStringLiteral("running"), QString()},
                                                           {QStringLiteral("macros"), rows}})
                                     .toJson(QJsonDocument::Compact));
    }

private:
    QString m_service;
    QDBusConnection m_bus;
    bool m_ok = false;
    quint64 m_revision = 1;
    QList<InputActions::Macro> m_macros;
};

} // namespace QindaQt::Link::Test
