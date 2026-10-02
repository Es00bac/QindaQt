// SPDX-License-Identifier: GPL-3.0-or-later
// Private native remote-input journey: the real xdg-desktop-portal frontend,
// the production resident composition with mapped native consent input, and
// a private production compositor running the candidate eis plugin. Input is
// libei only; no host bus, input device, display or clipboard is touched.
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/services/portal/appearance_source.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <qindaqt/services/portal/foundation_composition.h>
#include <qindaqt/services/portal/resident_portal_service.h>
#include <qindaqt/services/secret_portal/secret_portal_adaptor.h>
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QProcess>
#include <QUuid>
#include <QtTest>
#include <libei.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
using namespace QindaQt::Services::Portal;
namespace {
class EmptyAppearance final : public AppearanceSource {
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    const std::optional<AppearanceTruth> &current() const override { return empty; }
    QString diagnostic() const override { return {}; }
private:
    std::optional<AppearanceTruth> empty;
};
// libei peer driven from QTRY expressions; never touches host devices.
struct Ei {
    Ei(bool sender, int fd) : context(sender ? ei_new_sender(nullptr) : ei_new_receiver(nullptr)) {
        ei_configure_name(context, "qindaqt-native-remote-input");
        ok = ei_setup_backend_fd(context, fd) == 0;
    }
    ~Ei() {
        for (auto *device : std::as_const(devices)) ei_device_unref(device);
        ei_unref(context);
    }
    bool pump() {
        ei_dispatch(context);
        while (auto *event = ei_get_event(context)) {
            const auto type = ei_event_get_type(event);
            seen << type;
            if (type == EI_EVENT_SEAT_ADDED)
                ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_POINTER_ABSOLUTE,
                                          EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_BUTTON, EI_DEVICE_CAP_SCROLL, nullptr);
            else if (type == EI_EVENT_DEVICE_ADDED) devices << ei_device_ref(ei_event_get_device(event));
            else if (type == EI_EVENT_DEVICE_RESUMED) resumed << ei_event_get_device(event);
            else if (type == EI_EVENT_KEYBOARD_KEY && ei_event_keyboard_get_key_is_press(event)) keys << ei_event_keyboard_get_key(event);
            ei_event_unref(event);
        }
        return true;
    }
    ei_device *device(ei_device_capability capability) const {
        for (auto *candidate : devices)
            if (resumed.contains(candidate) && ei_device_has_capability(candidate, capability)) return candidate;
        return nullptr;
    }
    void emulate(ei_device *device, const std::function<void()> &events) {
        if (!started.contains(device)) { ei_device_start_emulating(device, ++sequence); started << device; }
        events();
        ei_device_frame(device, ei_now(context));
    }
    bool disconnected() { pump(); return seen.contains(EI_EVENT_DISCONNECT) || seen.contains(EI_EVENT_DEVICE_REMOVED); }
    ei *context;
    bool ok = false;
    QList<ei_event_type> seen;
    QList<ei_device *> devices, resumed, started;
    QList<uint32_t> keys;
    uint32_t sequence = 0;
};
class Collector final : public QObject {
    Q_OBJECT
public:
    QHash<QString, QPair<quint32, QVariantMap>> responses;
    QList<QDBusMessage> received;
    QList<QDBusMessage> named(const QString &member) const {
        QList<QDBusMessage> result;
        for (const auto &message : received) if (message.member() == member) result << message;
        return result;
    }
public Q_SLOTS:
    void response(quint32 value, const QVariantMap &results, const QDBusMessage &message) { responses.insert(message.path(), {value, results}); }
    void any(const QDBusMessage &message) { received << message; }
};
QVariantMap options(const QDBusMessage &message, int index) { return qdbus_cast<QVariantMap>(message.arguments().value(index)); }
}
class NativeRemoteInputTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        qDBusRegisterMetaType<QList<QVariantMap>>();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Documents")));
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.impl.portal.PermissionStore")));
        backend = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("native-remote-input-backend")));
        const QString data = qEnvironmentVariable("XDG_DATA_HOME");
        QVERIFY(QDir().mkpath(data + QStringLiteral("/applications")));
        QFile app(data + QStringLiteral("/applications/org.test.RemoteInput.desktop")); QVERIFY(app.open(QIODevice::WriteOnly));
        app.write("[Desktop Entry]\nType=Application\nName=Synthetic remote input caller\nExec=/bin/true\n"); app.close();
        resident = std::make_unique<ResidentPortalService>(appearance, *backend);
        broker = std::make_unique<QindaQt::Services::SecretPortal::QtKeyringPortalBroker>(*backend);
        secret = std::make_unique<QindaQt::Services::SecretPortal::SecretPortalAdaptor>(resident->backendHost(), *broker, *backend);
        composition = std::make_unique<PortalFoundationComposition>(resident->backendHost(), *backend,
            qEnvironmentVariable("XDG_RUNTIME_DIR"), qEnvironmentVariable("QINDAQT_PORTAL_TEST_HELPER"), QString{}, QStringList{data});
        QCOMPARE(resident->start(), PortalServiceStartStatus::Started); QVERIFY(composition->start());
        sessionCaller = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
            qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("native-remote-input-session")));
        QVERIFY(sessionCaller->isConnected());
        // AGENT-GUARD: wait for the real ordinary session caller's public
        // attachment reply. Starting an asynchronous supervisor retry is not
        // evidence that consent has an admitted display/lock authority yet.
        auto attach = QDBusMessage::createMethodCall(QStringLiteral("org.qindaqt.Portal1"),
            QStringLiteral("/org/qindaqt/Portal1"), QStringLiteral("org.qindaqt.Portal1"), QStringLiteral("AttachSessionWithDisplay"));
        attach << QStringLiteral("qindaqt-7");
        auto pendingAttach = sessionCaller->asyncCall(attach, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(pendingAttach.isFinished(), 3000);
        const QDBusReply<bool> attached(pendingAttach.reply());
        QVERIFY2(attached.isValid(), qPrintable(attached.error().message()));
        QVERIFY2(attached.value(), "Actual compositor/session attachment was denied");
        QindaQt::Platform::Compositor::CompositorAttachment attachment(*sessionCaller,
            qEnvironmentVariable("XDG_RUNTIME_DIR"), [&](const QString &owner) { return owner == sessionCaller->baseService(); });
        QVERIFY(attachment.attach(sessionCaller->baseService(), QStringLiteral("qindaqt-7")));
        QindaQt::Services::SessionLockState::QtNativeLockTransport lockTransport(*sessionCaller);
        QindaQt::Services::SessionLockState::NativeLockStateMonitor lockMonitor(lockTransport,
            [&](const QString &owner, quint64 pid) {
                const auto identity = attachment.identity();
                return identity && identity->compositorOwner == owner && identity->compositorPid == pid;
            });
        QSignalSpy privacyFailures(&lockTransport, &QindaQt::Services::SessionLockState::NativeLockTransport::failed);
        QVERIFY(lockMonitor.start());
        QVERIFY2(QTest::qWaitFor([&] { return lockMonitor.contentMayBeShown(); }, 5000),
            privacyFailures.isEmpty() ? "Real native privacy did not admit attached compositor" : qPrintable(privacyFailures.last().last().toString()));
        frontend.start(QStringLiteral(QINDAQT_FRONTEND_EXECUTABLE), {QStringLiteral("--verbose")}); QVERIFY(frontend.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.portal.Desktop")).value(), 10000);
        QVERIFY(bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), {}, QStringLiteral("org.freedesktop.portal.Request"),
                            QStringLiteral("Response"), &events, SLOT(response(quint32,QVariantMap,QDBusMessage))));
        for (const auto &[interface, member] : std::initializer_list<std::pair<const char *, const char *>>{
                 {"org.freedesktop.portal.InputCapture", "Activated"}, {"org.freedesktop.portal.InputCapture", "Deactivated"},
                 {"org.freedesktop.portal.InputCapture", "Disabled"}, {"org.freedesktop.portal.Clipboard", "SelectionOwnerChanged"},
                 {"org.freedesktop.portal.Clipboard", "SelectionTransfer"}, {"org.freedesktop.portal.Session", "Closed"}})
            QVERIFY(bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), {}, QLatin1String(interface), QLatin1String(member),
                                &events, SLOT(any(QDBusMessage))));
        auto registration = call("org.freedesktop.host.portal.Registry", "Register", {QStringLiteral("org.test.RemoteInput"), QVariantMap{}});
        QVERIFY2(registration.type() == QDBusMessage::ReplyMessage, qPrintable(registration.errorMessage()));
    }
    void remoteDesktopCaptureAndClipboardJourney() {
        const QString rd = createSession("org.freedesktop.portal.RemoteDesktop", {}, "rd1");
        QVERIFY(!rd.isEmpty());
        QCOMPARE(call("org.freedesktop.portal.Clipboard", "RequestClipboard", {QVariant::fromValue(QDBusObjectPath(rd)), QVariantMap{}}).type(),
                 QDBusMessage::ReplyMessage);
        QCOMPARE(response("org.freedesktop.portal.RemoteDesktop", "SelectDevices", {QVariant::fromValue(QDBusObjectPath(rd)),
                 QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("rd1_select")}, {QStringLiteral("types"), 3U}}}, "rd1_select").first, 0U);
        qputenv("QINDAQT_PORTAL_TEST_MODE", "grant-choices");
        const auto started = response("org.freedesktop.portal.RemoteDesktop", "Start", {QVariant::fromValue(QDBusObjectPath(rd)), QString{},
                 QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("rd1_start")}}}, "rd1_start");
        QCOMPARE(started.first, 0U);
        QCOMPARE(started.second.value(QStringLiteral("devices")).toUInt(), 3U);
        QCOMPARE(started.second.value(QStringLiteral("clipboard_enabled")).toBool(), true);
        QVERIFY(audit().contains(" mapped grant-choices"));
        Ei sender(true, eisFd("org.freedesktop.portal.RemoteDesktop", rd));
        QVERIFY(sender.ok);
        QTRY_VERIFY_WITH_TIMEOUT(sender.pump() && sender.device(EI_DEVICE_CAP_KEYBOARD) && sender.device(EI_DEVICE_CAP_POINTER)
                                 && sender.device(EI_DEVICE_CAP_POINTER_ABSOLUTE), 10000);

        qputenv("QINDAQT_PORTAL_TEST_MODE", "grant");
        const QString ic = createSession("org.freedesktop.portal.InputCapture", {{QStringLiteral("capabilities"), 3U}}, "ic1", true);
        QVERIFY(!ic.isEmpty());
        const auto zones = response("org.freedesktop.portal.InputCapture", "GetZones", {QVariant::fromValue(QDBusObjectPath(ic)),
                 QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("ic1_zones")}}}, "ic1_zones");
        QCOMPARE(zones.first, 0U);
        const uint zoneSet = zones.second.value(QStringLiteral("zone_set")).toUInt();
        QDBusArgument position; position.beginStructure(); position << 0 << 0 << 0 << 759; position.endStructure();
        const QList<QVariantMap> barriers{{{QStringLiteral("barrier_id"), 1U}, {QStringLiteral("position"), QVariant::fromValue(position)}}};
        const auto set = response("org.freedesktop.portal.InputCapture", "SetPointerBarriers", {QVariant::fromValue(QDBusObjectPath(ic)),
                 QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("ic1_barriers")}}, QVariant::fromValue(barriers), zoneSet}, "ic1_barriers");
        QCOMPARE(set.first, 0U);
        QVERIFY(qdbus_cast<QList<uint>>(set.second.value(QStringLiteral("failed_barriers"))).isEmpty());
        Ei receiver(false, eisFd("org.freedesktop.portal.InputCapture", ic));
        QVERIFY(receiver.ok);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.pump() && !receiver.devices.isEmpty(), 10000);
        QCOMPARE(call("org.freedesktop.portal.InputCapture", "Enable", {QVariant::fromValue(QDBusObjectPath(ic)), QVariantMap{}}).type(),
                 QDBusMessage::ReplyMessage);
        // Remote pointer input crosses the consented left-edge barrier.
        auto *absolute = sender.device(EI_DEVICE_CAP_POINTER_ABSOLUTE);
        sender.emulate(absolute, [&] { ei_device_pointer_motion_absolute(absolute, 0, 300); });
        auto *relative = sender.device(EI_DEVICE_CAP_POINTER);
        QTRY_VERIFY_WITH_TIMEOUT((sender.emulate(relative, [&] { ei_device_pointer_motion(relative, -12, 0); }), receiver.pump(),
                                  !events.named(QStringLiteral("Activated")).isEmpty()), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.pump() && receiver.seen.contains(EI_EVENT_DEVICE_START_EMULATING), 5000);
        // Remote keyboard input is delivered to the capturing client.
        auto *keyboard = sender.device(EI_DEVICE_CAP_KEYBOARD);
        sender.emulate(keyboard, [&] { ei_device_keyboard_key(keyboard, KEY_Q, true); });
        sender.emulate(keyboard, [&] { ei_device_keyboard_key(keyboard, KEY_Q, false); });
        QTRY_VERIFY_WITH_TIMEOUT(receiver.pump() && receiver.keys.contains(KEY_Q), 5000);
        const uint activation = options(events.named(QStringLiteral("Activated")).constLast(), 1).value(QStringLiteral("activation_id")).toUInt();
        QCOMPARE(call("org.freedesktop.portal.InputCapture", "Release", {QVariant::fromValue(QDBusObjectPath(ic)),
                 QVariantMap{{QStringLiteral("activation_id"), activation}, {QStringLiteral("cursor_position"), QVariant::fromValue(QPointF(500, 380))}}}).type(),
                 QDBusMessage::ReplyMessage);
        QTRY_VERIFY_WITH_TIMEOUT(!events.named(QStringLiteral("Deactivated")).isEmpty(), 5000);

        // Remote selection pasted by an ordinary Wayland client.
        QCOMPARE(call("org.freedesktop.portal.Clipboard", "SetSelection", {QVariant::fromValue(QDBusObjectPath(rd)),
                 QVariantMap{{QStringLiteral("mime_types"), QStringList{QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("text/plain")}}}}).type(),
                 QDBusMessage::ReplyMessage);
        QProcess paste; paste.setProcessEnvironment(clientEnvironment());
        paste.start(QStringLiteral(QINDAQT_CLIPBOARD_CLIENT), {QStringLiteral("paste")}); QVERIFY(paste.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(!events.named(QStringLiteral("SelectionTransfer")).isEmpty(), 10000);
        const auto transfer = events.named(QStringLiteral("SelectionTransfer")).constFirst();
        {
            // Close the last local descriptor before claiming completion; a
            // temporary copy alone leaves the reply holding the pipe open.
            const QDBusReply<QDBusUnixFileDescriptor> writer = call("org.freedesktop.portal.Clipboard", "SelectionWrite",
                {QVariant::fromValue(QDBusObjectPath(rd)), transfer.arguments().value(2).toUInt()});
            QVERIFY2(writer.isValid(), qPrintable(writer.error().message()));
            QCOMPARE(::write(writer.value().fileDescriptor(), "remote payload", 14), ssize_t(14));
        }
        QCOMPARE(call("org.freedesktop.portal.Clipboard", "SelectionWriteDone", {QVariant::fromValue(QDBusObjectPath(rd)),
                 transfer.arguments().value(2).toUInt(), true}).type(), QDBusMessage::ReplyMessage);
        QTRY_VERIFY_WITH_TIMEOUT(paste.state() == QProcess::NotRunning, 10000);
        QCOMPARE(paste.readAllStandardOutput().trimmed(), QByteArray("PASTED remote payload"));

        // Local selection read by the remote session.
        QProcess copy; copy.setProcessEnvironment(clientEnvironment());
        copy.start(QStringLiteral(QINDAQT_CLIPBOARD_CLIENT), {QStringLiteral("copy"), QStringLiteral("local payload")}); QVERIFY(copy.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(copy.readAllStandardOutput().contains("COPIED"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            for (const auto &owner : events.named(QStringLiteral("SelectionOwnerChanged")))
                if (options(owner, 1).value(QStringLiteral("mime_types")).toStringList().contains(QStringLiteral("text/plain;charset=utf-8"))
                    && !options(owner, 1).value(QStringLiteral("session_is_owner")).toBool()) return true;
            return false;
        }(), 10000);
        const QDBusReply<QDBusUnixFileDescriptor> reader = call("org.freedesktop.portal.Clipboard", "SelectionRead",
            {QVariant::fromValue(QDBusObjectPath(rd)), QStringLiteral("text/plain;charset=utf-8")});
        QVERIFY2(reader.isValid(), qPrintable(reader.error().message()));
        const int readFd = reader.value().fileDescriptor();
        QVERIFY(fcntl(readFd, F_SETFL, fcntl(readFd, F_GETFL) | O_NONBLOCK) == 0);
        QByteArray copied; char buffer[64];
        QTRY_VERIFY_WITH_TIMEOUT([&] { const auto n = ::read(readFd, buffer, sizeof buffer); if (n > 0) copied.append(buffer, n); return n == 0; }(), 5000);
        QCOMPARE(copied, QByteArray("local payload"));
        copy.terminate(); copy.waitForFinished(3000);

        // Close ends both compositor transports.
        QCOMPARE(closeSession(ic).type(), QDBusMessage::ReplyMessage);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.disconnected(), 5000);
        QCOMPARE(closeSession(rd).type(), QDBusMessage::ReplyMessage);
        QTRY_VERIFY_WITH_TIMEOUT(sender.disconnected(), 5000);
    }
    void nativeLockEndsRemoteDesktop() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "grant");
        events.received.clear();
        const QString rd = createSession("org.freedesktop.portal.RemoteDesktop", {}, "rd2");
        QCOMPARE(response("org.freedesktop.portal.RemoteDesktop", "SelectDevices", {QVariant::fromValue(QDBusObjectPath(rd)),
                 QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("rd2_select")}, {QStringLiteral("types"), 1U}}}, "rd2_select").first, 0U);
        QCOMPARE(response("org.freedesktop.portal.RemoteDesktop", "Start", {QVariant::fromValue(QDBusObjectPath(rd)), QString{},
                 QVariantMap{{QStringLiteral("handle_token"), QStringLiteral("rd2_start")}}}, "rd2_start").first, 0U);
        Ei sender(true, eisFd("org.freedesktop.portal.RemoteDesktop", rd));
        QTRY_VERIFY_WITH_TIMEOUT(sender.pump() && sender.device(EI_DEVICE_CAP_KEYBOARD), 10000);
        auto lock = QDBusMessage::createMethodCall(QString(QindaQt::CompositorNames::service), QString(QindaQt::CompositorNames::nativeLockPath),
            QString(QindaQt::CompositorNames::nativeLockInterface), QStringLiteral("RequestLockWithReceipt"));
        lock << QUuid::createUuid().toString(QUuid::Id128);
        bus.asyncCall(lock);
        QTRY_VERIFY_WITH_TIMEOUT(sender.disconnected(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!events.named(QStringLiteral("Closed")).isEmpty(), 10000);
        QVERIFY(eisFdError("org.freedesktop.portal.RemoteDesktop", rd));
    }
    void cleanupTestCase() {
        frontend.terminate(); if (!frontend.waitForFinished(3000)) { frontend.kill(); frontend.waitForFinished(3000); }
        sessionCaller.reset(); QDBusConnection::disconnectFromBus(QStringLiteral("native-remote-input-session")); composition.reset(); secret.reset(); broker.reset(); resident.reset();
        backend.reset(); QDBusConnection::disconnectFromBus(QStringLiteral("native-remote-input-backend"));
    }
