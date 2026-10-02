// SPDX-License-Identifier: GPL-3.0-or-later
// Private-bus RemoteDesktop backend lifetime. A synthetic compositor object
// stands in for the fork's org.kde.KWin.EIS.RemoteDesktop; actual EIS input
// through the private native compositor is a separate manager gate.
#include "remote_input_fixture.h"
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include <QDBusContext>
#include <QDBusReply>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::Portal::RemoteInput;
class Compositor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.RemoteDesktop")
public:
    ~Compositor() override { for (const int fd : std::as_const(peers)) ::close(fd); }
    QDBusUnixFileDescriptor transport(int &cookie) {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0) return {};
        peers << pair[0]; cookie = ++next;
        const QDBusUnixFileDescriptor fd(pair[1]); ::close(pair[1]);
        return fd;
    }
    QList<int> capabilities, disconnected, peers, clipboardsOpened, clipboardsClosed;
    QList<QStringList> selections;
    QStringList callers;
    QString readMime;
    QDBusMessage held;
    bool delay = false;
    int next = 0;
public Q_SLOTS:
    int connectClipboard() { const int handle = int(clipboardsOpened.size()) + 1; clipboardsOpened << handle; return handle; }
    void disconnectClipboard(int handle) { clipboardsClosed << handle; }
    void setSelection(int, const QStringList &mimeTypes) { selections << mimeTypes; }
    QDBusUnixFileDescriptor readSelection(int, const QString &mimeType) {
        readMime = mimeType;
        int pipe[2];
        if (pipe2(pipe, O_CLOEXEC) != 0 || ::write(pipe[1], "world", 5) != 5) return {};
        ::close(pipe[1]);
        const QDBusUnixFileDescriptor fd(pipe[0]); ::close(pipe[0]);
        return fd;
    }
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
    QList<QDBusMessage> clipboard;
