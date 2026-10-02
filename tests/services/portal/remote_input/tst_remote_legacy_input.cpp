// SPDX-License-Identifier: GPL-3.0-or-later
// Deprecated RemoteDesktop Notify* through the session's EIS context. A real
// libeis server stands behind the synthetic compositor object, so the test
// observes the exact events the compositor engine would receive; physical
// input through the native compositor remains a separate native gate.
#include "remote_input_fixture.h"
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include <QDBusContext>
#include <QSocketNotifier>
#include <libeis.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::Portal::RemoteInput;
class EisServer final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.RemoteDesktop")
public:
    EisServer() : m_eis(eis_new(nullptr)) {
        if (eis_setup_backend_fd(m_eis) != 0) return;
        m_notifier = new QSocketNotifier(eis_get_fd(m_eis), QSocketNotifier::Read, this);
        QObject::connect(m_notifier, &QSocketNotifier::activated, this, &EisServer::dispatch);
    }
    ~EisServer() override {
        for (auto *device : std::as_const(m_devices)) eis_device_unref(device);
        for (auto *seat : std::as_const(m_seats)) eis_seat_unref(seat);
        eis_unref(m_eis);
    }
    QStringList events;
    QList<int> disconnected;
    int clients = 0, gone = 0, next = 0;
public Q_SLOTS:
    QDBusUnixFileDescriptor connectToEIS(int, int &cookie) {
        const int fd = eis_backend_fd_add_client(m_eis);
        if (fd < 0) return {};
        cookie = ++next;
        const QDBusUnixFileDescriptor transport(fd); ::close(fd);
        return transport;
    }
    void disconnect(int cookie) { disconnected << cookie; }
