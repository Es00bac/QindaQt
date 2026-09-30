// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/private_bus.h"
#include <qindaqt/services/portal/inhibit_adaptor.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/power_service/resident_power_service.h>
#include <qindaqt/services/power_service/unavailable_power_collaborators.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Power;
class PowerPort final : public PowerTransport {
public:
    using PowerTransport::PowerTransport;
    void start() override { QTimer::singleShot(0, this, [this] { Q_EMIT ownerChanged(QStringLiteral(":1.100")); }); }
    void stop() override {}
    void fetchSnapshot(const QString &, quint64) override {}
    void submitOperation(const QString &, quint64, const PowerClientRequest &) override {}
    void queryIdleInhibitorState(const QString &owner, quint64 serial) override {
        QTimer::singleShot(0, this, [this, owner, serial] { Q_EMIT idleInhibitorStateReply(owner, serial, true, supported, 0, {}); });
    }
    void acquireIdleInhibitor(const QString &owner, quint64 serial, const QString &, const QString &, IdleInhibitorScopes scopes) override {
        ++acquires; requested = scopes.toInt(); lastOwner = owner; lastSerial = serial;
        if (!hold) QTimer::singleShot(0, this, [this, owner, serial] { Q_EMIT idleInhibitorAcquireReply(owner, serial, true, Handle{1, QStringLiteral("owned")}, {}); });
    }
    void releaseIdleInhibitor(const QString &, quint64, const Handle &) override { ++releases; }
    quint32 supported = 7; bool hold = false; int acquires = 0, releases = 0; quint32 requested = 0;
    QString lastOwner; quint64 lastSerial = 0;
};
class InhibitTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start()); backend = fixture->connect(); frontend = fixture->connect();
        QVERIFY(frontend->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        registry = std::make_unique<RequestRegistry>(*backend); port = std::make_unique<PowerPort>();
        idle = std::make_unique<PowerIdleInhibition>(*port, [] { return true; }); host = std::make_unique<QObject>();
        new InhibitAdaptor(*host, *registry, *idle, *backend);
        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(backend->registerService(QStringLiteral("org.test.Portal"))); QTest::qWait(20);
    }
    void cleanup() { host.reset(); idle.reset(); port.reset(); registry.reset(); backend.reset(); frontend.reset(); fixture.reset(); }
    void completeIdleRequestCloseReleases() {
        auto reply = call(8); QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<> result = *reply;
        QVERIFY(!result.isError()); QCOMPARE(port->acquires, 1); QCOMPARE(port->requested, 7U);
        close(); QTRY_COMPARE(port->releases, 1);
    }
    void partialAndUnsupportedScopesFail() {
        port->supported = 3; auto partial = call(8); QTRY_VERIFY(partial->isFinished()); const QDBusPendingReply<> result = *partial;
        QVERIFY(result.isError()); QCOMPARE(port->acquires, 0);
        auto explicitSuspend = call(4); QTRY_VERIFY(explicitSuspend->isFinished()); const QDBusPendingReply<> suspend = *explicitSuspend;
        QVERIFY(suspend.isError()); QCOMPARE(port->acquires, 0);
        auto zero = call(0); QTRY_VERIFY(zero->isFinished()); const QDBusPendingReply<> invalid = *zero; QVERIFY(invalid.isError());
    }
    void cancelledLateAcquireReleases() {
        port->hold = true; auto reply = call(8); QTRY_COMPARE(port->acquires, 1); close();
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<> result = *reply; QVERIFY(result.isError());
        Q_EMIT port->idleInhibitorAcquireReply(port->lastOwner, port->lastSerial, true, Handle{1, QStringLiteral("late")}, {});
        QCOMPARE(port->releases, 1);
    }
    void frontendLossReleasesLease() {
        auto reply = call(8); QTRY_VERIFY(reply->isFinished()); QVERIFY(frontend->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_COMPARE(port->releases, 1);
    }

    void actualResidentZeroScopesRefusesIdle() {
        backend->unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop")); host.reset(); idle.reset(); port.reset();
        auto provider = fixture->connect();
        ResidentPowerService resident(std::make_unique<UnavailableBatteryCollaborator>(),
            std::make_unique<UnavailableProfileCollaborator>(), std::make_unique<UnavailableSessionCollaborator>(), *provider);
        QCOMPARE(resident.start(), PowerServiceStartStatus::Started);
        QtPowerTransport actual(*backend); QSignalSpy owner(&actual, &PowerTransport::ownerChanged);
        PowerIdleInhibition nativeIdle(actual, [] { return true; }); QTRY_VERIFY(!owner.isEmpty());
        host = std::make_unique<QObject>(); new InhibitAdaptor(*host, *registry, nativeIdle, *backend);
        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        auto reply = call(8); QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<> result = *reply;
        QVERIFY(result.isError()); QCOMPARE(result.error().name(), QStringLiteral("org.freedesktop.portal.Error.NotAllowed"));
        // Real current-owner Power1 receipt reports zero. No synthetic scope
        // acknowledgement is substituted for unavailable native consumers.
        backend->unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop")); host.reset();
    }
    void providerReplacementNoReplay() {
        auto reply = call(8); QTRY_VERIFY(reply->isFinished());
        Q_EMIT port->ownerChanged(QStringLiteral(":1.101")); QTest::qWait(20); QCOMPARE(port->acquires, 1);
    }
private:
    std::unique_ptr<QDBusPendingCallWatcher> call(quint32 flags) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.Inhibit"), QStringLiteral("Inhibit"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QStringLiteral("org.test.App"), QString{}, flags,
            QVariantMap{{QStringLiteral("reason"), QStringLiteral("Synthetic playback")}}});
        return std::make_unique<QDBusPendingCallWatcher>(frontend->asyncCall(message));
    }
    void close() {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), path,
            QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher reply(frontend->asyncCall(message)); QTRY_VERIFY(reply.isFinished()); const QDBusPendingReply<> result = reply; QVERIFY(!result.isError());
    }
    const QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/fixture/inhibit");
    std::unique_ptr<PortalPrivateBus> fixture; std::unique_ptr<QDBusConnection> backend, frontend;
    std::unique_ptr<RequestRegistry> registry; std::unique_ptr<PowerPort> port;
    std::unique_ptr<PowerIdleInhibition> idle; std::unique_ptr<QObject> host;
};
QTEST_GUILESS_MAIN(InhibitTest)
#include "tst_inhibit.moc"