public Q_SLOTS:
    void closed() { ++count; }
    void received(const QDBusMessage &message) { clipboard << message; }
};
class RemoteDesktopTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        QVERIFY(bus.start());
        compositor = std::make_unique<Compositor>();
        QVERIFY(bus.compositor->registerObject(QStringLiteral("/org/kde/KWin/EIS/RemoteDesktop"), compositor.get(),
                                               QDBusConnection::ExportAllSlots));
        selectedCompositor = bus.compositor->baseService();
        host = std::make_unique<QObject>(); registry = std::make_unique<RequestRegistry>(*bus.service);
        consent = std::make_unique<FakeConsent>();
        eis = std::make_unique<CompositorEis>(*bus.service, [this] { return selectedCompositor; });
        new RemoteDesktopAdaptor(*host, *registry, *consent, *eis, *bus.service);
        QVERIFY(bus.service->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(bus.service->registerService(QStringLiteral("org.test.Portal")));
        spy = std::make_unique<ClosedSpy>();
        QVERIFY(bus.client->connect(QString{}, bus.session, QStringLiteral("org.freedesktop.impl.portal.Session"),
                                    QStringLiteral("Closed"), spy.get(), SLOT(closed())));
        for (const auto *member : {"SelectionOwnerChanged", "SelectionTransfer"})
            QVERIFY(bus.client->connect(QString{}, QStringLiteral("/org/freedesktop/portal/desktop"),
                QStringLiteral("org.freedesktop.impl.portal.Clipboard"), QLatin1String(member), spy.get(), SLOT(received(QDBusMessage))));
    }
    void cleanup() {
        host.reset(); eis.reset(); registry.reset(); consent.reset(); compositor.reset(); spy.reset();
        bus.stop();
    }
    void grantConnectAndClose() {
        QVERIFY(started(Keyboard | Pointer));
        QCOMPARE(consent->question.appId, QStringLiteral("org.test.App"));
        QVERIFY(consent->question.subtitle.contains(QStringLiteral("keyboard, pointer")));
        QCOMPARE(bus.results.value(QStringLiteral("devices")).toUInt(), quint32(Keyboard | Pointer));
        QCOMPARE(bus.results.value(QStringLiteral("clipboard_enabled")).toBool(), false);
        const QDBusPendingReply<QDBusUnixFileDescriptor> fd = connectToEis();
        QVERIFY2(!fd.isError(), qPrintable(fd.error().message())); QVERIFY(fd.value().isValid());
        QCOMPARE(compositor->capabilities, QList<int>{int(Keyboard | Pointer)});
        QCOMPARE(compositor->callers, QStringList{bus.service->baseService()});
        QVERIFY(connectToEis().isError()); // One transport per granted session.
        QCOMPARE(compositor->capabilities.size(), 1);
        QCOMPARE(bus.sessionCall(*bus.stranger, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
        QVERIFY(compositor->disconnected.isEmpty());
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(compositor->disconnected, QList<int>{1});
        QCOMPARE(spy->count, 0); // Frontend-initiated Close needs no Closed signal.
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void denialAndCancelCloseSession() {
        QVERIFY(selected(Pointer));
        auto start = remote(QStringLiteral("Start"), {bus.request(3), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        QTRY_COMPARE(consent->asks, 1);
        Q_EMIT consent->completed(consent->token, RequestResponse::Cancelled, ChoiceValues{});
        QCOMPARE(bus.response(start), 1U);
        QTRY_COMPARE(spy->count, 1);
        QVERIFY(connectToEis().isError());
        QVERIFY(compositor->capabilities.isEmpty());
        // Request.Close while consent is visible cancels the helper and session.
        bus.session.replace(QLatin1String("/s1"), QLatin1String("/s2"));
        QVERIFY(selected(Pointer));
        start = remote(QStringLiteral("Start"), {bus.request(6), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        QTRY_COMPARE(consent->asks, 2);
        auto close = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), bus.requestPrefix + QLatin1Char('6'),
            QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QCOMPARE(bus.client->call(close, QDBus::BlockWithGui).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(bus.response(start), 1U); QCOMPARE(consent->cancels, 2);
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void refusesUnstartedInvalidAndForeignCalls() {
        auto create = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("CreateSession"));
        create.setArguments({bus.request(1), bus.sessionHandle(), app, QVariantMap{}});
        QCOMPARE(bus.stranger->call(create, QDBus::BlockWithGui).type(), QDBusMessage::ErrorMessage);
        QCOMPARE(bus.response(remote(QStringLiteral("CreateSession"), {bus.request(1), bus.sessionHandle(), app, QVariantMap{}})), 0U);
        QVERIFY(connectToEis().isError()); // Not granted yet.
        QCOMPARE(bus.response(remote(QStringLiteral("SelectDevices"), {bus.request(2), bus.sessionHandle(), app,
                                                                      QVariantMap{{QStringLiteral("types"), 8U}}})), 2U);
        QCOMPARE(bus.response(remote(QStringLiteral("SelectDevices"), {bus.request(3), bus.sessionHandle(), QStringLiteral("org.test.Other"),
                                                                      QVariantMap{{QStringLiteral("types"), 1U}}})), 2U);
        auto notify = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("NotifyPointerMotion"));
        notify.setArguments({bus.sessionHandle(), QVariantMap{}, 1.0, 1.0});
        // Unstarted sessions refuse legacy input before any EIS context opens.
        QCOMPARE(bus.client->call(notify, QDBus::BlockWithGui).errorName(), QStringLiteral("org.freedesktop.portal.Error.NotAllowed"));
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
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void frontendOwnerLossEndsTransport() {
        QVERIFY(started(Touchscreen));
        QVERIFY(!connectToEis().isError());
        QVERIFY(bus.client->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_COMPARE(compositor->disconnected, QList<int>{1});
    }
    void lateTransportAfterCloseIsDisconnected() {
        QVERIFY(started(Pointer));
        compositor->delay = true;
        auto pending = asyncConnect();
        QTRY_VERIFY(compositor->held.type() == QDBusMessage::MethodCallMessage);
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QVERIFY(PrivatePortalBus::wait(pending)); QVERIFY(pending.isError());
        int cookie = 0; const auto fd = compositor->transport(cookie);
        QVERIFY(bus.compositor->send(compositor->held.createReply({QVariant::fromValue(fd), cookie})));
        QTRY_COMPARE(compositor->disconnected, QList<int>{cookie});
    }
    void clipboardGrantedTransfersAndReads() {
        QVERIFY(clipboardStarted(QStringLiteral("true")));
        QCOMPARE(consent->question.choices.size(), 1);
        QCOMPARE(consent->question.choices.front().id, QStringLiteral("clipboard"));
        QCOMPARE(consent->question.choices.front().initial, QStringLiteral("false"));
        QCOMPARE(bus.results.value(QStringLiteral("clipboard_enabled")).toBool(), true);
        QCOMPARE(compositor->clipboardsOpened, QList<int>{1});
        compositorSignal(QStringLiteral("selectionChanged"), {1, QStringList{QStringLiteral("text/plain")}, false});
        QTRY_COMPARE(spy->clipboard.size(), 1);
        QCOMPARE(spy->clipboard.front().member(), QStringLiteral("SelectionOwnerChanged"));
        const auto owner = qdbus_cast<QVariantMap>(spy->clipboard.front().arguments().at(1));
        QCOMPARE(owner.value(QStringLiteral("mime_types")).toStringList(), QStringList{QStringLiteral("text/plain")});
        QCOMPARE(owner.value(QStringLiteral("session_is_owner")).toBool(), false);
        QCOMPARE(clipboard(QStringLiteral("SetSelection"), {bus.sessionHandle(),
            QVariantMap{{QStringLiteral("mime_types"), QStringList{QStringLiteral("text/plain")}}}}).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(compositor->selections, QList<QStringList>{QStringList{QStringLiteral("text/plain")}});
        int paste[2];
        QCOMPARE(pipe2(paste, O_CLOEXEC), 0);
        compositorSignal(QStringLiteral("selectionTransfer"), {1, QStringLiteral("text/plain"),
                                                              QVariant::fromValue(QDBusUnixFileDescriptor(paste[1]))});
        ::close(paste[1]);
        QTRY_COMPARE(spy->clipboard.size(), 2);
        QCOMPARE(spy->clipboard.at(1).member(), QStringLiteral("SelectionTransfer"));
        QCOMPARE(spy->clipboard.at(1).arguments().at(2).toUInt(), 1U);
        {
            const QDBusReply<QDBusUnixFileDescriptor> writer = clipboard(QStringLiteral("SelectionWrite"), {bus.sessionHandle(), 1U});
            QVERIFY(writer.isValid());
            QCOMPARE(::write(writer.value().fileDescriptor(), "hello", 5), ssize_t(5));
        }
        char pasted[8] = {};
        QCOMPARE(::read(paste[0], pasted, sizeof pasted), ssize_t(5));
        QCOMPARE(QByteArray(pasted), QByteArray("hello"));
        ::close(paste[0]);
        QCOMPARE(clipboard(QStringLiteral("SelectionWrite"), {bus.sessionHandle(), 1U}).type(), QDBusMessage::ErrorMessage);
        QCOMPARE(clipboard(QStringLiteral("SelectionWriteDone"), {bus.sessionHandle(), 1U, true}).type(), QDBusMessage::ReplyMessage);
        const QDBusReply<QDBusUnixFileDescriptor> reader = clipboard(QStringLiteral("SelectionRead"), {bus.sessionHandle(), QStringLiteral("text/plain")});
        QVERIFY(reader.isValid());
        char copied[8] = {};
        QCOMPARE(::read(reader.value().fileDescriptor(), copied, sizeof copied), ssize_t(5));
        QCOMPARE(QByteArray(copied), QByteArray("world"));
        QCOMPARE(compositor->readMime, QStringLiteral("text/plain"));
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(compositor->clipboardsClosed, QList<int>{1});
    }
    void clipboardNeedsExplicitChoice() {
        QVERIFY(clipboardStarted(QStringLiteral("false")));
        QCOMPARE(bus.results.value(QStringLiteral("clipboard_enabled")).toBool(), false);
        QVERIFY(compositor->clipboardsOpened.isEmpty());
        QCOMPARE(clipboard(QStringLiteral("SetSelection"), {bus.sessionHandle(),
            QVariantMap{{QStringLiteral("mime_types"), QStringList{QStringLiteral("text/plain")}}}}).type(), QDBusMessage::ErrorMessage);
        QCOMPARE(clipboard(QStringLiteral("SelectionRead"), {bus.sessionHandle(), QStringLiteral("text/plain")}).type(),
                 QDBusMessage::ErrorMessage);
        // RequestClipboard after Start cannot add clipboard to a granted session.
        QCOMPARE(clipboard(QStringLiteral("RequestClipboard"), {bus.sessionHandle(), QVariantMap{}}).type(), QDBusMessage::ErrorMessage);
        QVERIFY(compositor->selections.isEmpty());
    }
    void clipboardEndsWithNativeAuthority() {
        QVERIFY(clipboardStarted(QStringLiteral("true")));
        consent->allowed = false; Q_EMIT consent->authorityLost();
        QTRY_COMPARE(compositor->clipboardsClosed, QList<int>{1});
        int paste[2];
        QCOMPARE(pipe2(paste, O_CLOEXEC), 0);
        compositorSignal(QStringLiteral("selectionTransfer"), {1, QStringLiteral("text/plain"),
                                                              QVariant::fromValue(QDBusUnixFileDescriptor(paste[1]))});
        ::close(paste[1]);
        QTest::qWait(200);
        QVERIFY(spy->clipboard.isEmpty()); // A retired handle forwards nothing.
        char byte = 0;
        QCOMPARE(::read(paste[0], &byte, 1), ssize_t(0)); // Requester sees EOF.
        ::close(paste[0]);
    }
    void replacedCompositorGetsNoTransport() {
        QVERIFY(started(Keyboard));
        selectedCompositor.clear();
        QVERIFY(connectToEis().isError());
        QVERIFY(compositor->capabilities.isEmpty());
    }
private:
    QDBusPendingCall remote(const QString &member, const QVariantList &arguments) {
        return bus.call(QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), member, arguments);
    }
    bool selected(quint32 devices) {
        return bus.response(remote(QStringLiteral("CreateSession"), {bus.request(1), bus.sessionHandle(), app, QVariantMap{}})) == 0U
            && bus.response(remote(QStringLiteral("SelectDevices"), {bus.request(2), bus.sessionHandle(), app,
                                                                    QVariantMap{{QStringLiteral("types"), devices}}})) == 0U;
    }
    bool started(quint32 devices) {
        if (!selected(devices)) return false;
        auto start = remote(QStringLiteral("Start"), {bus.request(3), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        if (!QTest::qWaitFor([this] { return consent->asks == 1; })) return false;
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, ChoiceValues{});
        return bus.response(start) == 0U;
    }
    bool clipboardStarted(const QString &choice) {
        if (!selected(Keyboard)) return false;
        if (clipboard(QStringLiteral("RequestClipboard"), {bus.sessionHandle(), QVariantMap{}}).type() != QDBusMessage::ReplyMessage)
            return false;
        auto start = remote(QStringLiteral("Start"), {bus.request(3), bus.sessionHandle(), app, QString{}, QVariantMap{}});
        if (!QTest::qWaitFor([this] { return consent->asks == 1; })) return false;
        Q_EMIT consent->completed(consent->token, RequestResponse::Success,
                                  ChoiceValues{{QStringLiteral("clipboard"), choice}});
        return bus.response(start) == 0U;
    }
    QDBusMessage clipboard(const QString &member, const QVariantList &arguments) {
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
                                                   QStringLiteral("org.freedesktop.impl.portal.Clipboard"), member);
        call.setArguments(arguments);
        return bus.client->call(call, QDBus::BlockWithGui);
    }
    void compositorSignal(const QString &member, const QVariantList &arguments) {
        auto signal = QDBusMessage::createTargetedSignal(bus.service->baseService(), QStringLiteral("/org/kde/KWin/EIS/RemoteDesktop"),
                                                         QStringLiteral("org.kde.KWin.EIS.RemoteDesktop"), member);
        signal.setArguments(arguments);
        QVERIFY(bus.compositor->send(signal));
    }
    QDBusPendingCall asyncConnect() {
        return remote(QStringLiteral("ConnectToEIS"), {bus.sessionHandle(), app, QVariantMap{}});
    }
    QDBusPendingReply<QDBusUnixFileDescriptor> connectToEis() {
        auto call = asyncConnect();
        PrivatePortalBus::wait(call);
        return call;
    }
    uint property(const QString &name) {
        auto get = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        get.setArguments({QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), name});
        const QDBusReply<QDBusVariant> reply = bus.client->call(get, QDBus::BlockWithGui);
        return reply.isValid() ? reply.value().variant().toUInt() : 0U;
    }
    const QString app = QStringLiteral("org.test.App");
    PrivatePortalBus bus;
    QString selectedCompositor;
    std::unique_ptr<QObject> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<FakeConsent> consent;
    std::unique_ptr<CompositorEis> eis;
    std::unique_ptr<Compositor> compositor;
    std::unique_ptr<ClosedSpy> spy;
};
QTEST_GUILESS_MAIN(RemoteDesktopTest)
#include "tst_remote_desktop.moc"
