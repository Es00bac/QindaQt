// SPDX-License-Identifier: GPL-3.0-or-later
// Combined RemoteDesktop + ScreenCast sessions as xdg-desktop-portal 1.20
// drives them: RemoteDesktop.CreateSession/SelectDevices, ScreenCast
// .SelectSources with the RemoteDesktop handle, then RemoteDesktop.Start. A fake
// capture port stands in for the protected producer; actual PipeWire nodes and
// the native compositor are the separate native gates.
#include "remote_input_fixture.h"
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include <qindaqt/services/portal/screencast_adaptor.h>
#include <QDBusContext>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::Portal::RemoteInput;
class Eis final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.RemoteDesktop")
public:
    QList<int> disconnected;
    int next = 0;
public Q_SLOTS:
    QDBusUnixFileDescriptor connectToEIS(int, int &cookie) {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0) return {};
        ::close(pair[0]); cookie = ++next;
        const QDBusUnixFileDescriptor fd(pair[1]); ::close(pair[1]);
        return fd;
    }
    void disconnect(int cookie) { disconnected << cookie; }
};
class Producer final : public CaptureUI {
public:
    bool admitted() const override { return allowed; }
    void request(RequestToken value, const CaptureRequest &r) override { token = value; current = r; ++opens; }
    void cancel(RequestToken value) override { if (token == value) token = 0; }
    void stop(const QString &path) override { stopped << path; }
    void revoke() override { allowed = false; Q_EMIT authorityLost(); }
    RequestToken token = 0; CaptureRequest current{}; bool allowed = true; int opens = 0; QStringList stopped;
};
class Closed final : public QObject {
    Q_OBJECT
public:
    int count = 0;
public Q_SLOTS:
    void closed() { ++count; }
};
class RemoteScreenCastTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        QVERIFY(bus.start());
        eisObject = std::make_unique<Eis>();
        QVERIFY(bus.compositor->registerObject(QStringLiteral("/org/kde/KWin/EIS/RemoteDesktop"), eisObject.get(), QDBusConnection::ExportAllSlots));
        compositorName = bus.compositor->baseService();
        host = std::make_unique<QObject>(); registry = std::make_unique<RequestRegistry>(*bus.service);
        consent = std::make_unique<FakeConsent>(); producer = std::make_unique<Producer>();
        eis = std::make_unique<CompositorEis>(*bus.service, [this] { return compositorName; });
        auto *desktop = new RemoteDesktopAdaptor(*host, *registry, *consent, *eis, *producer, *bus.service);
        new ScreenCastAdaptor(*host, *registry, *producer, *bus.service, &desktop->screenCastSources());
        QVERIFY(bus.service->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(bus.service->registerService(QStringLiteral("org.test.Portal")));
        spy = std::make_unique<Closed>();
        QVERIFY(bus.client->connect(QString{}, bus.session, QStringLiteral("org.freedesktop.impl.portal.Session"),
                                    QStringLiteral("Closed"), spy.get(), SLOT(closed())));
    }
    void cleanup() { host.reset(); eis.reset(); registry.reset(); consent.reset(); producer.reset(); eisObject.reset(); spy.reset(); bus.stop(); }
    void combinedSessionPublishesProducerStreamsAndStopsThem() {
        QVERIFY(selected(Keyboard | Pointer));
        QCOMPARE(bus.response(sources({{QStringLiteral("multiple"), true}, {QStringLiteral("cursor_mode"), 2U}})), 0U);
        auto start = remote(QStringLiteral("Start"), {bus.request(4), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        QTRY_COMPARE(consent->asks, 1);
        QVERIFY(consent->question.subtitle.contains(QStringLiteral("Screens to share")));
        QCOMPARE(producer->opens, 0); // Input consent precedes the producer's own choice.
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, ChoiceValues{});
        QTRY_COMPARE(producer->opens, 1);
        QVERIFY(producer->current.kind == CaptureKind::Stream);
        QCOMPARE(producer->current.session, bus.session);
        QCOMPARE(producer->current.caller, bus.client->baseService());
        QVERIFY(producer->current.multiple); QCOMPARE(producer->current.cursorMode, 2U);
        QVERIFY(!start.isFinished());
        Q_EMIT producer->completed(producer->token, RequestResponse::Success, streams({41, 42}));
        QCOMPARE(bus.response(start), 0U);
        QCOMPARE(bus.results.value(QStringLiteral("devices")).toUInt(), quint32(Keyboard | Pointer));
        QCOMPARE(bus.results.value(QStringLiteral("clipboard_enabled")).toBool(), false);
        const auto published = qdbus_cast<CaptureStreams>(bus.results.value(QStringLiteral("streams")).value<QDBusArgument>());
        QCOMPARE(published.size(), 2); QCOMPARE(published.at(0).node, 41U); QCOMPARE(published.at(1).node, 42U);
        QVERIFY(!bus.results.contains(QStringLiteral("restore_data")));
        auto transport = remote(QStringLiteral("ConnectToEIS"), {bus.sessionHandle(), app, QVariantMap{}});
        PrivatePortalBus::wait(transport); QVERIFY(!transport.isError());
        QVERIFY(producer->stopped.isEmpty());
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(producer->stopped, QStringList{bus.session});
        QTRY_COMPARE(eisObject->disconnected, QList<int>{1});
    }
    void refusedOrMismatchedShareClosesTheSession() {
        QVERIFY(selected(Pointer));
        QCOMPARE(bus.response(sources({})), 0U);
        auto start = granted();
        Q_EMIT producer->completed(producer->token, RequestResponse::Cancelled, {});
        QCOMPARE(bus.response(start), 1U);
        QTRY_COMPARE(spy->count, 1);
        QCOMPARE(producer->stopped, QStringList{bus.session});
        auto transport = remote(QStringLiteral("ConnectToEIS"), {bus.sessionHandle(), app, QVariantMap{}});
        PrivatePortalBus::wait(transport); QVERIFY(transport.isError());
        // A single selection can never publish two producer nodes.
        bus.session.replace(QLatin1String("/s1"), QLatin1String("/s2"));
        QVERIFY(selected(Pointer));
        QCOMPARE(bus.response(sources({})), 0U);
        start = granted();
        Q_EMIT producer->completed(producer->token, RequestResponse::Success, streams({41, 42}));
        QCOMPARE(bus.response(start), 2U);
        QTRY_VERIFY(producer->stopped.contains(bus.session));
    }
    void selectionIsFencedToTheOwnerOnceWithoutPersistence() {
        QVERIFY(selected(Keyboard));
        auto foreign = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.ScreenCast"), QStringLiteral("SelectSources"));
        foreign.setArguments({bus.request(3), bus.sessionHandle(), app, QVariantMap{}});
        const QDBusMessage refused = bus.stranger->call(foreign, QDBus::BlockWithGui);
        QVERIFY(refused.type() == QDBusMessage::ErrorMessage || refused.arguments().value(0).toUInt() == 2U);
        QCOMPARE(bus.response(sources({{QStringLiteral("persist_mode"), 2U}})), 2U);
        QCOMPARE(bus.response(bus.call(QStringLiteral("org.freedesktop.impl.portal.ScreenCast"), QStringLiteral("SelectSources"),
            {bus.request(3), bus.sessionHandle(), QStringLiteral("org.test.Other"), QVariantMap{}})), 2U);
        QCOMPARE(bus.response(sources({{QStringLiteral("types"), 2U}})), 2U);
        QCOMPARE(bus.response(sources({})), 0U);
        QCOMPARE(bus.response(sources({})), 2U); // Once per session, as the frontend enforces.
        QCOMPARE(producer->opens, 0);
    }
    void producerCompositorAndAuthorityLossCloseSessions() {
        QVERIFY(selected(Keyboard));
        QCOMPARE(bus.response(sources({})), 0U);
        auto start = granted();
        Q_EMIT producer->completed(producer->token, RequestResponse::Success, streams({51}));
        QCOMPARE(bus.response(start), 0U);
        Q_EMIT producer->closed(bus.session); // Stream ended or producer died.
        QTRY_COMPARE(spy->count, 1);
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
        // Compositor/session binding loss surfaces as capture authority loss.
        bus.session.replace(QLatin1String("/s1"), QLatin1String("/s2"));
        QVERIFY(bus.client->connect(QString{}, bus.session, QStringLiteral("org.freedesktop.impl.portal.Session"),
                                    QStringLiteral("Closed"), spy.get(), SLOT(closed())));
        QVERIFY(selected(Keyboard));
        QCOMPARE(bus.response(sources({})), 0U);
        start = granted();
        producer->revoke();
        QCOMPARE(bus.response(start), 2U);
        QTRY_COMPARE(spy->count, 2);
        QVERIFY(producer->stopped.contains(bus.session));
    }
    void callerLossStopsPendingShare() {
        QVERIFY(selected(Pointer));
        QCOMPARE(bus.response(sources({})), 0U);
        auto start = granted();
        Q_UNUSED(start);
        const QString name = bus.names.at(1);
        bus.client.reset(); QDBusConnection::disconnectFromBus(name);
        QTRY_VERIFY(producer->stopped.contains(bus.session));
        QVERIFY(!registry->live(producer->token));
    }
private:
    QDBusPendingCall remote(const QString &member, const QVariantList &arguments) {
        return bus.call(QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), member, arguments);
    }
    QDBusPendingCall sources(const QVariantMap &options) {
        return bus.call(QStringLiteral("org.freedesktop.impl.portal.ScreenCast"), QStringLiteral("SelectSources"),
                        {bus.request(3), bus.sessionHandle(), app, options});
    }
    bool selected(quint32 devices) {
        return bus.response(remote(QStringLiteral("CreateSession"), {bus.request(1), bus.sessionHandle(), app, QVariantMap{}})) == 0U
            && bus.response(remote(QStringLiteral("SelectDevices"), {bus.request(2), bus.sessionHandle(), app,
                                                                    QVariantMap{{QStringLiteral("types"), devices}}})) == 0U;
    }
    QDBusPendingCall granted() {
        const int asked = consent->asks, opened = producer->opens;
        auto start = remote(QStringLiteral("Start"), {bus.request(4), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        if (!QTest::qWaitFor([&] { return consent->asks == asked + 1; })) return start;
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, ChoiceValues{});
        QTest::qWaitFor([&] { return producer->opens == opened + 1; });
        return start;
    }
    static QVariantMap streams(const QList<quint32> &nodes) {
        const QVariantMap properties{{QStringLiteral("position"), QVariant::fromValue(CaptureCoordinate{0, 0})},
                                     {QStringLiteral("size"), QVariant::fromValue(CaptureCoordinate{800, 600})}};
        CaptureStreams list;
        for (const auto node : nodes) list.append({node, properties});
        return {{QStringLiteral("streams"), QVariant::fromValue(list)}};
    }
    const QString app = QStringLiteral("org.test.App");
    PrivatePortalBus bus;
    QString compositorName;
    std::unique_ptr<QObject> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<FakeConsent> consent;
    std::unique_ptr<Producer> producer;
    std::unique_ptr<CompositorEis> eis;
    std::unique_ptr<Eis> eisObject;
    std::unique_ptr<Closed> spy;
};
QTEST_GUILESS_MAIN(RemoteScreenCastTest)
#include "tst_remote_screencast.moc"
