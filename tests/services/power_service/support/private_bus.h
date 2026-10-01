// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QProcess>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>
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
        // AGENT-GUARD: Untyped, explicit configuration contains no host
        // include or activation directory. Standard --session appends installed
        // activation directories even to otherwise isolated private buses.
        QFile config(root.filePath(QStringLiteral("bus.conf")));
        if (!root.isValid() || !config.open(QIODevice::WriteOnly)) return false;
        const QByteArray contents("<busconfig><listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>"
                     "<policy context=\"default\"><allow user=\"*\"/><allow own=\"*\"/>"
                     "<allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
                     "</policy></busconfig>");
        config.write(contents);
        config.close();
        process.setProgram(QStringLiteral("dbus-daemon"));
        process.setArguments({QStringLiteral("--config-file=") + config.fileName(),
                              QStringLiteral("--nofork"), QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        qInfo().noquote() << "private-power-bus pid" << process.processId()
            << "root" << root.path() << "configuration-sha256"
            << QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex();
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

    QTemporaryDir root;
    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
    QList<QString> openedConnections;
};

} // namespace QindaQt::Tests