private:
    QDBusMessage call(const char *interface, const char *member, const QVariantList &arguments) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"),
                                                      QString::fromLatin1(interface), QString::fromLatin1(member));
        message.setArguments(arguments);
        return bus.call(message, QDBus::BlockWithGui, 20000);
    }
    QPair<quint32, QVariantMap> response(const char *interface, const char *member, const QVariantList &arguments, const char *token) {
        const auto reply = call(interface, member, arguments);
        if (reply.type() != QDBusMessage::ReplyMessage) { qWarning().noquote() << member << reply.errorMessage(); return {99U, {}}; }
        const QString path = qdbus_cast<QDBusObjectPath>(reply.arguments().value(0)).path();
        Q_UNUSED(token);
        if (!QTest::qWaitFor([&] { return events.responses.contains(path); }, 20000)) return {98U, {}};
        return events.responses.take(path);
    }
    QString createSession(const char *interface, QVariantMap extra, const char *token, bool parent = false) {
        extra.insert(QStringLiteral("handle_token"), QString::fromLatin1(token));
        extra.insert(QStringLiteral("session_handle_token"), QString::fromLatin1(token) + QStringLiteral("_session"));
        const auto created = parent ? response(interface, "CreateSession", {QString{}, extra}, token) : response(interface, "CreateSession", {extra}, token);
        if (created.first != 0U) {
            qInfo() << "CreateSession response" << interface << created.first << created.second;
            return {};
        }
        const auto handle = created.second.value(QStringLiteral("session_handle"));
        if (handle.metaType() == QMetaType::fromType<QDBusObjectPath>()) return handle.value<QDBusObjectPath>().path();
        if (handle.metaType() == QMetaType::fromType<QDBusArgument>()) return qdbus_cast<QDBusObjectPath>(handle).path();
        return handle.toString();
    }
    int eisFd(const char *interface, const QString &session) {
        const QDBusReply<QDBusUnixFileDescriptor> reply = call(interface, "ConnectToEIS", {QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{}});
        if (!reply.isValid()) { qWarning().noquote() << interface << reply.error().message(); return -1; }
        return ::dup(reply.value().fileDescriptor());
    }
    bool eisFdError(const char *interface, const QString &session) {
        return call(interface, "ConnectToEIS", {QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{}}).type() == QDBusMessage::ErrorMessage;
    }
    QDBusMessage closeSession(const QString &path) {
        return bus.call(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                                                       QStringLiteral("org.freedesktop.portal.Session"), QStringLiteral("Close")), QDBus::BlockWithGui);
    }
    QProcessEnvironment clientEnvironment() const {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("QT_FATAL_WARNINGS"));
        return environment;
    }
    QByteArray audit() const {
        QFile file(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT"));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    std::unique_ptr<QDBusConnection> backend;
    EmptyAppearance appearance;
    std::unique_ptr<ResidentPortalService> resident;
    std::unique_ptr<QindaQt::Services::SecretPortal::QtKeyringPortalBroker> broker;
    std::unique_ptr<QindaQt::Services::SecretPortal::SecretPortalAdaptor> secret;
    std::unique_ptr<PortalFoundationComposition> composition;
    std::unique_ptr<QDBusConnection> sessionCaller;
    QProcess frontend;
    Collector events;
};
QTEST_GUILESS_MAIN(NativeRemoteInputTest)
#include "tst_native_remote_input.moc"
