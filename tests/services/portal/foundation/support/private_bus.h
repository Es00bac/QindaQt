// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <memory>
#include <vector>
class PortalPrivateBus final {
public:
    bool start() {
        if (!directory.isValid()) return false;
        QFile config(directory.filePath(QStringLiteral("bus.conf"))); if (!config.open(QIODevice::WriteOnly)) return false;
        config.write("<busconfig><type>session</type><listen>unix:tmpdir=" + directory.path().toUtf8()
            + "</listen><auth>EXTERNAL</auth><policy context='default'><allow send_destination='*'/>"
              "<allow receive_sender='*'/><allow own='*'/></policy></busconfig>"); config.close();
        daemon.start(QStringLiteral("/usr/bin/dbus-daemon"), {QStringLiteral("--nofork"),
            QStringLiteral("--print-address=1"), QStringLiteral("--config-file=") + config.fileName()});
        if (!daemon.waitForStarted() || !daemon.waitForReadyRead()) return false;
        address = QString::fromUtf8(daemon.readLine()).trimmed(); return !address.isEmpty();
    }
    std::unique_ptr<QDBusConnection> connect() {
        const auto name = QUuid::createUuid().toString(QUuid::Id128); names.append(name);
        return std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, name));
    }
    ~PortalPrivateBus() {
        for (const auto &name : names) QDBusConnection::disconnectFromBus(name);
        daemon.terminate(); if (!daemon.waitForFinished(2000)) { daemon.kill(); daemon.waitForFinished(2000); }
    }
private:
    QTemporaryDir directory; QProcess daemon; QString address; QStringList names;
};
