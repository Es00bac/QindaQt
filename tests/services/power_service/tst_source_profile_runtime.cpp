// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/profile_policy_bus.h"
#include "support/fake_ppd_service.h"
#include "support/fake_upower_service.h"
#include "support/profile_settings_source.h"
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_service/resident_settings_service.h>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QDir>
#include <QtTest>
using namespace QindaQt::Tests;
using namespace QindaQt::Power;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;
namespace {
QList<FakeUpowerService::DeviceSpec> supplies(bool battery, uint warning = 2)
{
    return {{QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0"),
             {{QStringLiteral("Type"), uint(2)}, {QStringLiteral("PowerSupply"), true},
              {QStringLiteral("IsPresent"), true}, {QStringLiteral("State"), uint(2)},
              {QStringLiteral("Percentage"), 55.0}, {QStringLiteral("BatteryLevel"), uint(1)},
              {QStringLiteral("WarningLevel"), warning}}},
            {QStringLiteral("/org/freedesktop/UPower/devices/line_power_AC"),
             {{QStringLiteral("Type"), uint(1)}, {QStringLiteral("Online"), !battery}}}};
}
struct Runtime {
    ProfilePolicyBus bus;
    QDBusConnection settingsBus{QStringLiteral("unused-settings")};
    QDBusConnection profilesBus{QStringLiteral("unused-profiles")};
    QDBusConnection upowerBus{QStringLiteral("unused-upower")};
    QDBusConnection clientBus{QStringLiteral("unused-client")};
    QDBusConnection legacyBus{QStringLiteral("unused-legacy")};
    std::unique_ptr<ResidentSettingsService> service;
    std::unique_ptr<FakePpdService> ppd;
    std::unique_ptr<FakeUpowerService> upower;
    std::unique_ptr<QtSettingsTransport> transport;
    std::unique_ptr<SettingsClient> settings;
    std::unique_ptr<QtPowerTransport> powerTransport;
    std::unique_ptr<PowerClient> power;
    QProcess process;
    ~Runtime() {
        process.terminate();
        if (!process.waitForFinished(2000)) { process.kill(); process.waitForFinished(); }
    }
    bool refuse(const QString &message) { qWarning().noquote() << message; return false; }
    bool start(bool exclusive = true, bool legacy = false) {
        if (!bus.start()) return refuse(QStringLiteral("private bus startup failed"));
        // Separate owners also avoid nested SubPath virtual-object registries.
        settingsBus = bus.open(); profilesBus = bus.open(); upowerBus = bus.open();
        clientBus = bus.open(); legacyBus = bus.open();
        if (legacy && !legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement"))) return false;
        if (!startSettings()) return false;
        QString error;
        ppd = std::make_unique<FakePpdService>(profilesBus, false);
        ppd->setProfiles({QStringLiteral("power-saver"), QStringLiteral("balanced"), QStringLiteral("performance")});
        ppd->setActiveProfile(QStringLiteral("balanced"));
        ppd->setHolds({{QStringLiteral("power-saver"), QStringLiteral("External"), QStringLiteral("untouched")}});
        if (!ppd->registerService()) return refuse(QStringLiteral("fixture profiles registration failed"));
        upower = std::make_unique<FakeUpowerService>(upowerBus);
        upower->setDevices(supplies(false));
        if (!upower->registerService()) return refuse(QStringLiteral("fixture supplies registration failed"));
        transport = std::make_unique<QtSettingsTransport>(clientBus);
        settings = std::make_unique<SettingsClient>(*transport,
            QStringList{QStringLiteral("power.profile.ac"), QStringLiteral("power.profile.battery"), QStringLiteral("power.profile.lowBattery")},
            ClientTiming{500, 0, {10,20}});
        if (!settings->start(&error)) return refuse(QStringLiteral("fixture Settings client startup: ") + error);
        powerTransport = std::make_unique<QtPowerTransport>(clientBus);
        power = std::make_unique<PowerClient>(powerTransport.get());
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), bus.address);
        environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), bus.address);
        environment.insert(QStringLiteral("HOME"), bus.root.path());
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), bus.root.path());
        environment.insert(QStringLiteral("XDG_DATA_HOME"), bus.root.path());
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), bus.root.path());
        process.setProcessEnvironment(environment);
        QStringList arguments{QStringLiteral("--upstream=production"),
             QStringLiteral("--backlight-root=") + bus.root.filePath(QStringLiteral("backlight"))};
        if (exclusive) arguments.append(QStringLiteral("--profile-policy=native-exclusive"));
        process.start(QStringLiteral(QINDAQT_POWER_SERVICE_EXECUTABLE), arguments);
        if (!process.waitForStarted()) return refuse(QStringLiteral("fixture resident process startup: ") + process.errorString());
        qInfo().noquote() << "source-profile-service pid" << process.processId()
                         << "root" << bus.root.path();
        power->start();
        return true;
    }
    bool startSettings(bool freshOwner = false) {
        if (freshOwner) settingsBus = bus.open();
        QString error;
        auto active = SettingsSchema::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
        auto old = SettingsSchema::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
        if (!active || !old) return refuse(QStringLiteral("fixture schema load: ") + error);
        service = std::make_unique<ResidentSettingsService>(settingsBus, *active, *old,
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
            bus.root.filePath(QStringLiteral("settings.json")));
        const auto settingsStatus = service->start();
        if (!settingsStatus.ok()) return refuse(QStringLiteral("fixture Settings1 startup: ")
            + settingsServiceStartStatusName(settingsStatus.status) + QLatin1Char(' ')
            + settingsStatus.message);
        return true;
    }
    void source(bool battery, uint warning = 2) {
        upower->setOnBattery(battery);
        upower->setDevices(supplies(battery, warning));
        upower->emitServicePropertiesChanged();
    }
};
}
class SourceProfileRuntimeTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void sourceSequenceNoneAndExternalHold();
    void explicitGateRevokesOnLegacyArrival();
    void ownerLossAndUnsupportedProfiles();
    void defaultAssemblyIsDormant();
    void holdLimitRetriesOnlyAfterAdmissionChanges();
    void knownRejectionDoesNotSpin();
    void timeoutDoesNotReplay();
    void dispatchedProviderLossDoesNotReplay();
    void sourceAuthorityLossRemovesOwnedHold();
    void releaseTimeoutDoesNotReplay();
    void balancedPreferenceReleasesOnlyOwnedHold();
    void manualProfileOverrideWaitsForNewPolicyInput();
    void regressedSettingsRevisionCannotSelectStalePreferences();
};
#define SET_PROFILE(row, settingKey, settingValue) \
    QTRY_VERIFY(row.settings->canSetUserValue(QStringLiteral(settingKey))); \
    QVERIFY(row.settings->setUserValue(QStringLiteral(settingKey), QStringLiteral(settingValue))); \
    QTRY_VERIFY(row.settings->canSetUserValue(QStringLiteral(settingKey)) \
        && row.settings->snapshot()->values.value(QStringLiteral(settingKey)).toString() == QStringLiteral(settingValue))
