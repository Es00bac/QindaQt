// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
#include <qindaqt/services/native_lock_service/native_lock_request.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/native_lock_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/native_lock_runtime/native_lock_runtime.h>
#include <QtTest>
#include "../../apps/settings/screensaver/screensaver_model_test_support.h"
#include "../../services/power_client/support/fake_power_transport.h"

using namespace QindaQt;
using namespace QindaQt::Session::NativeLockRuntime;
using namespace QindaQt::Services::SessionLockState;
using namespace QindaQt::Services::NativeLock;
using ScreensaverTestSupport::FakeSettingsTransport;
using QindaQt::Tests::FakePowerTransport;
using QindaQt::Tests::powerClientSnapshot;

namespace {
class FakeIdle final : public Platform::Idle::IdleObservation {
public:
    using IdleObservation::IdleObservation;
    void setTimeout(int milliseconds) override {
        timeout = milliseconds;
        Q_EMIT changed();
    }
    bool available() const override { return enabled && timeout > 0; }
    bool idle() const override { return available() && idleNow; }
    void refresh() override {}
    void revoke() override { enabled = false; Q_EMIT changed(); }
    void setIdle(bool value) { idleNow = value; Q_EMIT changed(); }
    int timeout = 0;
    bool enabled = true;
    bool idleNow = false;
};

class FakeRequest final : public NativeLockRequest {
public:
    using NativeLockRequest::NativeLockRequest;
    bool request() override { ++requests; return allow; }
    void cancel() override {}
    int requests = 0;
    bool allow = true;
};

class MemoryLockTransport final : public NativeLockTransport {
public:
    struct Query { quint64 generation; quint64 serial; QString owner; };
    bool start(QString *) override { running = true; return true; }
    void stop() override { running = false; }
    void requestOwner(quint64 value) override { generation = value; }
    void requestPid(quint64 value, const QString &name) override { generation = value; owner = name; }
    bool subscribe(const QString &name) override { subscribed = name; return true; }
    void unsubscribe() override { subscribed.clear(); }
    void requestState(quint64 gen, quint64 serial, const QString &name) override {
        queries.append({gen, serial, name});
    }
    void authority() {
        Q_EMIT ownerResolved(generation, QStringLiteral(":1.10"));
        Q_EMIT pidResolved(generation, QStringLiteral(":1.10"), 4242);
    }
    void invalidate() { Q_EMIT stateInvalidated(QStringLiteral(":1.10")); }
    void answer(bool locked, bool protectedPresentation) {
        const auto query = queries.constLast();
        Q_EMIT stateResolved(query.generation, query.serial, query.owner,
                              locked, protectedPresentation);
    }
    bool running = false;
    quint64 generation = 0;
    QString owner, subscribed;
    QList<Query> queries;
};

struct Fixture {
    FakeSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settings{
        settingsTransport, Services::LockPreferences::scopedKeys()};
    Services::LockPreferences::PreferencesProvider preferences{settings};
    FakeIdle idle;
    FakeRequest request;
    MemoryLockTransport lockTransport;
    NativeLockStateMonitor monitor{lockTransport,
        [](const QString &owner, quint64 pid) {
            return owner == QStringLiteral(":1.10") && pid == 4242;
        }};
    FakePowerTransport powerTransport;
    Power::PowerClient power{&powerTransport};
    std::unique_ptr<Runtime> runtime;

    void seed(bool automatic, bool resume, int grace) {
        settingsTransport.setValue("lock.automaticEnabled", automatic);
        settingsTransport.setValue("lock.idleTimeoutSeconds", QVariant::fromValue<qint64>(300));
        settingsTransport.setValue("lock.onResume", resume);
        settingsTransport.setValue("lock.graceSeconds", QVariant::fromValue<qint64>(grace));
    }
    void start(bool withPower = false) {
        QString error;
        QVERIFY(settings.start(&error));
        settingsTransport.announceOwner();
        QTRY_VERIFY_WITH_TIMEOUT(settings.snapshot().has_value(), 3000);
        QVERIFY(monitor.start(&error));
        lockTransport.authority();
        runtime = std::make_unique<Runtime>(preferences, idle, request, monitor,
                                            withPower ? &power : nullptr);
        QVERIFY(runtime->start());
        lockTransport.authority();
    }
    void unlockState() {
        lockTransport.answer(false, false);
        QCOMPARE(monitor.state(), LockState::Unlocked);
    }
    ~Fixture() {
        runtime.reset();
        settings.stop();
        monitor.stop();
        power.stop();
    }
};
}

class NativeLockRuntimeTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void automaticIdleUsesConfirmedTimeoutAndCancelableGrace();
    void manualLockWorksWhenAutomaticIdleIsDisabled();
    void automaticLockLeaseDefersIdleButNotManual();
    void automaticLockWaitsForCurrentPowerOwnerLeaseState();
    void suspendWaitsForActualProtectedState();
    void resumeWaitsForAuthenticatedStateAndHonorsPreference();
};

