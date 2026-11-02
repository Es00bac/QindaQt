// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/fake_power_collaborators.h"
#include "peripheral_decoder_p.h"
#include "upower_device_decoder_p.h"
#include <qindaqt/services/power_service/power_service_coordinator.h>
#include <QtTest>
using namespace QindaQt::Power;
using namespace QindaQt::Power::Upstream;
using namespace QindaQt::Tests;
class PeripheralServiceTest : public QObject {
 Q_OBJECT
private Q_SLOTS:
 void documentedUpowerBatteryLevels_data() {
    QTest::addColumn<uint>("raw");
    QTest::addColumn<uint>("expected");
    QTest::newRow("unknown") << uint(0) << uint(BatteryLevel::Unknown);
    QTest::newRow("percentage") << uint(1) << uint(BatteryLevel::None);
    QTest::newRow("low") << uint(3) << uint(BatteryLevel::Low);
    QTest::newRow("critical") << uint(4) << uint(BatteryLevel::Critical);
    QTest::newRow("normal") << uint(6) << uint(BatteryLevel::Normal);
    QTest::newRow("high") << uint(7) << uint(BatteryLevel::High);
    QTest::newRow("full") << uint(8) << uint(BatteryLevel::Full);
 }
 void documentedUpowerBatteryLevels() {
    QFETCH(uint,raw); QFETCH(uint,expected);
    QVariantMap props{{"Type",uint(12)},{"PowerSupply",false},
        {"Model",QStringLiteral("Controller")},{"BatteryLevel",raw},{"State",uint(2)}};
    props["Percentage"] = 55.0; // Coarse/Unknown levels must override this placeholder.
    PeripheralBattery row;
    QCOMPARE(decodePeripheral(QStringLiteral("/controller"),props,row),PeripheralDecode::Accepted);
    QCOMPARE(uint(row.level),expected);
    QCOMPARE(row.percentageKnown,raw == 1);
    QCOMPARE(row.percentage,raw == 1 ? 55.0 : 0.0);
    props["Type"] = uint(2); props["PowerSupply"] = true; props["IsPresent"] = true;
    UpowerDeviceTruth supply;
    QVERIFY(decodeUpowerDevice(QStringLiteral("/battery"),props,supply));
    QVERIFY(supply.hasSupply);
    QCOMPARE(uint(supply.supply.level),expected);
    QCOMPARE(supply.supply.percentageKnown,raw == 1);
 }
 void warningOnlyAndUnknownOrdinalsAreRejected_data() {
    QTest::addColumn<uint>("raw");
    QTest::newRow("discharging-warning") << uint(2);
    QTest::newRow("action-warning") << uint(5);
    QTest::newRow("outside-upower-enum") << uint(9);
 }
 void warningOnlyAndUnknownOrdinalsAreRejected() {
    QFETCH(uint,raw);
    QVariantMap props{{"Type",uint(12)},{"PowerSupply",false},{"BatteryLevel",raw}};
    PeripheralBattery row; row.model = QStringLiteral("retained");
    QCOMPARE(decodePeripheral(QStringLiteral("/controller"),props,row),PeripheralDecode::Malformed);
    QCOMPARE(row.model,QStringLiteral("retained"));
    props["Type"] = uint(2); props["PowerSupply"] = true; props["IsPresent"] = true;
    UpowerDeviceTruth supply;
    QVERIFY(!decodeUpowerDevice(QStringLiteral("/battery"),props,supply));
 }
 void dualSensePercentageAndChargingPassThrough() {
    // Sanitized live UPower properties: Linux has already converted the
    // controller's 10%-wide step to its midpoint. Never rescale it again.
    const QVariantMap props{{"Type",uint(12)},{"PowerSupply",false},
        {"Model",QStringLiteral("DualSense Wireless Controller")},
        {"BatteryLevel",uint(1)},{"Percentage",15.0},{"State",uint(1)}};
    PeripheralBattery row;
    QCOMPARE(decodePeripheral(QStringLiteral("/controller"),props,row),PeripheralDecode::Accepted);
    QCOMPARE(row.kind,PeripheralKind::Controller);
    QVERIFY(row.percentageKnown); QCOMPARE(row.percentage,15.0);
    QCOMPARE(row.state,ChargeState::Charging);
 }
 void speakerControllerAndMalformedIsolation() {
    QVariantMap props{{"Type",uint(18)},{"PowerSupply",false},{"Model",QStringLiteral("JLab GO Party")},{"Percentage",50.0},{"State",uint(2)}};
    PeripheralBattery row; UpowerDeviceTruth supply;
    QVERIFY(decodeUpowerDevice(QStringLiteral("/speaker"),props,supply)); QVERIFY(!supply.hasSupply);
    QCOMPARE(decodePeripheral(QStringLiteral("/speaker"),props,row),PeripheralDecode::Accepted);
    QCOMPARE(row.kind,PeripheralKind::Speaker); QCOMPARE(row.percentage,50.0); QVERIFY(!row.timeToEmptyKnown);
    QVERIFY(!row.handle.opaqueId.contains(QStringLiteral("/speaker")));
    props["Type"]=uint(12); props["Percentage"]=5.0;
    QCOMPARE(decodePeripheral(QStringLiteral("/controller"),props,row),PeripheralDecode::Accepted);
    QCOMPARE(row.kind,PeripheralKind::Controller); QCOMPARE(row.percentage,5.0);
    props["Percentage"]=QStringLiteral("bad");
    QCOMPARE(decodePeripheral(QStringLiteral("/controller"),props,row),PeripheralDecode::Malformed);
    QVERIFY(decodeUpowerDevice(QStringLiteral("/controller"),props,supply));
    props["Type"]=uint(999); props.remove("Percentage"); props["BatteryLevel"]=uint(3);
    QCOMPARE(decodePeripheral(QStringLiteral("/future"),props,row),PeripheralDecode::Accepted);
    QCOMPARE(row.kind,PeripheralKind::Other); QVERIFY(!row.percentageKnown); QCOMPARE(row.level,BatteryLevel::Low);
    props["PowerSupply"]=true;
    QCOMPARE(decodePeripheral(QStringLiteral("/battery"),props,row),PeripheralDecode::Ignored);
 }
 void independentRevisionGenerationAndLoss() {
    FakeBatteryCollaborator battery; FakeProfileCollaborator profiles; FakeSessionCollaborator session;
    PowerServiceCoordinator coordinator(&battery,&profiles,&session); coordinator.start();
    battery.publish(fixtureBatteryFacts()); profiles.publish(fixtureProfileFacts()); session.publish(fixtureSessionFacts());
    const auto original=coordinator.snapshot();
    PeripheralBattery row; row.handle={0,QStringLiteral("speaker")};
    PeripheralFacts facts; facts.devices={row};
    Q_EMIT battery.peripheralFactsChanged(battery.generation,facts);
    QCOMPARE(coordinator.peripheralSnapshot().devices.size(),1);
    QCOMPARE(coordinator.snapshot(),original);
    const auto accepted=coordinator.peripheralSnapshot();
    Q_EMIT battery.peripheralFactsChanged(battery.generation+1,{});
    QCOMPARE(coordinator.peripheralSnapshot(),accepted);
    facts.devices[0].percentage=900;
    Q_EMIT battery.peripheralFactsChanged(battery.generation,facts);
    QCOMPARE(coordinator.peripheralSnapshot().availability,Availability::Degraded);
    QVERIFY(coordinator.peripheralSnapshot().devices.isEmpty()); QCOMPARE(coordinator.snapshot(),original);
    facts.devices={row}; Q_EMIT battery.peripheralFactsChanged(battery.generation,facts);
    battery.replaceAuthority(); QVERIFY(coordinator.peripheralSnapshot().devices.isEmpty());
    QCOMPARE(coordinator.peripheralSnapshot().epoch,coordinator.snapshot().epoch);
    Q_EMIT battery.peripheralFactsChanged(battery.generation,facts);
    battery.publishUnavailable(); QVERIFY(coordinator.peripheralSnapshot().devices.isEmpty());
    coordinator.stop(); Q_EMIT battery.peripheralFactsChanged(battery.generation,facts);
    QVERIFY(coordinator.peripheralSnapshot().devices.isEmpty());
 }
};
QTEST_GUILESS_MAIN(PeripheralServiceTest)
#include "tst_peripheral_service.moc"
