// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

#include <QString>

namespace QindaQt::Tests {

// One isolated dbus-daemon per fixture plus named connections to it. Nothing
// here ever touches the host session or system bus: every connection is made
// by explicit address. AGENT-GUARD: tests must not set DBUS_SYSTEM_BUS_ADDRESS
// in their own process environment; only spawned helper processes may carry it.
class PrivateBus final
{
public:
    bool start()
    {
        process.setProgram(QStringLiteral("dbus-daemon"));
        process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-power-upstream-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus()
    {
        for (const QString &open : openedConnections) {
            QDBusConnection::disconnectFromBus(open);
        }
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(2000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QDBusConnection openConnection(const QString &suffix)
    {
        const QString newConnectionName = name + QLatin1Char('-') + suffix;
        const QDBusConnection newConnection =
            QDBusConnection::connectToBus(address, newConnectionName);
        if (newConnection.isConnected()) {
            openedConnections.push_back(newConnectionName);
        }
        return newConnection;
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
    QList<QString> openedConnections;
};

} // namespace QindaQt::Tests