private:
    void device(eis_seat *seat, std::initializer_list<eis_device_capability> capabilities) {
        auto *created = eis_seat_new_device(seat);
        eis_device_configure_name(created, "test device");
        for (const auto capability : capabilities) eis_device_configure_capability(created, capability);
        eis_device_add(created);
        eis_device_resume(created);
        m_devices << created;
    }
    void dispatch() {
        eis_dispatch(m_eis);
        while (auto *event = eis_get_event(m_eis)) {
            switch (eis_event_get_type(event)) {
            case EIS_EVENT_CLIENT_CONNECT: {
                auto *client = eis_event_get_client(event);
                eis_client_connect(client);
                auto *seat = eis_client_new_seat(client, "seat");
                for (const auto capability : {EIS_DEVICE_CAP_POINTER, EIS_DEVICE_CAP_BUTTON, EIS_DEVICE_CAP_SCROLL, EIS_DEVICE_CAP_KEYBOARD})
                    eis_seat_configure_capability(seat, capability);
                eis_seat_add(seat);
                m_seats << seat;
                ++clients;
                break;
            }
            case EIS_EVENT_SEAT_BIND:
                if (m_devices.isEmpty()) {
                    device(eis_event_get_seat(event), {EIS_DEVICE_CAP_POINTER, EIS_DEVICE_CAP_BUTTON, EIS_DEVICE_CAP_SCROLL});
                    device(eis_event_get_seat(event), {EIS_DEVICE_CAP_KEYBOARD});
                }
                break;
            case EIS_EVENT_POINTER_MOTION:
                events << QStringLiteral("motion %1 %2").arg(eis_event_pointer_get_dx(event)).arg(eis_event_pointer_get_dy(event));
                break;
            case EIS_EVENT_BUTTON_BUTTON:
                events << QStringLiteral("button %1 %2").arg(eis_event_button_get_button(event)).arg(eis_event_button_get_is_press(event));
                break;
            case EIS_EVENT_KEYBOARD_KEY:
                events << QStringLiteral("key %1 %2").arg(eis_event_keyboard_get_key(event)).arg(eis_event_keyboard_get_key_is_press(event));
                break;
            case EIS_EVENT_SCROLL_DISCRETE:
                events << QStringLiteral("discrete");
                break;
            case EIS_EVENT_CLIENT_DISCONNECT:
                ++gone;
                break;
            default:
                break;
            }
            eis_event_unref(event);
        }
    }
    eis *m_eis;
    QSocketNotifier *m_notifier = nullptr;
    QList<eis_seat *> m_seats;
    QList<eis_device *> m_devices;
};
class LegacyInputTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        QVERIFY(bus.start());
        server = std::make_unique<EisServer>();
        QVERIFY(bus.compositor->registerObject(QStringLiteral("/org/kde/KWin/EIS/RemoteDesktop"), server.get(), QDBusConnection::ExportAllSlots));
        compositorName = bus.compositor->baseService();
        host = std::make_unique<QObject>(); registry = std::make_unique<RequestRegistry>(*bus.service);
        consent = std::make_unique<FakeConsent>();
        eis = std::make_unique<CompositorEis>(*bus.service, [this] { return compositorName; });
        new RemoteDesktopAdaptor(*host, *registry, *consent, *eis, *bus.service);
        QVERIFY(bus.service->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(bus.service->registerService(QStringLiteral("org.test.Portal")));
    }
    void cleanup() { host.reset(); eis.reset(); registry.reset(); consent.reset(); server.reset(); bus.stop(); }
    void notifyReachesTheCompositorInOrderAndEndsWithTheSession() {
        QVERIFY(started(Keyboard | Pointer));
        // Sent before any EIS device exists: they wait, in order, for it.
        QCOMPARE(notify(QStringLiteral("NotifyPointerMotion"), {QVariantMap{}, 3.0, 4.0}).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(notify(QStringLiteral("NotifyPointerButton"), {QVariantMap{}, 272, 1U}).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(notify(QStringLiteral("NotifyKeyboardKeycode"), {QVariantMap{}, 30, 1U}).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(notify(QStringLiteral("NotifyPointerAxisDiscrete"), {QVariantMap{}, 0U, 1}).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(server->events, (QStringList{QStringLiteral("motion 3 4"), QStringLiteral("button 272 1"),
                                                  QStringLiteral("key 30 1"), QStringLiteral("discrete")}));
        QCOMPARE(server->clients, 1); // One context per session, opened lazily.
        auto transport = bus.call(QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("ConnectToEIS"),
                                  {bus.sessionHandle(), app, QVariantMap{}});
        PrivatePortalBus::wait(transport); QVERIFY(transport.isError());
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(server->disconnected, QList<int>{1});
        QTRY_COMPARE(server->gone, 1);
    }
    void notifyIsFencedByOwnerPhaseDeviceAndArguments() {
        QCOMPARE(bus.response(remote(QStringLiteral("CreateSession"), {bus.request(1), bus.sessionHandle(), app, QVariantMap{}})), 0U);
        QCOMPARE(notify(QStringLiteral("NotifyPointerMotion"), {QVariantMap{}, 1.0, 1.0}).errorName(), QStringLiteral("org.freedesktop.portal.Error.NotAllowed"));
        bus.session.replace(QLatin1String("/s1"), QLatin1String("/s2"));
        QVERIFY(started(Pointer));
        auto foreign = message(QStringLiteral("NotifyPointerMotion"), {QVariantMap{}, 1.0, 1.0});
        QCOMPARE(bus.stranger->call(foreign, QDBus::BlockWithGui).errorName(), QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));
        QCOMPARE(notify(QStringLiteral("NotifyKeyboardKeycode"), {QVariantMap{}, 30, 1U}).errorName(), QStringLiteral("org.freedesktop.portal.Error.NotAllowed"));
        QCOMPARE(notify(QStringLiteral("NotifyKeyboardKeysym"), {QVariantMap{}, 97, 1U}).errorName(), QStringLiteral("org.freedesktop.DBus.Error.NotSupported"));
        QCOMPARE(notify(QStringLiteral("NotifyPointerButton"), {QVariantMap{}, 272, 2U}).errorName(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
        QCOMPARE(notify(QStringLiteral("NotifyPointerMotionAbsolute"), {QVariantMap{}, 1U, 1.0, 1.0}).errorName(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
        QCOMPARE(server->events, QStringList{});
    }
    void nativeLockDisconnectsLegacyContext() {
        QVERIFY(started(Pointer));
        QCOMPARE(notify(QStringLiteral("NotifyPointerMotion"), {QVariantMap{}, 1.0, 2.0}).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(server->events, QStringList{QStringLiteral("motion 1 2")});
        consent->allowed = false; Q_EMIT consent->authorityLost();
        QTRY_COMPARE(server->disconnected, QList<int>{1});
        QTRY_COMPARE(server->gone, 1);
        QCOMPARE(notify(QStringLiteral("NotifyPointerMotion"), {QVariantMap{}, 1.0, 2.0}).errorName(), QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));
    }
private:
    QDBusPendingCall remote(const QString &member, const QVariantList &arguments) {
        return bus.call(QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), member, arguments);
    }
    QDBusMessage message(const QString &member, QVariantList arguments) const {
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
                                                   QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), member);
        arguments.prepend(bus.sessionHandle());
        call.setArguments(arguments);
        return call;
    }
    QDBusMessage notify(const QString &member, const QVariantList &arguments) {
        return bus.client->call(message(member, arguments), QDBus::BlockWithGui);
    }
    bool started(quint32 devices) {
        if (bus.response(remote(QStringLiteral("CreateSession"), {bus.request(1), bus.sessionHandle(), app, QVariantMap{}})) != 0U
            || bus.response(remote(QStringLiteral("SelectDevices"), {bus.request(2), bus.sessionHandle(), app,
                                                                    QVariantMap{{QStringLiteral("types"), devices}}})) != 0U)
            return false;
        const int asked = consent->asks;
        auto start = remote(QStringLiteral("Start"), {bus.request(3), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        if (!QTest::qWaitFor([&] { return consent->asks == asked + 1; })) return false;
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, ChoiceValues{});
        return bus.response(start) == 0U;
    }
    const QString app = QStringLiteral("org.test.App");
    PrivatePortalBus bus;
    QString compositorName;
    std::unique_ptr<QObject> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<FakeConsent> consent;
    std::unique_ptr<CompositorEis> eis;
    std::unique_ptr<EisServer> server;
};
QTEST_GUILESS_MAIN(LegacyInputTest)
#include "tst_remote_legacy_input.moc"
