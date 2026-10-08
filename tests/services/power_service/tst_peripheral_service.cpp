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
    props["Type"]=uint(999); props.remove("Percentage"); props["BatteryLevel"]=uint(2);
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
