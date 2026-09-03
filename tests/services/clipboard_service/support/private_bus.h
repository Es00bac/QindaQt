// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Tests {

class PrivateClipboardBus final {
public:
    bool start()
    {
        process.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        serviceConnectionName = QStringLiteral("qindaqt-clipboard-service-%1")
                                    .arg(QUuid::createUuid().toString(QUuid::Id128));
        serviceConnection = QDBusConnection::connectToBus(address, serviceConnectionName);
        return !address.isEmpty() && serviceConnection.isConnected();
    }

    QDBusConnection connectClient(const QString &suffix)
    {
        const QString name = QStringLiteral("qindaqt-clipboard-client-%1-%2")
                                 .arg(suffix, QUuid::createUuid().toString(QUuid::Id128));
        clientConnectionNames.append(name);
        return QDBusConnection::connectToBus(address, name);
    }

    ~PrivateClipboardBus()
    {
        for (const QString &name : clientConnectionNames) {
            QDBusConnection::disconnectFromBus(name);
        }
        if (!serviceConnectionName.isEmpty()) {
            QDBusConnection::disconnectFromBus(serviceConnectionName);
        }
        process.terminate();
        if (!process.waitForFinished(1'000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString serviceConnectionName;
    QStringList clientConnectionNames;
    QDBusConnection serviceConnection{QStringLiteral("invalid")};
};

} // namespace QindaQt::Tests
