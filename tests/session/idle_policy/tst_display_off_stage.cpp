// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/session/idle_policy/display_off_stage.h>
#include <qindaqt/session/idle_policy/source_preferences.h>
#include "../../../src/session/idle_policy/production/kwayland_dpms_controller.h"
#include <QtTest>
#include "../../services/power_client/support/fake_power_transport.h"

using namespace QindaQt;
using namespace QindaQt::Session::IdlePolicy;
using QindaQt::Tests::FakePowerTransport;
using QindaQt::Tests::powerClientSnapshot;

namespace {
class FakeIdle final : public Platform::Idle::IdleObservation {
public:
    void setTimeout(int value) override { timeout = value; Q_EMIT changed(); }
    bool available() const override { return timeout > 0 && live; }
    bool idle() const override { return available() && idleNow; }
    void refresh() override { live = true; Q_EMIT changed(); }
    void revoke() override { live = false; Q_EMIT changed(); }
    void setIdle(bool value) { idleNow = value; Q_EMIT changed(); }
    int timeout = 0;
    bool live = true;
    bool idleNow = false;
};
class FakeDisplay final : public DisplayPowerPort {
public:
    bool available() const override { return live; }
    void requestOff() override { ++offRequests; }
    void requestOn() override { ++onRequests; }
    void restoreAndStop() override { ++restoreRequests; live = false; }
    int offRequests = 0;
    int onRequests = 0;
    int restoreRequests = 0;
    bool live = true;
};
}

class DisplayOffStageTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void usesConfirmedTimeoutAndRestoresOnActivity();
    void displayLeaseAndUnknownOwnerStateSuppressTheStage();
    void disablingPreferenceRestoresAndDisarms();
    void losingConfirmedPreferencesRestoresWithoutFallback();
    void attachmentRevocationRestoresAndStopsTheStage();
    void missingAdmittedFdDoesNotFallbackToWaylandDisplay();
    void sourceSelectionUsesAdmittedWarningWithAcPriority();
    void sourcePreferencesRequireCurrentLineageAndBoundedValues();
};

void DisplayOffStageTests::usesConfirmedTimeoutAndRestoresOnActivity()
{
    FakeIdle idle;
    FakeDisplay display;
    FakePowerTransport transport;
    Power::PowerClient power(&transport);
    DisplayOffPreferences preference{true, 77};
    DisplayOffStage stage(idle, display, power, [&] { return std::optional(preference); });
    stage.start();
    QCOMPARE(idle.timeout, 77'000);
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 1);
    QVERIFY(stage.displaysOffRequested());
    idle.setIdle(false);
    QCOMPARE(display.onRequests, 1);
    QVERIFY(!stage.displaysOffRequested());
    stage.stop();
}

void DisplayOffStageTests::displayLeaseAndUnknownOwnerStateSuppressTheStage()
{
    FakeIdle idle;
    FakeDisplay display;
    FakePowerTransport transport;
    Power::PowerClient power(&transport);
    DisplayOffPreferences preference{true, 60};
    DisplayOffStage stage(idle, display, power, [&] { return std::optional(preference); });
    stage.start();
    power.start();
    transport.announceOwner(QStringLiteral(":1.42"));
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 0); // Current owner has no authenticated receipt yet.
    const auto query = transport.idleStateRequests.constLast();
    transport.replyIdleState(query, 0x7U, 0x2U);
    QVERIFY(power.hasIdleInhibitorState());
    QCOMPARE(idle.timeout, 0); // DisplayOff lease disarms the automatic stage.
    QCOMPARE(display.offRequests, 0);
    const auto requestId = power.refreshIdleInhibitorState();
    const auto nextQuery = transport.idleStateRequests.constLast();
    QCOMPARE(nextQuery.requestId, requestId);
    transport.replyIdleState(nextQuery, 0x7U, 0x0U);
    QVERIFY(!power.activeIdleInhibitorScopes().testFlag(Power::IdleInhibitorScope::DisplayOff));
    QCOMPARE(idle.timeout, 60'000);
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 1);
    stage.stop();
    power.stop();
}

void DisplayOffStageTests::disablingPreferenceRestoresAndDisarms()
{
    FakeIdle idle;
    FakeDisplay display;
    FakePowerTransport transport;
    Power::PowerClient power(&transport);
    DisplayOffPreferences preference{true, 20};
    DisplayOffStage stage(idle, display, power, [&] { return std::optional(preference); });
    stage.start();
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 1);
    preference = {false, 0};
    stage.refreshPreferences();
    QCOMPARE(display.onRequests, 1);
    QCOMPARE(idle.timeout, 0);
    QVERIFY(!stage.displaysOffRequested());
}

