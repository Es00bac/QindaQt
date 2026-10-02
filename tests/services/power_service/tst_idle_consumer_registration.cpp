// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/private_bus.h"
#include "support/fake_power_collaborators.h"
#include <qindaqt/services/power_service/resident_power_service.h>
#include <qindaqt/services/power_client/idle_consumer_registrar.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <QDBusPendingReply>
#include <QtTest>
using namespace QindaQt::Power;
using namespace QindaQt::Tests;
namespace {
class Fixture final {
public:
    bool start() {
        if (!bus.start()) return false;
        server = bus.openConnection(QStringLiteral("server"));
        supervisor = bus.openConnection(QStringLiteral("supervisor"));
        stranger = bus.openConnection(QStringLiteral("stranger"));
        auto b = std::make_unique<FakeBatteryCollaborator>(); battery = b.get();
        auto p = std::make_unique<FakeProfileCollaborator>(); profiles = p.get();
        auto s = std::make_unique<FakeSessionCollaborator>(); session = s.get();
        service = std::make_unique<ResidentPowerService>(std::move(b), std::move(p), std::move(s), server);
        if (service->start() != PowerServiceStartStatus::Started) return false;
        battery->publish(fixtureBatteryFacts());
        profiles->publish(fixtureProfileFacts());
        session->publish(fixtureSessionFacts());
        return supervisor.registerService(QStringLiteral("org.qindaqt.Session1"));
    }
    QDBusMessage call(const QString &method, const QList<QVariant> &args = {}) const {
        auto result = QDBusMessage::createMethodCall(server.baseService(),
            QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName), method);
        result.setArguments(args); return result;
    }
    QDBusPendingReply<bool> declare(const QDBusConnection &actor, quint32 scopes, quint64 epoch = 0) {
        return actor.asyncCall(call(QStringLiteral("RegisterIdleConsumers"),
            {QVariant::fromValue(epoch == 0 ? currentEpoch() : epoch), scopes}));
    }
    QDBusPendingReply<quint32> query(const QString &method = QStringLiteral("GetIdleInhibitorCapabilities")) {
        return stranger.asyncCall(call(method));
    }
    QDBusPendingReply<Handle> acquire() {
        return stranger.asyncCall(call(QStringLiteral("AcquireIdleInhibitor"),
            {QStringLiteral("Video"),QStringLiteral("Playback"),quint32{7}}));
    }
    quint64 currentEpoch() const { return service->coordinator()->snapshot().epoch; }
    PrivateBus bus;
    QDBusConnection server{QStringLiteral("invalid")},supervisor{QStringLiteral("invalid")},stranger{QStringLiteral("invalid")};
    FakeBatteryCollaborator *battery=nullptr;
    FakeProfileCollaborator *profiles=nullptr;
    FakeSessionCollaborator *session=nullptr;
    std::unique_ptr<ResidentPowerService> service;
};
}
class IdleConsumerRegistrationTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void requiresExclusiveModeCurrentCallerEpochAndCompleteMask() {
        Fixture f; QVERIFY(f.start());
        auto disabled=f.declare(f.supervisor,7);QTRY_VERIFY(disabled.isFinished());QVERIFY(!disabled.isError());QVERIFY(!disabled.value());
        f.service->setNativeIdleAdmission(true);
        auto wrong=f.declare(f.stranger,7);QTRY_VERIFY(wrong.isFinished());QVERIFY(!wrong.value());
        auto stale=f.declare(f.supervisor,7,f.currentEpoch()+1);QTRY_VERIFY(stale.isFinished());QVERIFY(!stale.value());
        auto zero=QDBusPendingReply<bool>(f.supervisor.asyncCall(f.call(QStringLiteral("RegisterIdleConsumers"),{QVariant::fromValue(quint64{0}),quint32{7}})));
        QTRY_VERIFY(zero.isFinished());QVERIFY(!zero.value());
        for (quint32 scopes : {quint32{1},quint32{3},quint32{8},quint32{0xffffffff}}) {
            auto malformed=f.declare(f.supervisor,scopes);QTRY_VERIFY(malformed.isFinished());QVERIFY(!malformed.value());
        }
        auto ready=f.declare(f.supervisor,7);QTRY_VERIFY(ready.isFinished());QVERIFY2(!ready.isError(),qPrintable(ready.error().message()));QVERIFY(ready.value());
        auto caps=f.query();QTRY_VERIFY(caps.isFinished());QCOMPARE(caps.value(),quint32{7});
        auto lease=f.acquire();QTRY_VERIFY(lease.isFinished());QVERIFY(!lease.isError());QCOMPARE(lease.value().epoch,f.currentEpoch());QVERIFY(!lease.value().opaqueId.isEmpty());
        auto active=f.query(QStringLiteral("GetActiveIdleInhibitorScopes"));QTRY_VERIFY(active.isFinished());QCOMPARE(active.value(),quint32{7});
        auto foreign=QDBusPendingReply<bool>(f.supervisor.asyncCall(f.call(QStringLiteral("ReleaseIdleInhibitor"),{QVariant::fromValue(lease.value())})));
        QTRY_VERIFY(foreign.isFinished());QVERIFY(!foreign.value());
        auto own=QDBusPendingReply<bool>(f.stranger.asyncCall(f.call(QStringLiteral("ReleaseIdleInhibitor"),{QVariant::fromValue(lease.value())})));
        QTRY_VERIFY(own.isFinished());QVERIFY(own.value());
    }
    void supervisorReplacementAndEpochChangeRevokeLeases() {
        Fixture f;QVERIFY(f.start());f.service->setNativeIdleAdmission(true);
        auto registration=f.declare(f.supervisor,7);QTRY_VERIFY(registration.isFinished());QVERIFY(registration.value());
        auto lease=f.acquire();QTRY_VERIFY(lease.isFinished());QVERIFY(!lease.isError());
        QVERIFY(f.supervisor.unregisterService(QStringLiteral("org.qindaqt.Session1")));
        QVERIFY(f.stranger.registerService(QStringLiteral("org.qindaqt.Session1")));
        auto caps=f.query();QTRY_VERIFY(caps.isFinished());QCOMPARE(caps.value(),quint32{0});
        auto release=QDBusPendingReply<bool>(f.stranger.asyncCall(f.call(QStringLiteral("ReleaseIdleInhibitor"),{QVariant::fromValue(lease.value())})));
        QTRY_VERIFY(release.isFinished());QVERIFY(!release.value());
        auto old=f.declare(f.supervisor,7);QTRY_VERIFY(old.isFinished());QVERIFY(!old.value());
        auto replacement=f.declare(f.stranger,7);QTRY_VERIFY(replacement.isFinished());QVERIFY(replacement.value());
        const auto epoch=f.currentEpoch();f.battery->replaceAuthority();
        QVERIFY(f.currentEpoch()!=epoch);
        auto after=f.query();QTRY_VERIFY(after.isFinished());QCOMPARE(after.value(),quint32{0});
        auto stale=f.declare(f.stranger,7,epoch);QTRY_VERIFY(stale.isFinished());QVERIFY(!stale.value());
    }
    void legacyArrivalWithdrawalAndAdmissionLossRevokeLeases() {
        Fixture f;QVERIFY(f.start());f.service->setNativeIdleAdmission(true);
        auto ready=f.declare(f.supervisor,7);QTRY_VERIFY(ready.isFinished());QVERIFY(ready.value());
        auto lease=f.acquire();QTRY_VERIFY(lease.isFinished());QVERIFY(!lease.isError());
        QVERIFY(f.stranger.registerService(QStringLiteral("org.kde.Solid.PowerManagement")));
        auto caps=f.query();QTRY_VERIFY(caps.isFinished());QCOMPARE(caps.value(),quint32{0});
        auto competing=f.declare(f.supervisor,7);QTRY_VERIFY(competing.isFinished());QVERIFY(!competing.value());
        QVERIFY(f.stranger.unregisterService(QStringLiteral("org.kde.Solid.PowerManagement")));
        auto restored=f.declare(f.supervisor,7);QTRY_VERIFY(restored.isFinished());QVERIFY(restored.value());
        auto withdrawal=f.declare(f.supervisor,0);QTRY_VERIFY(withdrawal.isFinished());QVERIFY(withdrawal.value());
        auto cleared=f.query();QTRY_VERIFY(cleared.isFinished());QCOMPARE(cleared.value(),quint32{0});
        auto again=f.declare(f.supervisor,7);QTRY_VERIFY(again.isFinished());QVERIFY(again.value());
        f.service->setNativeIdleAdmission(false);
        auto lost=f.query();QTRY_VERIFY(lost.isFinished());QCOMPARE(lost.value(),quint32{0});
        auto unsupported=f.acquire();QTRY_VERIFY(unsupported.isFinished());QVERIFY(unsupported.isError());
        QCOMPARE(unsupported.error().name(),QStringLiteral("org.qindaqt.Power1.Error.Unsupported"));
    }
    void publicRegistrarUsesCurrentLineageAndWithdrawsOnCancel() {
        Fixture f;QVERIFY(f.start());f.service->setNativeIdleAdmission(true);
        QtPowerTransport transport(f.supervisor);PowerClient power(&transport);power.start();
        QTRY_VERIFY(power.hasSnapshot());QTRY_VERIFY(power.hasIdleInhibitorState());
        IdleConsumerRegistrar registrar(f.supervisor,power);QSignalSpy finished(&registrar,&IdleConsumerRegistrar::requestFinished);
        const auto all=IdleInhibitorScopes::fromInt(7);
        QVERIFY(registrar.declareConsumers(all)!=0);QTRY_COMPARE(finished.size(),1);
        QVERIFY(finished.first().at(1).toBool());QVERIFY(finished.first().at(2).toBool());
        QTRY_COMPARE(power.supportedIdleInhibitorScopes(),all);
        auto lease=f.acquire();QTRY_VERIFY(lease.isFinished());QVERIFY(!lease.isError());
        registrar.cancel();QTRY_COMPARE(power.supportedIdleInhibitorScopes(),IdleInhibitorScopes{});
        QVERIFY(registrar.declareConsumers(IdleInhibitorScope::AutomaticLock)==0);
        IdleConsumerRegistrar ordinary(f.stranger,power);QVERIFY(ordinary.declareConsumers(all)==0);
        finished.clear();QVERIFY(registrar.declareConsumers(all)!=0);registrar.cancel();
        auto caps=f.query();QTRY_VERIFY(caps.isFinished());QCOMPARE(caps.value(),quint32{0});
        QCoreApplication::processEvents();QCOMPARE(finished.size(),0);
        power.stop();
    }
};
QTEST_GUILESS_MAIN(IdleConsumerRegistrationTests)
#include "tst_idle_consumer_registration.moc"