void NativeLockRuntimeTests::automaticIdleUsesConfirmedTimeoutAndCancelableGrace() {
    Fixture f;
    f.seed(true, true, 5);
    f.start();
    f.unlockState();
    QCOMPARE(f.idle.timeout, 300000);
    f.idle.setIdle(true);
    QTest::qWait(300);
    QCOMPARE(f.request.requests, 0);
    f.idle.setIdle(false);
    QTest::qWait(5000);
    QCOMPARE(f.request.requests, 0);
    f.idle.setIdle(true);
    QTRY_COMPARE_WITH_TIMEOUT(f.request.requests, 1, 6000);
    QVERIFY(f.monitor.state() == LockState::Unlocked);
    f.settings.stop();
    f.monitor.stop();
}

void NativeLockRuntimeTests::manualLockWorksWhenAutomaticIdleIsDisabled() {
    Fixture f;
    f.seed(false, false, 30);
    f.start();
    f.unlockState();
    QCOMPARE(f.idle.timeout, 0);
    QVERIFY(!f.idle.available());
    QVERIFY(f.runtime->requestManualLock());
    QCOMPARE(f.request.requests, 1);
}

void NativeLockRuntimeTests::automaticLockLeaseDefersIdleButNotManual() {
    Fixture f;
    f.seed(true, false, 0);
    f.start(true);
    f.powerTransport.announceOwner(QStringLiteral(":1.42"));
    QCOMPARE(f.powerTransport.idleStateRequests.size(), 1);
    f.powerTransport.replyIdleState(f.powerTransport.idleStateRequests.constLast(),
                                    0x7U, 0x1U);
    QVERIFY(f.power.hasIdleInhibitorState());
    QVERIFY(f.power.activeIdleInhibitorScopes().testFlag(
        Power::IdleInhibitorScope::AutomaticLock));
    f.unlockState();
    f.idle.setIdle(true);
    QTest::qWait(100);
    QCOMPARE(f.request.requests, 0);

    // This lease only suppresses the automatic path; an explicit request still
    // reaches NativeLockRequest while the same confirmed lease is active.
    QVERIFY(f.runtime->requestManualLock());
    QCOMPARE(f.request.requests, 1);
}

void NativeLockRuntimeTests::automaticLockWaitsForCurrentPowerOwnerLeaseState() {
    Fixture f;
    f.seed(true, false, 0);
    f.start(true);
    f.powerTransport.announceOwner(QStringLiteral(":1.42"));
    f.unlockState();
    f.idle.setIdle(true);
    QTest::qWait(100);
    QCOMPARE(f.request.requests, 0);

    // Once the exact current owner confirms no active scopes, the idle stage
    // may proceed. A delayed or failed query must never race an accepted lease.
    const auto query = f.powerTransport.idleStateRequests.constLast();
    f.powerTransport.replyIdleState(query, 0x7U, 0x0U);
    QTRY_COMPARE_WITH_TIMEOUT(f.request.requests, 1, 1000);
}

void NativeLockRuntimeTests::suspendWaitsForActualProtectedState() {
    Fixture f;
    f.seed(false, false, 0);
    f.start();
    f.unlockState();
    bool dispatched = false;
    QSignalSpy completion(f.runtime.get(), &Runtime::suspendDispatchFinished);
    QVERIFY(f.runtime->requestSuspend([&] { dispatched = true; }));
    QCOMPARE(f.request.requests, 1);
    QVERIFY(!dispatched);
    f.request.completed(RequestResult::Admitted);
    QCOMPARE(f.monitor.state(), LockState::Unlocked);
    QVERIFY(!dispatched);
    f.lockTransport.invalidate();
    f.lockTransport.answer(true, false);
    QCOMPARE(f.monitor.state(), LockState::Locking);
    QVERIFY(!dispatched);
    f.lockTransport.invalidate();
    f.lockTransport.answer(true, true);
    QCOMPARE(f.monitor.state(), LockState::Locked);
    QVERIFY(dispatched);
    QCOMPARE(completion.size(), 1);
    QCOMPARE(completion.constFirst().constFirst().toBool(), true);
}

void NativeLockRuntimeTests::resumeWaitsForAuthenticatedStateAndHonorsPreference() {
    Fixture f;
    f.seed(true, true, 0);
    f.start(true);
    auto sleeping = powerClientSnapshot();
    sleeping.source.preparingForSleep = true;
    f.powerTransport.announceOwner(QStringLiteral(":1.42"));
    QVERIFY(!f.powerTransport.fetches.isEmpty());
    f.powerTransport.reply(f.powerTransport.fetches.constLast(), sleeping);
    QTRY_VERIFY(f.power.hasSnapshot());
    auto awake = sleeping;
    awake.revision++;
    awake.source.preparingForSleep = false;
    f.powerTransport.invalidate(QStringLiteral(":1.42"), sleeping.epoch,
                                awake.revision);
    f.powerTransport.reply(f.powerTransport.fetches.constLast(), awake);
    QTRY_VERIFY(!f.power.snapshot().source.preparingForSleep);
    QCOMPARE(f.request.requests, 0);
    f.lockTransport.authority();
    f.unlockState();
    QTRY_COMPARE(f.request.requests, 1);
}
QTEST_GUILESS_MAIN(NativeLockRuntimeTests)
#include "tst_native_lock_runtime.moc"
