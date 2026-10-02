// SPDX-License-Identifier: GPL-3.0-or-later
// Private-bus RemoteDesktop backend lifetime. A synthetic compositor object
// stands in for the fork's org.kde.KWin.EIS.RemoteDesktop; actual EIS input
// through the private native compositor is a separate manager gate.
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include <QDBusContext>
#include <QDBusPendingReply>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::Portal::RemoteInput;
class Consent final : public AccessConsent {
public:
    using AccessConsent::AccessConsent;
    bool admitted() const override { return allowed; }
    void ask(RequestToken t, const AccessQuestion &q) override { token = t; question = q; ++asks; }
    void cancel(RequestToken t) override { if (token == t) { token = 0; ++cancels; } }
    bool allowed = true;
    RequestToken token = 0;
    AccessQuestion question;
    int asks = 0, cancels = 0;
};
class Compositor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.RemoteDesktop")
public:
    ~Compositor() override { for (const int fd : peers) ::close(fd); }
    QDBusUnixFileDescriptor transport(int &cookie) {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0) return {};
        peers << pair[0]; cookie = ++next;
        const QDBusUnixFileDescriptor fd(pair[1]); ::close(pair[1]);
        return fd;
    }
    QList<int> capabilities, disconnected, peers;
    QStringList callers;
    QDBusMessage held;
    bool delay = false;
    int next = 0;
