// SPDX-License-Identifier: GPL-3.0-or-later
// Shared private dbus-daemon, frontend/caller/compositor/stranger peers and an
// injected consent port for the remote-input adaptor tests. Peers live in the
// test thread, so every wait must process events (no blocking QDBus::Block).
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <memory>

class FakeConsent final : public QindaQt::Services::Portal::AccessConsent {
public:
    using AccessConsent::AccessConsent;
    bool admitted() const override { return allowed; }
    void ask(QindaQt::Services::Portal::RequestToken t, const QindaQt::Services::Portal::AccessQuestion &q) override {
        token = t; question = q; ++asks;
    }
    void cancel(QindaQt::Services::Portal::RequestToken t) override { if (token == t) { token = 0; ++cancels; } }
    bool allowed = true;
    QindaQt::Services::Portal::RequestToken token = 0;
    QindaQt::Services::Portal::AccessQuestion question;
    int asks = 0, cancels = 0;
};

class PrivatePortalBus {
public:
    bool start() {
        directory = std::make_unique<QTemporaryDir>();
        if (!directory->isValid()) return false;
        QFile config(directory->filePath(QStringLiteral("bus.conf")));
        if (!config.open(QIODevice::WriteOnly)) return false;
        config.write("<busconfig><type>session</type><listen>unix:tmpdir=" + directory->path().toUtf8()
            + "</listen><auth>EXTERNAL</auth><policy context='default'><allow send_destination='*'/>"
              "<allow receive_sender='*'/><allow own='*'/></policy></busconfig>");
        config.close();
        daemon.start(QStringLiteral("/usr/bin/dbus-daemon"), {QStringLiteral("--nofork"),
            QStringLiteral("--print-address=1"), QStringLiteral("--config-file=") + config.fileName()});
        if (!daemon.waitForStarted() || !daemon.waitForReadyRead()) return false;
        const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
        for (auto *peer : {&service, &client, &compositor, &stranger}) {
            const QString name = QUuid::createUuid().toString();
            names << name;
            *peer = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, name));
            if (!(*peer)->isConnected()) return false;
        }
        const QString caller = client->baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
        session = QStringLiteral("/org/freedesktop/portal/desktop/session/%1/s1").arg(caller);
        requestPrefix = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/r").arg(caller);
        return client->registerService(QStringLiteral("org.freedesktop.portal.Desktop"));
    }
    void stop() {
        service.reset(); client.reset(); compositor.reset(); stranger.reset();
        for (const auto &name : std::as_const(names)) QDBusConnection::disconnectFromBus(name);
        names.clear();
        daemon.terminate();
        if (!daemon.waitForFinished(2000)) { daemon.kill(); daemon.waitForFinished(2000); }
        directory.reset();
    }
    QVariant request(int n) const { return QVariant::fromValue(QDBusObjectPath(requestPrefix + QString::number(n))); }
    QVariant sessionHandle() const { return QVariant::fromValue(QDBusObjectPath(session)); }
    QDBusPendingCall call(const QString &interface, const QString &member, const QVariantList &arguments) const {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
                                                      interface, member);
        message.setArguments(arguments);
        return client->asyncCall(message, 10000);
    }
    static bool wait(const QDBusPendingCall &call) { return QTest::qWaitFor([&call] { return call.isFinished(); }, 10000); }
    // Returns the standard response, or 99 for a D-Bus error; keeps results.
    quint32 response(const QDBusPendingCall &call) {
        wait(call);
        const QDBusPendingReply<quint32, QVariantMap> reply = call;
        if (reply.isError()) return 99U;
        results = reply.argumentAt<1>();
        return reply.argumentAt<0>();
    }
    QDBusMessage sessionCall(QDBusConnection &bus, const QString &member) const {
        return bus.call(QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), session,
                                                      QStringLiteral("org.freedesktop.impl.portal.Session"), member),
                        QDBus::BlockWithGui);
    }
    QProcess daemon;
    QStringList names;
    QString session, requestPrefix;
    QVariantMap results;
    std::unique_ptr<QTemporaryDir> directory;
    std::unique_ptr<QDBusConnection> service, client, compositor, stranger;
};