void SourceProfileRuntimeTests::sourceSequenceNoneAndExternalHold()
{
    Runtime row; QVERIFY(row.start());
    SET_PROFILE(row, "power.profile.ac", "performance");
    SET_PROFILE(row, "power.profile.battery", "none");
    SET_PROFILE(row, "power.profile.lowBattery", "power-saver");
    QTRY_VERIFY(row.power->hasSnapshot() && row.power->snapshot().profiles.holds.size() == 2);
    QCOMPARE(row.ppd->holdRequests.last().profile, QStringLiteral("performance"));
    QTest::qWait(100);
    const auto count = row.ppd->holdRequests.size();
    row.source(false); row.ppd->emitPropertiesChanged(); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), count);
    row.source(true);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 1);
    QCOMPARE(row.ppd->holdRequests.size(), count);
    QCOMPARE(row.ppd->releaseRequests.size(), count);
    row.source(true, 3);
    QTRY_COMPARE(row.ppd->holdRequests.size(), count + 1);
    QCOMPARE(row.ppd->holdRequests.last().profile, QStringLiteral("power-saver"));
    row.source(false);
    QTRY_COMPARE(row.ppd->holdRequests.size(), count + 2);
    QCOMPARE(row.ppd->holdRequests.last().profile, QStringLiteral("performance"));
    SET_PROFILE(row, "power.profile.ac", "none");
    QTRY_VERIFY(row.power->snapshot().profiles.holds.size() == 1);
    QCOMPARE(row.power->snapshot().profiles.holds.first().applicationName, QStringLiteral("External"));
    QVERIFY(row.ppd->setProfileRequests.isEmpty());
    QCOMPARE(row.power->supportedIdleInhibitorScopes(), IdleInhibitorScopes{});
}
void SourceProfileRuntimeTests::explicitGateRevokesOnLegacyArrival()
{
    Runtime row; QVERIFY(row.start(true, true));
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTest::qWait(150); QVERIFY(row.ppd->holdRequests.isEmpty());
    QVERIFY(row.legacyBus.unregisterService(QStringLiteral("org.kde.Solid.PowerManagement")));
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    QVERIFY(row.legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement")));
    QTRY_COMPARE(row.ppd->releaseRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 1);
    QTest::qWait(150); QCOMPARE(row.ppd->holdRequests.size(), 1);
}
void SourceProfileRuntimeTests::ownerLossAndUnsupportedProfiles()
{
    Runtime row; QVERIFY(row.start());
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    row.service->stop();
    QTRY_COMPARE(row.ppd->releaseRequests.size(), 1);
    row.source(true); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    // A replacement service process must have a new unique bus owner. The
    // same-owner epoch switch is deliberately refused by SettingsClient.
    QVERIFY(row.startSettings(true));
    row.source(false);
    QTRY_COMPARE(row.ppd->holdRequests.size(), 2);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    row.ppd->setProfiles({QStringLiteral("balanced"), QStringLiteral("power-saver")});
    row.ppd->setActiveProfile(QStringLiteral("balanced"));
    // Keep the unsupported own hold out of validated provider inventory;
    // provider disappearance invalidates authority rather than stale release.
    row.ppd->unregisterService();
    QTRY_VERIFY(!row.power->snapshot().capabilities.testFlag(Capability::ProfileHolds));
    row.source(true); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 2);
    row.ppd->setHolds({{QStringLiteral("power-saver"), QStringLiteral("External"), QStringLiteral("untouched")}});
    row.source(false); QTest::qWait(100);
    QVERIFY(row.ppd->registerService());
    QTRY_VERIFY(row.power->snapshot().capabilities.testFlag(Capability::ProfileHolds));
    row.source(false); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 2);
    QCOMPARE(row.ppd->releaseRequests.size(), 1);
}
void SourceProfileRuntimeTests::defaultAssemblyIsDormant()
{
    Runtime row; QVERIFY(row.start(false));
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_VERIFY(row.power->hasSnapshot());
    QTest::qWait(150); QVERIFY(row.ppd->holdRequests.isEmpty());
}
void SourceProfileRuntimeTests::holdLimitRetriesOnlyAfterAdmissionChanges()
{
    Runtime row; QVERIFY(row.start());
    QList<FakePpdService::HoldSpec> holds;
    for (int i = 0; i < 8; ++i)
        holds.append({QStringLiteral("power-saver"), QStringLiteral("External%1").arg(i), QStringLiteral("untouched")});
    row.ppd->setHolds(holds); row.ppd->emitPropertiesChanged();
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 8);
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTest::qWait(150); QVERIFY(row.ppd->holdRequests.isEmpty());
    row.source(false); QTest::qWait(150); QVERIFY(row.ppd->holdRequests.isEmpty());
    holds.removeLast(); row.ppd->setHolds(holds); row.ppd->emitPropertiesChanged();
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 8);
    SET_PROFILE(row, "power.profile.ac", "none");
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 7);
    QCOMPARE(row.ppd->releaseRequests.size(), 1);
}
void SourceProfileRuntimeTests::knownRejectionDoesNotSpin()
{
    Runtime row; QVERIFY(row.start()); row.ppd->setRejectHold(true);
    QList<FakePpdService::HoldSpec> holds{
        {QStringLiteral("power-saver"), QStringLiteral("External"), QStringLiteral("untouched")},
        {QStringLiteral("performance"), QStringLiteral("External2"), QStringLiteral("untouched")}};
    row.ppd->setHolds(holds); row.ppd->emitPropertiesChanged();
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    row.source(false); row.ppd->emitPropertiesChanged(); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    // The same authenticated inventory in another order is no new admission.
    holds.move(0, 1); row.ppd->setHolds(holds); row.ppd->emitPropertiesChanged();
    QTRY_COMPARE(row.power->snapshot().profiles.holds.first().applicationName, QStringLiteral("External2"));
    QTest::qWait(150); QCOMPARE(row.ppd->holdRequests.size(), 1);
    row.ppd->setProfiles({QStringLiteral("performance"), QStringLiteral("balanced"), QStringLiteral("power-saver")});
    row.ppd->emitPropertiesChanged();
    QTRY_COMPARE(row.power->snapshot().profiles.supported.first().id, QStringLiteral("performance"));
    QTest::qWait(150); QCOMPARE(row.ppd->holdRequests.size(), 1);
    row.ppd->setRejectHold(false);
    SET_PROFILE(row, "power.profile.ac", "power-saver");
    QTRY_COMPARE(row.ppd->holdRequests.size(), 2);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 3);
    QVERIFY(row.ppd->releaseRequests.isEmpty());
}
void SourceProfileRuntimeTests::timeoutDoesNotReplay()
{
    Runtime row; QVERIFY(row.start()); row.ppd->setDropHoldReply(true);
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    QTest::qWait(3300);
    row.ppd->setDropHoldReply(false);
    row.ppd->emitPropertiesChanged();
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    // The timed-out acquire yielded no cookie. Metadata cannot authorize an
    // invented release; the provider's connection lifetime owns retirement.
    QVERIFY(row.ppd->releaseRequests.isEmpty());
    SET_PROFILE(row, "power.profile.ac", "power-saver");
    row.source(true); row.source(false); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    QVERIFY(row.ppd->releaseRequests.isEmpty());
}
void SourceProfileRuntimeTests::dispatchedProviderLossDoesNotReplay()
{
    Runtime row; QVERIFY(row.start()); row.ppd->setDropHoldReply(true);
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    row.ppd->unregisterService();
    QTRY_VERIFY(!row.power->snapshot().capabilities.testFlag(Capability::ProfileHolds));
    row.ppd->setDropHoldReply(false);
    row.ppd->setHolds({{QStringLiteral("power-saver"), QStringLiteral("External"), QStringLiteral("untouched")}});
    QVERIFY(row.ppd->registerService());
    QTRY_VERIFY(row.power->snapshot().capabilities.testFlag(Capability::ProfileHolds));
    SET_PROFILE(row, "power.profile.ac", "power-saver");
    QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    QVERIFY(row.ppd->releaseRequests.isEmpty());
}
void SourceProfileRuntimeTests::sourceAuthorityLossRemovesOwnedHold()
{
    Runtime row; QVERIFY(row.start());
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    row.upower->unregisterService();
    QTRY_COMPARE(row.ppd->releaseRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 1);
    QTest::qWait(150); QCOMPARE(row.ppd->holdRequests.size(), 1);
    QVERIFY(row.upower->registerService());
    QTRY_COMPARE(row.ppd->holdRequests.size(), 2);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    QCOMPARE(row.ppd->releaseRequests.size(), 1);
}
void SourceProfileRuntimeTests::releaseTimeoutDoesNotReplay()
{
    Runtime row; QVERIFY(row.start());
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    row.ppd->setDropReleaseReply(true);
    SET_PROFILE(row, "power.profile.ac", "none");
    QTRY_COMPARE(row.ppd->releaseRequests.size(), 1);
    QTest::qWait(3300);
    SET_PROFILE(row, "power.profile.ac", "power-saver");
    row.source(true); row.source(false); row.ppd->emitPropertiesChanged();
    QTest::qWait(150);
    QCOMPARE(row.ppd->releaseRequests.size(), 1);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
}
void SourceProfileRuntimeTests::balancedPreferenceReleasesOnlyOwnedHold()
{
    Runtime row; QVERIFY(row.start());
    SET_PROFILE(row, "power.profile.ac", "performance");
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    SET_PROFILE(row, "power.profile.ac", "balanced");
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 1);
    QCOMPARE(row.ppd->releaseRequests.size(), 1);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    QCOMPARE(row.power->snapshot().profiles.activeProfileId, QStringLiteral("balanced"));
    QVERIFY(row.ppd->setProfileRequests.isEmpty());
}
void SourceProfileRuntimeTests::manualProfileOverrideWaitsForNewPolicyInput()
{
    Runtime row; QVERIFY(row.start());
    SET_PROFILE(row, "power.profile.ac", "performance");
    SET_PROFILE(row, "power.profile.battery", "power-saver");
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    // Only the exact-owner targeted ProfileReleased signal initiates readback.
    row.ppd->manualProfileChange(QStringLiteral("balanced"));
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 0);
    row.source(false); row.ppd->emitPropertiesChanged(); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    QVERIFY(row.ppd->releaseRequests.isEmpty());
    row.source(true);
    QTRY_COMPARE(row.ppd->holdRequests.size(), 2);
    QCOMPARE(row.ppd->holdRequests.last().profile, QStringLiteral("power-saver"));
}
void SourceProfileRuntimeTests::regressedSettingsRevisionCannotSelectStalePreferences()
{
    Runtime row; QVERIFY(row.start());
    QTRY_VERIFY(row.power->hasSnapshot());
    row.service->stop();
    auto replacementBus = row.bus.open();
    ProfileSettingsSource replacement(replacementBus);
    QVERIFY(replacement.start());
    QTRY_COMPARE(row.ppd->holdRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 2);
    replacement.revision = 9;
    replacement.values[QStringLiteral("power.profile.ac")] = QStringLiteral("power-saver");
    replacement.invalidate(11);
    QTRY_COMPARE(row.ppd->releaseRequests.size(), 1);
    QTRY_COMPARE(row.power->snapshot().profiles.holds.size(), 1);
    row.source(true); QTest::qWait(150);
    QCOMPARE(row.ppd->holdRequests.size(), 1);
    QVERIFY(row.ppd->setProfileRequests.isEmpty());
}
QTEST_GUILESS_MAIN(SourceProfileRuntimeTests)
#include "tst_source_profile_runtime.moc"
