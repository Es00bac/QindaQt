// SPDX-License-Identifier: GPL-3.0-or-later
// Private-bus InputCapture backend lifetime against a synthetic compositor
// InputCaptureManager/InputCapture pair shaped like the fork EIS plugin.
#include "remote_input_fixture.h"
#include <qindaqt/services/portal/remote_input/input_capture_adaptor.h>
#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusMetaType>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::Portal::RemoteInput;
using Barrier = QPair<QPoint, QPoint>;
class CaptureObject final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.InputCapture")
public:
    ~CaptureObject() override { for (const int fd : std::as_const(peers)) ::close(fd); }
    QList<Barrier> barriers;
    QList<int> peers;
    QPointF released;
    bool applied = false;
    int enables = 0, disables = 0, releases = 0;
public Q_SLOTS:
    QDBusUnixFileDescriptor connectToEIS() {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0) return {};
        peers << pair[0];
        const QDBusUnixFileDescriptor fd(pair[1]); ::close(pair[1]);
        return fd;
    }
    void enable(const QList<Barrier> &requested) { barriers = requested; ++enables; }
    void disable() { ++disables; Q_EMIT disabled(); }
    void release(const QPointF &position, bool apply) { released = position; applied = apply; ++releases; }
Q_SIGNALS:
    void disabled();
    void activated(uint activationId, const QPointF &cursorPosition);
    void deactivated(uint activationId);
};
class Manager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.InputCaptureManager")
public:
    explicit Manager(QDBusConnection &connection) : bus(connection) {}
    QDBusConnection &bus;
    std::vector<std::unique_ptr<CaptureObject>> captures;
    QList<uint> added;
    QStringList removed;
    QList<QRect> layout{{0, 0, 1920, 1080}, {1920, 0, 1280, 1024}};
