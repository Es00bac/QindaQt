// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore/QProcess>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>
#include <QtCore/QFile>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
namespace QindaQt::Tests {
// Explicit untyped daemon configuration: no standard session configuration,
// included host files or service directories. Owner-loss activation has no
// installed fallback. All connection names/addresses belong to this fixture.
class ProfilePolicyBus final {
public:
    bool start() {
        QFile config(root.filePath(QStringLiteral("bus.conf")));
        if (!root.isValid() || !config.open(QIODevice::WriteOnly)) return false;
        const QByteArray contents("<busconfig><listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>"
                     "<policy context=\"default\"><allow user=\"*\"/><allow own=\"*\"/>"
                     "<allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
                     "</policy></busconfig>");
        config.write(contents);
        config.close();
        daemon.start(QStringLiteral("dbus-daemon"),
            {QStringLiteral("--config-file=" ) + config.fileName(),
             QStringLiteral("--nofork"), QStringLiteral("--nopidfile"),
             QStringLiteral("--print-address=1")});
        if (!daemon.waitForStarted() || !daemon.waitForReadyRead()) return false;
        address = QString::fromUtf8(daemon.readLine()).trimmed();
        qInfo().noquote() << "source-profile-bus pid" << daemon.processId()
            << "root" << root.path() << "configuration-sha256"
            << QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex();
        return !address.isEmpty();
    }
    QDBusConnection open() {
        const QString name = QUuid::createUuid().toString(QUuid::Id128);
        names.append(name);
        return QDBusConnection::connectToBus(address, name);
    }
    ~ProfilePolicyBus() {
        for (const auto &name : names) QDBusConnection::disconnectFromBus(name);
        daemon.terminate();
        if (!daemon.waitForFinished(2000)) { daemon.kill(); daemon.waitForFinished(); }
    }
    QTemporaryDir root;
    QProcess daemon;
    QString address;
    QStringList names;
};
}