void DisplayOffStageTests::missingAdmittedFdDoesNotFallbackToWaylandDisplay()
{
    KWaylandDpmsController display;
    qputenv("WAYLAND_DISPLAY", QByteArrayLiteral("path-must-not-be-used"));
    QString error;
    QVERIFY(!display.start([] { return -1; }, [] { return true; }, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!display.available());
    qunsetenv("WAYLAND_DISPLAY");
}

void DisplayOffStageTests::sourceSelectionUsesAdmittedWarningWithAcPriority()
{
    Power::Snapshot snapshot;
    snapshot.epoch = 9;
    snapshot.availability = Power::Availability::Ready;
    snapshot.source.onBattery = false;
    snapshot.composite.warning = Power::WarningLevel::Action;
    QVERIFY(selectPowerSourceProfile(snapshot) == PowerSourceProfile::Ac);

    snapshot.source.onBattery = true;
    for (const auto warning : {Power::WarningLevel::Unknown,
                               Power::WarningLevel::None,
                               Power::WarningLevel::Discharging}) {
        snapshot.composite.warning = warning;
        QVERIFY(selectPowerSourceProfile(snapshot) == PowerSourceProfile::Battery);
    }
    for (const auto warning : {Power::WarningLevel::Low,
                               Power::WarningLevel::Critical,
                               Power::WarningLevel::Action}) {
        snapshot.composite.warning = warning;
        QVERIFY(selectPowerSourceProfile(snapshot) == PowerSourceProfile::LowBattery);
    }
    snapshot.availability = Power::Availability::Starting;
    QVERIFY(!selectPowerSourceProfile(snapshot));
    snapshot.availability = Power::Availability::Ready;
    snapshot.wireValid = false;
    QVERIFY(!selectPowerSourceProfile(snapshot));
}

void DisplayOffStageTests::sourcePreferencesRequireCurrentLineageAndBoundedValues()
{
    Services::SettingsClient::SettingsSnapshot snapshot;
    snapshot.owner = QStringLiteral(":1.90");
    snapshot.epoch = QStringLiteral("settings-epoch");
    snapshot.revision = 1;
    snapshot.values.insert(QStringLiteral("power.idle.lowBattery.displayOffEnabled"), true);
    snapshot.values.insert(QStringLiteral("power.idle.lowBattery.displayOffSeconds"), 3600);
    const auto preference = displayOffPreferencesFor(
        snapshot, snapshot.owner, PowerSourceProfile::LowBattery);
    QVERIFY(preference.has_value());
    QVERIFY(preference->enabled);
    QCOMPARE(preference->timeoutSeconds, 3600);

    QVERIFY(!displayOffPreferencesFor(
        snapshot, QStringLiteral(":1.91"), PowerSourceProfile::LowBattery));
    snapshot.values.insert(QStringLiteral("power.idle.lowBattery.displayOffSeconds"),
                           QStringLiteral("3600"));
    QVERIFY(!displayOffPreferencesFor(
        snapshot, snapshot.owner, PowerSourceProfile::LowBattery));
    snapshot.values.insert(QStringLiteral("power.idle.lowBattery.displayOffSeconds"), 14401);
    QVERIFY(!displayOffPreferencesFor(
        snapshot, snapshot.owner, PowerSourceProfile::LowBattery));
    snapshot.values.insert(QStringLiteral("power.idle.lowBattery.displayOffSeconds"), 60);
    snapshot.values.insert(QStringLiteral("power.idle.lowBattery.displayOffEnabled"), false);
    const auto disabled = displayOffPreferencesFor(
        snapshot, snapshot.owner, PowerSourceProfile::LowBattery);
    QVERIFY(disabled.has_value());
    QVERIFY(!disabled->enabled);
    QCOMPARE(disabled->timeoutSeconds, 0);
}

void DisplayOffStageTests::losingConfirmedPreferencesRestoresWithoutFallback()
{
    FakeIdle idle;
    FakeDisplay display;
    FakePowerTransport transport;
    Power::PowerClient power(&transport);
    std::optional<DisplayOffPreferences> confirmed = DisplayOffPreferences{true, 20};
    DisplayOffStage stage(idle, display, power, [&] { return confirmed; });
    stage.start();
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 1);

    confirmed.reset();
    stage.refreshPreferences();
    QCOMPARE(idle.timeout, 0);
    QCOMPARE(display.onRequests, 1);
    QVERIFY(!stage.displaysOffRequested());
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 1);
    stage.stop();
}

void DisplayOffStageTests::attachmentRevocationRestoresAndStopsTheStage()
{
    FakeIdle idle;
    FakeDisplay display;
    FakePowerTransport transport;
    Power::PowerClient power(&transport);
    DisplayOffPreferences preference{true, 20};
    DisplayOffStage stage(idle, display, power, [&] { return std::optional(preference); });
    stage.start();
    idle.setIdle(true);
    QCOMPARE(display.offRequests, 1);
    stage.attachmentRevoked();
    QCOMPARE(display.restoreRequests, 1);
    QCOMPARE(idle.timeout, 0);
    QVERIFY(!idle.available());
}

QTEST_GUILESS_MAIN(DisplayOffStageTests)
#include "tst_display_off_stage.moc"