public Q_SLOTS:
    QDBusUnixFileDescriptor connectToEIS(int types, int &cookie) {
        capabilities << types; callers << message().service();
        if (delay) { setDelayedReply(true); held = message(); return {}; }
        return transport(cookie);
    }
    void disconnect(int cookie) { disconnected << cookie; }
};
class ClosedSpy final : public QObject {
    Q_OBJECT
public:
    int count = 0;
public Q_SLOTS:
    void closed() { ++count; }
};
class RemoteDesktopTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        directory = std::make_unique<QTemporaryDir>(); QVERIFY(directory->isValid());
        QFile config(directory->filePath(QStringLiteral("bus.conf"))); QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("<busconfig><type>session</type><listen>unix:tmpdir=" + directory->path().toUtf8()
            + "</listen><auth>EXTERNAL</auth><policy context='default'><allow send_destination='*'/>"
              "<allow receive_sender='*'/><allow own='*'/></policy></busconfig>"); config.close();
        daemon.start(QStringLiteral("/usr/bin/dbus-daemon"), {QStringLiteral("--nofork"),
            QStringLiteral("--print-address=1"), QStringLiteral("--config-file=") + config.fileName()});
        QVERIFY(daemon.waitForStarted()); QVERIFY(daemon.waitForReadyRead());
        address = QString::fromUtf8(daemon.readLine()).trimmed();
        for (auto *name : {&serviceName, &clientName, &compositorName, &strangerName}) *name = QUuid::createUuid().toString();
        service = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, serviceName));
        client = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, clientName));
        compositorBus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, compositorName));
        stranger = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, strangerName));
        QVERIFY(service->isConnected() && client->isConnected() && compositorBus->isConnected() && stranger->isConnected());
        QVERIFY(client->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        compositor = std::make_unique<Compositor>();
        QVERIFY(compositorBus->registerObject(QStringLiteral("/org/kde/KWin/EIS/RemoteDesktop"), compositor.get(),
                                              QDBusConnection::ExportAllSlots));
        selectedCompositor = compositorBus->baseService();
        host = std::make_unique<QObject>(); registry = std::make_unique<RequestRegistry>(*service);
        consent = std::make_unique<Consent>();
        eis = std::make_unique<CompositorEis>(*service, [this] { return selectedCompositor; });
        new RemoteDesktopAdaptor(*host, *registry, *consent, *eis, *service);
        QVERIFY(service->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(service->registerService(QStringLiteral("org.test.Portal")));
        const QString caller = client->baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
        session = QStringLiteral("/org/freedesktop/portal/desktop/session/%1/s1").arg(caller);
        requestPrefix = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/r").arg(caller);
        spy = std::make_unique<ClosedSpy>();
        QVERIFY(client->connect(QString{}, session, QStringLiteral("org.freedesktop.impl.portal.Session"),
                                QStringLiteral("Closed"), spy.get(), SLOT(closed())));
    }
    void cleanup() {
        host.reset(); eis.reset(); registry.reset(); consent.reset(); compositor.reset(); spy.reset();
        service.reset(); client.reset(); compositorBus.reset(); stranger.reset();
        for (const auto &name : {serviceName, clientName, compositorName, strangerName}) QDBusConnection::disconnectFromBus(name);
        daemon.terminate(); if (!daemon.waitForFinished(2000)) { daemon.kill(); daemon.waitForFinished(2000); }
        directory.reset();
    }
    void grantConnectAndClose() {
        QVERIFY(started(Keyboard | Pointer));
        QCOMPARE(consent->question.appId, QStringLiteral("org.test.App"));
        QVERIFY(consent->question.subtitle.contains(QStringLiteral("keyboard, pointer")));
        QCOMPARE(lastResults.value(QStringLiteral("devices")).toUInt(), quint32(Keyboard | Pointer));
        QCOMPARE(lastResults.value(QStringLiteral("clipboard_enabled")).toBool(), false);
        const QDBusPendingReply<QDBusUnixFileDescriptor> fd = connectToEis();
        QVERIFY2(!fd.isError(), qPrintable(fd.error().message())); QVERIFY(fd.value().isValid());
        QCOMPARE(compositor->capabilities, QList<int>{int(Keyboard | Pointer)});
        QCOMPARE(compositor->callers, QStringList{service->baseService()});
        QVERIFY(connectToEis().isError()); // One transport per granted session.
        QCOMPARE(compositor->capabilities.size(), 1);
        QCOMPARE(sessionCall(*stranger, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
        QVERIFY(compositor->disconnected.isEmpty());
        QCOMPARE(sessionCall(*client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(compositor->disconnected, QList<int>{1});
        QCOMPARE(spy->count, 0); // Frontend-initiated Close needs no Closed signal.
        QCOMPARE(sessionCall(*client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void denialAndCancelCloseSession() {
        QVERIFY(selected(Pointer));
        auto start = request(QStringLiteral("Start"), {path(3), QVariant::fromValue(QDBusObjectPath(session)),
                                                      QStringLiteral("org.test.App"), QString{}, QVariantMap{}});
        QTRY_COMPARE(consent->asks, 1);
        Q_EMIT consent->completed(consent->token, RequestResponse::Cancelled, ChoiceValues{});
        QCOMPARE(response(start), 1U);
        QTRY_COMPARE(spy->count, 1);
        QVERIFY(connectToEis().isError());
        QVERIFY(compositor->capabilities.isEmpty());
        // Request.Close while consent is visible cancels the helper and session.
        session.replace(QLatin1String("/s1"), QLatin1String("/s2"));
        QVERIFY(selected(Pointer));
        start = request(QStringLiteral("Start"), {path(6), QVariant::fromValue(QDBusObjectPath(session)),
                                                 QStringLiteral("org.test.App"), QString{}, QVariantMap{}});
        QTRY_COMPARE(consent->asks, 2);
        auto close = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), requestPrefix + QLatin1Char('6'),
            QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QCOMPARE(client->call(close, QDBus::BlockWithGui).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(response(start), 1U); QCOMPARE(consent->cancels, 2);
        QCOMPARE(sessionCall(*client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void refusesUnstartedInvalidAndForeignCalls() {
        auto create = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("CreateSession"));
        create.setArguments({path(1), QVariant::fromValue(QDBusObjectPath(session)), QStringLiteral("org.test.App"), QVariantMap{}});
        QCOMPARE(stranger->call(create, QDBus::BlockWithGui).type(), QDBusMessage::ErrorMessage);
        QCOMPARE(response(request(QStringLiteral("CreateSession"), {path(1), QVariant::fromValue(QDBusObjectPath(session)),
                                                                   QStringLiteral("org.test.App"), QVariantMap{}})), 0U);
        QVERIFY(connectToEis().isError()); // Not granted yet.
        QCOMPARE(response(request(QStringLiteral("SelectDevices"), {path(2), QVariant::fromValue(QDBusObjectPath(session)),
            QStringLiteral("org.test.App"), QVariantMap{{QStringLiteral("types"), 8U}}})), 2U);
        QCOMPARE(response(request(QStringLiteral("SelectDevices"), {path(3), QVariant::fromValue(QDBusObjectPath(session)),
            QStringLiteral("org.test.Other"), QVariantMap{{QStringLiteral("types"), 1U}}})), 2U);
        auto notify = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("NotifyPointerMotion"));
        notify.setArguments({QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{}, 1.0, 1.0});
        const auto refused = client->call(notify, QDBus::BlockWithGui);
        QCOMPARE(refused.errorName(), QStringLiteral("org.freedesktop.DBus.Error.NotSupported"));
        QVERIFY(compositor->capabilities.isEmpty());
        QCOMPARE(property(QStringLiteral("AvailableDeviceTypes")), 7U);
        QCOMPARE(property(QStringLiteral("version")), 2U);
    }
    void nativeAuthorityLossEndsSessionAndTransport() {
        QVERIFY(started(Keyboard));
        QVERIFY(!connectToEis().isError());
        consent->allowed = false; Q_EMIT consent->authorityLost();
        QTRY_COMPARE(spy->count, 1);
        QCOMPARE(compositor->disconnected, QList<int>{1});
        QCOMPARE(sessionCall(*client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void frontendOwnerLossEndsTransport() {
        QVERIFY(started(Touchscreen));
        QVERIFY(!connectToEis().isError());
        QVERIFY(client->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_COMPARE(compositor->disconnected, QList<int>{1});
    }
    void lateTransportAfterCloseIsDisconnected() {
        QVERIFY(started(Pointer));
        compositor->delay = true;
        auto pending = asyncConnect();
        QTRY_VERIFY(compositor->held.type() == QDBusMessage::MethodCallMessage);
        QCOMPARE(sessionCall(*client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_VERIFY(pending.isFinished()); QVERIFY(pending.isError());
        int cookie = 0; const auto fd = compositor->transport(cookie);
        QVERIFY(compositorBus->send(compositor->held.createReply({QVariant::fromValue(fd), cookie})));
        QTRY_COMPARE(compositor->disconnected, QList<int>{cookie});
    }
    void replacedCompositorGetsNoTransport() {
        QVERIFY(started(Keyboard));
        selectedCompositor.clear();
        QVERIFY(connectToEis().isError());
        QVERIFY(compositor->capabilities.isEmpty());
    }
private:
    QVariant path(int n) const { return QVariant::fromValue(QDBusObjectPath(requestPrefix + QString::number(n))); }
    QDBusPendingCall request(const QString &member, const QVariantList &arguments) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), member);
        message.setArguments(arguments);
        return client->asyncCall(message, 10000);
    }
    quint32 response(const QDBusPendingCall &call) {
        // Same-thread private-bus peers need event processing while waiting.
        QTest::qWaitFor([&call] { return call.isFinished(); }, 10000);
        const QDBusPendingReply<quint32, QVariantMap> reply = call;
        if (reply.isError()) return 99U;
        lastResults = reply.argumentAt<1>();
        return reply.argumentAt<0>();
    }
    bool selected(quint32 devices) {
        const auto handle = QVariant::fromValue(QDBusObjectPath(session));
        return response(request(QStringLiteral("CreateSession"), {path(1), handle, QStringLiteral("org.test.App"), QVariantMap{}})) == 0U
            && response(request(QStringLiteral("SelectDevices"), {path(2), handle, QStringLiteral("org.test.App"),
                                                                 QVariantMap{{QStringLiteral("types"), devices}}})) == 0U;
    }
    bool started(quint32 devices) {
        if (!selected(devices)) return false;
        auto start = request(QStringLiteral("Start"), {path(3), QVariant::fromValue(QDBusObjectPath(session)),
                                                      QStringLiteral("org.test.App"), QString{}, QVariantMap{}});
        if (!QTest::qWaitFor([this] { return consent->asks == 1; })) return false;
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, ChoiceValues{});
        return response(start) == 0U;
    }
    QDBusPendingCall asyncConnect() {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("ConnectToEIS"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(session)), QStringLiteral("org.test.App"), QVariantMap{}});
        return client->asyncCall(message, 10000);
    }
    QDBusPendingReply<QDBusUnixFileDescriptor> connectToEis() {
        auto call = asyncConnect();
        QTest::qWaitFor([&call] { return call.isFinished(); }, 10000);
        return call;
    }
    QDBusMessage sessionCall(QDBusConnection &bus, const QString &member) {
        return bus.call(QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), session,
                                                      QStringLiteral("org.freedesktop.impl.portal.Session"), member),
                        QDBus::BlockWithGui);
    }
    uint property(const QString &name) {
        auto get = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        get.setArguments({QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), name});
        const QDBusReply<QDBusVariant> reply = client->call(get, QDBus::BlockWithGui);
        return reply.isValid() ? reply.value().variant().toUInt() : 0U;
    }
    QProcess daemon;
    QString address, serviceName, clientName, compositorName, strangerName, selectedCompositor, session, requestPrefix;
    QVariantMap lastResults;
    std::unique_ptr<QTemporaryDir> directory;
    std::unique_ptr<QDBusConnection> service, client, compositorBus, stranger;
    std::unique_ptr<QObject> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<Consent> consent;
    std::unique_ptr<CompositorEis> eis;
    std::unique_ptr<Compositor> compositor;
    std::unique_ptr<ClosedSpy> spy;
};
QTEST_GUILESS_MAIN(RemoteDesktopTest)
#include "tst_remote_desktop.moc"
