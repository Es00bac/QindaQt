// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Tests
{

// Private dbus-daemon fixture. Every Bluetooth D-Bus row runs against this
// isolated bus, never the ambient session bus: the service boundary under
// test is the constructing bus itself.
class PrivateBus final
{
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
        name = QStringLiteral("qindaqt-bluetooth-svc-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus()
    {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

} // namespace QindaQt::Tests