public Q_SLOTS:
    QDBusObjectPath addInputCapture(uint capabilities) {
        added << capabilities;
        const QString path = QStringLiteral("/org/kde/KWin/EIS/InputCapture/%1").arg(captures.size() + 1);
        captures.push_back(std::make_unique<CaptureObject>());
        bus.registerObject(path, captures.back().get(), QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
        return QDBusObjectPath(path);
    }
    void removeInputCapture(const QDBusObjectPath &capture) { removed << capture.path(); bus.unregisterObject(capture.path()); }
    QList<QRect> zones() { return layout; }
Q_SIGNALS:
    void zonesChanged();
};
class SignalSpy final : public QObject {
    Q_OBJECT
public:
    QList<QDBusMessage> messages;
    QStringList members() const { QStringList names; for (const auto &m : messages) names << m.member(); return names; }
public Q_SLOTS:
    void received(const QDBusMessage &message) { messages << message; }
};
class InputCaptureTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        qDBusRegisterMetaType<Barrier>();
        qDBusRegisterMetaType<QList<Barrier>>();
        qDBusRegisterMetaType<QList<QRect>>();
        qDBusRegisterMetaType<QList<QVariantMap>>();
    }
    void init() {
        QVERIFY(bus.start());
        manager = std::make_unique<Manager>(*bus.compositor);
        QVERIFY(bus.compositor->registerObject(QStringLiteral("/org/kde/KWin/EIS/InputCapture"), manager.get(),
                                               QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        selectedCompositor = bus.compositor->baseService();
        host = std::make_unique<QObject>(); registry = std::make_unique<RequestRegistry>(*bus.service);
        consent = std::make_unique<FakeConsent>();
        eis = std::make_unique<CompositorEis>(*bus.service, [this] { return selectedCompositor; });
        new InputCaptureAdaptor(*host, *registry, *consent, *eis, *bus.service);
        QVERIFY(bus.service->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(bus.service->registerService(QStringLiteral("org.test.Portal")));
        spy = std::make_unique<SignalSpy>();
        for (const auto *member : {"Disabled", "Activated", "Deactivated", "ZonesChanged"})
            QVERIFY(bus.client->connect(QString{}, QStringLiteral("/org/freedesktop/portal/desktop"),
                QStringLiteral("org.freedesktop.impl.portal.InputCapture"), QLatin1String(member), spy.get(), SLOT(received(QDBusMessage))));
    }
    void cleanup() {
        host.reset(); eis.reset(); registry.reset(); consent.reset(); manager.reset(); spy.reset();
        bus.stop();
    }
    void grantZonesBarriersEnableActivateRelease() {
        QVERIFY(created(Pointer));
        QCOMPARE(bus.results.value(QStringLiteral("capabilities")).toUInt(), quint32(Pointer));
        QCOMPARE(manager->added, QList<uint>{Pointer});
        QVERIFY(consent->question.subtitle.contains(QStringLiteral("pointer")));
        QCOMPARE(zones(2), 0U);
        QCOMPARE(bus.results.value(QStringLiteral("zone_set")).toUInt(), 1U);
        QCOMPARE(barriers(3, 1), 0U);
        const auto failed = qdbus_cast<QList<uint>>(bus.results.value(QStringLiteral("failed_barriers")));
        QCOMPARE(failed, (QList<uint>{2, 3}));
        const QDBusPendingReply<QDBusUnixFileDescriptor> fd = capture(QStringLiteral("ConnectToEIS"), {});
        QVERIFY2(!fd.isError(), qPrintable(fd.error().message())); QVERIFY(fd.value().isValid());
        QVERIFY(capture(QStringLiteral("ConnectToEIS"), {}).isError()); // One receiver per capture.
        QCOMPARE(direct(QStringLiteral("Enable")), 0U);
        auto &object = *manager->captures.front();
        QCOMPARE(object.enables, 1);
        QCOMPARE(object.barriers, (QList<Barrier>{{{0, 0}, {0, 1079}}, {{3199, 0}, {3199, 1023}}}));
        Q_EMIT object.activated(7, QPointF(0, 500));
        QTRY_COMPARE(spy->members(), QStringList{QStringLiteral("Activated")});
        const auto activated = spy->messages.front();
        QCOMPARE(activated.arguments().at(0).value<QDBusObjectPath>().path(), bus.session);
        const auto options = qdbus_cast<QVariantMap>(activated.arguments().at(1));
        QCOMPARE(options.value(QStringLiteral("activation_id")).toUInt(), 7U);
        QCOMPARE(qdbus_cast<QPointF>(options.value(QStringLiteral("cursor_position"))), QPointF(0, 500));
        QCOMPARE(direct(QStringLiteral("Release"), {{QStringLiteral("activation_id"), 7U},
                                                     {QStringLiteral("cursor_position"), QVariant::fromValue(QPointF(5, 5))}}), 0U);
        QCOMPARE(object.releases, 1); QCOMPARE(object.released, QPointF(5, 5)); QVERIFY(object.applied);
        Q_EMIT object.deactivated(7);
        QTRY_COMPARE(spy->messages.size(), 2);
        QCOMPARE(spy->messages.at(1).member(), QStringLiteral("Deactivated"));
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(manager->removed, QStringList{QStringLiteral("/org/kde/KWin/EIS/InputCapture/1")});
    }
    void denialArmsNothing() {
        auto create = createCall(Keyboard | Pointer);
        QTRY_COMPARE(consent->asks, 1);
        Q_EMIT consent->completed(consent->token, RequestResponse::Cancelled, ChoiceValues{});
        QCOMPARE(bus.response(create), 1U);
        QVERIFY(manager->added.isEmpty());
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
        QCOMPARE(bus.response(bus.call(interface, QStringLiteral("CreateSession"), {bus.request(2), bus.sessionHandle(), app, QString{},
            QVariantMap{{QStringLiteral("capabilities"), 8U}}})), 2U);
    }
    void zoneChangeDisablesAndInvalidatesBarriers() {
        QVERIFY(created(Pointer));
        QCOMPARE(zones(2), 0U);
        QCOMPARE(barriers(3, 1), 0U);
        QCOMPARE(direct(QStringLiteral("Enable")), 0U);
        manager->layout = {{0, 0, 2560, 1440}};
        Q_EMIT manager->zonesChanged();
        QTRY_VERIFY(spy->members().contains(QStringLiteral("ZonesChanged")));
        QTRY_COMPARE(manager->captures.front()->disables, 1);
        QCOMPARE(barriers(4, 1), 2U); // Stale zone set and cleared zones.
        QCOMPARE(zones(5), 0U);
        QCOMPARE(bus.results.value(QStringLiteral("zone_set")).toUInt(), 2U);
        QCOMPARE(direct(QStringLiteral("Enable")), 2U); // No barriers survive a layout change.
    }
    void nativeAuthorityLossRemovesCapture() {
        QVERIFY(created(Keyboard));
        consent->allowed = false; Q_EMIT consent->authorityLost();
        QTRY_COMPARE(manager->removed, QStringList{QStringLiteral("/org/kde/KWin/EIS/InputCapture/1")});
        QCOMPARE(bus.sessionCall(*bus.client, QStringLiteral("Close")).type(), QDBusMessage::ErrorMessage);
    }
    void forgedCompositorSignalIgnored() {
        QVERIFY(created(Pointer));
        auto forged = QDBusMessage::createSignal(QStringLiteral("/org/kde/KWin/EIS/InputCapture/1"),
                                                 QStringLiteral("org.kde.KWin.EIS.InputCapture"), QStringLiteral("activated"));
        forged << 9U << QVariant::fromValue(QPointF(1, 1));
        QVERIFY(bus.stranger->send(forged));
        QTest::qWait(200);
        QVERIFY(spy->messages.isEmpty());
        QCOMPARE(direct(QStringLiteral("Release")), 2U); // Never activated.
    }
private:
    QDBusPendingCall createCall(quint32 capabilities) {
        return bus.call(interface, QStringLiteral("CreateSession"), {bus.request(1), bus.sessionHandle(), app, QString{},
            QVariantMap{{QStringLiteral("capabilities"), capabilities}}});
    }
    bool created(quint32 capabilities) {
        auto create = createCall(capabilities);
        if (!QTest::qWaitFor([this] { return consent->asks == 1; })) return false;
        Q_EMIT consent->completed(consent->token, RequestResponse::Success, ChoiceValues{});
        return bus.response(create) == 0U;
    }
    quint32 zones(int request) {
        return bus.response(bus.call(interface, QStringLiteral("GetZones"), {bus.request(request), bus.sessionHandle(), app, QVariantMap{}}));
    }
    // Barrier 1 is the left edge of the first zone, 2 lies between zones, 3 is
    // diagonal, 4 is the right edge of the second zone.
    quint32 barriers(int request, uint zoneSet) {
        auto edge = [](uint id, int x1, int y1, int x2, int y2) {
            QDBusArgument position; position.beginStructure(); position << x1 << y1 << x2 << y2; position.endStructure();
            return QVariantMap{{QStringLiteral("barrier_id"), id}, {QStringLiteral("position"), QVariant::fromValue(position)}};
        };
        const QList<QVariantMap> list{edge(1, 0, 0, 0, 1079), edge(2, 1920, 0, 1920, 1023), edge(3, 0, 0, 10, 10),
                                      edge(4, 3200, 0, 3200, 1023)};
        return bus.response(bus.call(interface, QStringLiteral("SetPointerBarriers"),
            {bus.request(request), bus.sessionHandle(), app, QVariantMap{}, QVariant::fromValue(list), zoneSet}));
    }
    quint32 direct(const QString &member, const QVariantMap &options = {}) {
        return bus.response(bus.call(interface, member, {bus.sessionHandle(), app, options}));
    }
    QDBusPendingReply<QDBusUnixFileDescriptor> capture(const QString &member, const QVariantMap &options) {
        auto call = bus.call(interface, member, {bus.sessionHandle(), app, options});
        PrivatePortalBus::wait(call);
        return call;
    }
    const QString app = QStringLiteral("org.test.App");
    const QString interface = QStringLiteral("org.freedesktop.impl.portal.InputCapture");
    PrivatePortalBus bus;
    QString selectedCompositor;
    std::unique_ptr<QObject> host;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<FakeConsent> consent;
    std::unique_ptr<CompositorEis> eis;
    std::unique_ptr<Manager> manager;
    std::unique_ptr<SignalSpy> spy;
};
QTEST_GUILESS_MAIN(InputCaptureTest)
#include "tst_input_capture.moc"
