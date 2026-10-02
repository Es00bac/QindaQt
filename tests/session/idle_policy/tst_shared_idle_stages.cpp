// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/idle_policy/dim_stage.h>
#include <qindaqt/session/idle_policy/idle_suspend_stage.h>
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/power_client/power_client.h>
#include "../../services/power_client/support/fake_power_transport.h"
#include <QtTest>
using namespace QindaQt;
using namespace QindaQt::Session::IdlePolicy;
namespace {
class Idle final : public Platform::Idle::IdleObservation {
public:
    void setTimeout(int ms) override { timeout = ms; idleNow = false; Q_EMIT changed(); }
    bool available() const override { return live && timeout > 0; }
    bool idle() const override { return available() && idleNow; }
    void refresh() override { live = true; Q_EMIT changed(); }
    void revoke() override { live = false; idleNow = false; Q_EMIT changed(); }
    void expire() { idleNow = true; Q_EMIT changed(); }
    void resume() { idleNow = false; Q_EMIT activity(); Q_EMIT changed(); }
    int timeout = 0; bool live = true, idleNow = false;
};
class Sleep final : public IdleSleepPort {
public:
    bool available() const override { return live; }
    void query(const QString &action, std::function<void(bool)> callback) override {
        queried << action; pending = std::move(callback);
    }
    quint64 request(const QString &action) override { actions << action; return ++last; }
    void cancel(quint64 token) override { cancelled << token; }
    void reply(bool value) { const auto callback = std::move(pending); if (callback) callback(value); }
    QStringList queried, actions; QList<quint64> cancelled;
    std::function<void(bool)> pending; quint64 last = 0; bool live = true;
};
struct Fixture {
    Tests::FakePowerTransport transport;
    Power::PowerClient power{&transport};
    Power::Snapshot snapshot = Tests::powerClientSnapshot();
    std::optional<SharedIdlePreferences> prefs = SharedIdlePreferences{
        QStringLiteral("settings|epoch|ac"), true, 30, {true, 60}, true, QStringLiteral("suspend"), 120};
    Fixture() {
        snapshot.capabilities |= Power::Capability::InternalBacklight;
        snapshot.internalBacklights = {{.handle = {snapshot.epoch, QStringLiteral("panel")},
            .deviceName = QStringLiteral("panel"), .internal = true, .kind = Power::BacklightKind::Firmware,
            .maximum = 100, .observedKnown = true, .observed = 80,
            .status = Power::BacklightStatus::Ok, .reason = Power::BacklightReason::None, .diagnostic = {}}};
        power.start(); transport.announceOwner(QStringLiteral(":1.42"));
        transport.reply(transport.fetches.last(), snapshot);
    }
    void scopes(quint32 active = 0) {
        if (transport.idleStateRequests.isEmpty()) return;
        const auto id = power.refreshIdleInhibitorState(); Q_UNUSED(id);
        transport.replyIdleState(transport.idleStateRequests.last(), 7, active);
    }
    void complete(Power::OperationStatus status, quint32 observed) {
        const auto operation = transport.operations.last();
        ++snapshot.revision; snapshot.internalBacklights[0].observed = observed;
        Power::OperationResult result{.kind = operation.request.kind, .status = status,
            .initiatingEpoch = snapshot.epoch, .initiatingRevision = snapshot.revision - 1,
            .observedEpoch = snapshot.epoch, .observedRevision = snapshot.revision,
            .reasonCode = QStringLiteral("fixture"), .diagnostic = {}, .wireValid = true};
        transport.finish(operation, result);
    }
    void readback() { transport.reply(transport.fetches.last(), snapshot); }
};
}
class SharedIdleStageTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void validatedSourcePreferences();
    void suspendUsesActualScopeReceiptAndCancelsLateCan();
    void suspendEpisodeConsumesFailureUntilRealActivity();
    void suspendSourceLossCancelsOnlyOwnedTokenAndUncertaintyQuarantines();
    void dimWaitsForReadbackBeforeOwnedRestore();
    void dimPreservesExternalBrightnessAndUncertainMutation();
};
void SharedIdleStageTests::validatedSourcePreferences() {
    Services::SettingsClient::SettingsSnapshot s;
    s.owner = QStringLiteral(":1.9"); s.epoch = QStringLiteral("epoch"); s.revision = 1;
    const auto prefix = QStringLiteral("power.idle.battery.");
    s.values[prefix + QStringLiteral("displayOffEnabled")] = true;
    s.values[prefix + QStringLiteral("displayOffSeconds")] = 60;
    s.values[prefix + QStringLiteral("dimEnabled")] = true;
    s.values[prefix + QStringLiteral("dimSeconds")] = 30;
    s.values[prefix + QStringLiteral("lockBeforeDisplayOff")] = true;
    s.values[prefix + QStringLiteral("suspendAction")] = QStringLiteral("hibernate");
    s.values[prefix + QStringLiteral("suspendSeconds")] = 120;
    const auto p = sharedIdlePreferencesFor(s, s.owner, PowerSourceProfile::Battery);
    QVERIFY(p); QVERIFY(p->lockBeforeDisplayOff); QCOMPARE(p->suspendAction, QStringLiteral("hibernate"));
    QVERIFY(!sharedIdlePreferencesFor(s, QStringLiteral(":1.10"), PowerSourceProfile::Battery));
    s.values[prefix + QStringLiteral("dimSeconds")] = QStringLiteral("30");
    QVERIFY(!sharedIdlePreferencesFor(s, s.owner, PowerSourceProfile::Battery));
    s.values[prefix + QStringLiteral("dimSeconds")] = 30;
    s.values[prefix + QStringLiteral("suspendAction")] = QStringLiteral("power-off");
    QVERIFY(!sharedIdlePreferencesFor(s, s.owner, PowerSourceProfile::Battery));
}
void SharedIdleStageTests::suspendUsesActualScopeReceiptAndCancelsLateCan() {
    Fixture f; Idle idle; Sleep sleep;
    QVERIFY(f.power.hasSnapshot());
    IdleSuspendStage stage(idle, f.power, sleep, [&] { return f.prefs; }); stage.start();
    QCOMPARE(idle.timeout, 0); idle.expire(); QVERIFY(sleep.queried.isEmpty());
    f.scopes(4); QCOMPARE(idle.timeout, 0);
    f.scopes(); QCOMPARE(idle.timeout, 120000); idle.expire(); QCOMPARE(sleep.queried.size(), 1);
    idle.resume(); sleep.reply(true); QVERIFY(sleep.actions.isEmpty());
    idle.expire(); sleep.reply(true); QCOMPARE(sleep.actions, QStringList{QStringLiteral("suspend")});
    idle.resume(); QCOMPARE(sleep.cancelled, QList<quint64>{1});
}
void SharedIdleStageTests::suspendEpisodeConsumesFailureUntilRealActivity() {
    Fixture f; f.scopes(); Idle idle; Sleep sleep;
    IdleSuspendStage stage(idle, f.power, sleep, [&] { return f.prefs; }); stage.start();
    idle.expire(); sleep.reply(false); QCOMPARE(sleep.queried.size(), 1);
    idle.revoke(); idle.refresh(); idle.expire(); stage.refreshPreferences();
    QCOMPARE(sleep.queried.size(), 1);
    idle.resume(); idle.expire(); sleep.reply(true); QCOMPARE(sleep.actions.size(), 1);
}
void SharedIdleStageTests::suspendSourceLossCancelsOnlyOwnedTokenAndUncertaintyQuarantines() {
    Fixture f; f.scopes(); Idle idle; Sleep sleep;
    IdleSuspendStage stage(idle, f.power, sleep, [&] { return f.prefs; }); stage.start();
    idle.expire(); sleep.reply(true); QCOMPARE(sleep.last, quint64(1));
    f.prefs->lineage = QStringLiteral("new-source"); stage.refreshPreferences();
    QCOMPARE(sleep.cancelled, QList<quint64>{1});
    idle.resume(); idle.expire(); sleep.reply(true); QCOMPARE(sleep.last, quint64(2));
    Q_EMIT sleep.requestFinished(2, true);
    idle.resume(); idle.expire(); QCOMPARE(sleep.actions.size(), 2);
    QCOMPARE(idle.timeout, 0);
}
void SharedIdleStageTests::dimWaitsForReadbackBeforeOwnedRestore() {
    Fixture f; f.scopes(); Idle idle;
    DimStage stage(idle, f.power, [&] { return f.prefs; }); stage.start();
    idle.expire(); QCOMPARE(f.transport.operations.size(), 1);
    QCOMPARE(f.transport.operations.last().request.value, quint32(24));
    idle.resume(); f.complete(Power::OperationStatus::Succeeded, 24);
    QTRY_VERIFY(!f.power.operationPending());
    // Receipt alone is not physical readback and cannot authorize restoration.
    QCOMPARE(f.transport.operations.size(), 1);
    f.readback(); QTRY_COMPARE(f.transport.operations.size(), 2);
    QCOMPARE(f.transport.operations.last().request.value, quint32(80));
}
void SharedIdleStageTests::dimPreservesExternalBrightnessAndUncertainMutation() {
    for (const bool uncertain : {false, true}) {
        Fixture f; f.scopes(); Idle idle;
        DimStage stage(idle, f.power, [&] { return f.prefs; }); stage.start(); idle.expire();
        f.complete(uncertain ? Power::OperationStatus::Uncertain : Power::OperationStatus::Succeeded, 60);
        QTRY_VERIFY(!f.power.operationPending()); f.readback(); idle.resume();
        QCOMPARE(f.transport.operations.size(), 1);
        idle.expire();
        if (uncertain) QCOMPARE(f.transport.operations.size(), 1);
    }
}
QTEST_GUILESS_MAIN(SharedIdleStageTests)
#include "tst_shared_idle_stages.moc"
