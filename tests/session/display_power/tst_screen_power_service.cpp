// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native_sleep/sleep_test_support.h"
#include "../../services/power_client/support/fake_power_transport.h"
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/session/display_power/display_power_facade.h>
#include <qindaqt/session/display_power/scoped_display_power.h>
#include <qindaqt/session/display_power/screen_power_service.h>
#include <QDBusContext>
#include <QDBusPendingReply>
#include <QSet>
using namespace QindaQt;
using namespace QindaQt::Session::DisplayPower;
namespace {
const QString Path = QStringLiteral("/org/qindaqt/KWin/DisplayPower");
const QString Interface = QStringLiteral("org.qindaqt.KWin.DisplayPower1");
const QString ScreenPath = QStringLiteral("/org/qindaqt/ScreenPower1");
const QString ScreenInterface = QStringLiteral("org.qindaqt.ScreenPower1");
class BlankPeer final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.DisplayPower1")
public:
    BlankPeer(QDBusConnection connection, QString supervisor)
        : bus(std::move(connection)), actor(std::move(supervisor)) {}
public Q_SLOTS:
    void RequestInventoryWithReceipt(const QString &nonce) {
        auto receipt = QDBusMessage::createTargetedSignal(message().service(), Path, Interface, QStringLiteral("InventoryReceipt"));
        receipt << nonce << qulonglong{9} << true << admitted << QByteArray(R"([{"id":"panel","name":"Fixture","dpms":true,"on":true,"geometry":[0,0,640,480]}])");
        bus.send(receipt);
    }
    bool SetNativeAdmission(qulonglong epoch, bool value) {
        if (epoch != 9 || message().service() != actor) return false;
        admitted = value; if (!value) causes.clear(); return true;
    }
    bool AcquireBlank(qulonglong epoch, const QString &id, qulonglong deadline) {
        if (epoch != 9 || message().service() != actor || !admitted || !deadline) return false;
        causes.insert(id); return true;
    }
    bool ReleaseBlank(qulonglong epoch, const QString &id) {
        return epoch == 9 && message().service() == actor && causes.remove(id) > 0;
    }
public:
    QDBusConnection bus; QString actor; QSet<QString> causes; bool admitted = false;
};
QDBusPendingReply<bool> screenCall(QDBusConnection &bus, const QString &member, QVariantList args = {}) {
    auto call = QDBusMessage::createMethodCall(ScreenInterface, ScreenPath, ScreenInterface, member);
    call.setArguments(args); return bus.asyncCall(call, 5000);
}
struct Fixture {
    SleepTest::Fixture native;
    Tests::FakePowerTransport powerTransport;
    Power::PowerClient power{&powerTransport};
    std::unique_ptr<BlankPeer> peer;
    std::unique_ptr<ScopedDisplayPower> scoped;
    std::unique_ptr<DisplayPowerFacade> facade;
    std::unique_ptr<ScreenPowerService> service;
    bool lockBeforeOff = true;
    void start() {
        native.startNative();
        QVERIFY(native.attacker.registerService(QStringLiteral("org.qindaqt.Power1")));
        power.start(); powerTransport.announceOwner(native.attacker.baseService());
        powerTransport.reply(powerTransport.fetches.last(), Tests::powerClientSnapshot());
        peer = std::make_unique<BlankPeer>(native.compositorBus, native.supervisor.baseService());
        QVERIFY(native.compositorBus.registerObject(Path, peer.get(), QDBusConnection::ExportAllSlots));
        scoped = std::make_unique<ScopedDisplayPower>(native.supervisor, native.compositorBus.baseService(), quint32(getpid()),
            [this] { return native.attachment.live(); }, true);
        QVERIFY(scoped->start()); QTRY_VERIFY(scoped->available());
        facade = std::make_unique<DisplayPowerFacade>(*scoped, power, native.runtime, [this] { return std::optional(lockBeforeOff); });
        service = std::make_unique<ScreenPowerService>(native.supervisor, *facade); QVERIFY(service->start());
    }
    ~Fixture() {
        if (service) service->stop();
        service.reset(); facade.reset(); scoped.reset();
        native.compositorBus.unregisterObject(Path); peer.reset(); power.stop();
    }
};
}
class ScreenPowerTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void actualProtectedReceiptGatesScreenOff() {
        Fixture f; f.start();
        auto pending = screenCall(f.native.attacker, QStringLiteral("ScreenOff"), {qulonglong{11}, QString(32, u'a')});
        QTRY_COMPARE(f.native.native.requests, 1); QVERIFY(!pending.isFinished()); QVERIFY(f.peer->causes.isEmpty());
        f.native.native.state(true, false); QTRY_COMPARE(f.native.monitor.state(), Services::SessionLockState::LockState::Locking);
        QVERIFY(f.peer->causes.isEmpty());
        f.native.native.state(true, true); QTRY_VERIFY(f.native.monitor.presentationProtected());
        QTRY_VERIFY(pending.isFinished()); QVERIFY(!pending.isError()); QVERIFY(pending.value());
        QCOMPARE(f.peer->causes.size(), 1);
    }
    void foreignCallerAndWrongEpochNeverAcquire() {
        Fixture f; f.lockBeforeOff = false; f.start();
        auto foreign = screenCall(f.native.logindBus, QStringLiteral("CanScreenOff"));
        QTRY_VERIFY(foreign.isFinished()); QVERIFY(!foreign.value());
        auto stale = screenCall(f.native.attacker, QStringLiteral("ScreenOff"), {qulonglong{12}, QString(32, u'b')});
        QTRY_VERIFY(stale.isFinished()); QVERIFY(!stale.value()); QVERIFY(f.peer->causes.isEmpty());
    }
    void idleAndLidReleaseRemainIndependent() {
        Fixture f; f.lockBeforeOff = false; f.start();
        f.facade->requestOff(); QTRY_COMPARE(f.peer->causes.size(), 1);
        const auto idleCause = *f.peer->causes.cbegin();
        auto lid = screenCall(f.native.attacker, QStringLiteral("ScreenOff"), {qulonglong{11}, QString(32, u'c')});
        QTRY_VERIFY(lid.isFinished()); QVERIFY(lid.value()); QCOMPARE(f.peer->causes.size(), 2);
        auto release = screenCall(f.native.attacker, QStringLiteral("ReleaseScreenOff"), {QString(32, u'c')});
        QTRY_VERIFY(release.isFinished()); QVERIFY(release.value()); QTRY_COMPARE(f.peer->causes.size(), 1);
        QVERIFY(f.peer->causes.contains(idleCause));
        f.facade->requestOn(); QTRY_VERIFY(f.peer->causes.isEmpty());
    }
    void cancelBeforeProtectionCannotAcquireOnLateReceipt() {
        Fixture f; f.start();
        auto pending = screenCall(f.native.attacker, QStringLiteral("ScreenOff"), {qulonglong{11}, QString(32, u'd')});
        QTRY_COMPARE(f.native.native.requests, 1);
        auto release = screenCall(f.native.attacker, QStringLiteral("ReleaseScreenOff"), {QString(32, u'd')});
        QTRY_VERIFY(release.isFinished()); QVERIFY(release.value());
        QTRY_VERIFY(pending.isFinished()); QVERIFY(!pending.value());
        f.native.native.state(true, true); QTRY_VERIFY(f.native.monitor.presentationProtected());
        QVERIFY(f.peer->causes.isEmpty());
    }
};
QTEST_GUILESS_MAIN(ScreenPowerTests)
#include "tst_screen_power_service.moc"
