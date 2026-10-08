// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_protocol/peripheral_types.h>
#include <QtTest>
#include <limits>
using namespace QindaQt::Power;
class PeripheralProtocolTest : public QObject {
 Q_OBJECT
private Q_SLOTS:
 void roundTripAndHostileLengths() {
    PeripheralSnapshot source; source.epoch=7; source.revision=3; source.availability=Availability::Ready;
    for (int i=0;i<64;++i) { PeripheralBattery row; row.handle={7,QString::number(i)}; row.model=QStringLiteral("Controller"); source.devices.push_back(row); }
    QByteArray bytes; QVERIFY(encodePeripheralSnapshot(source,bytes));
    PeripheralSnapshot out; QVERIFY(decodePeripheralSnapshot(bytes,out)); QVERIFY(out==source);
    const auto unchanged=out;
    for (qsizetype size=0;size<bytes.size();++size) {
        QVERIFY(!decodePeripheralSnapshot(bytes.first(size),out)); QVERIFY(out==unchanged);
    }
    QVERIFY(!decodePeripheralSnapshot(bytes+'x',out));
    auto malicious=bytes; for(int i=28;i<32;++i) malicious[i]=char(0xff);
    QVERIFY(!decodePeripheralSnapshot(malicious,out)); QVERIFY(out==unchanged);
    source.devices.push_back(source.devices.first()); QVERIFY(!validatePeripheralSnapshot(source));
 }
 void canonicalUnknownAndTruncation() {
    PeripheralSnapshot value; value.epoch=1; value.revision=1; value.availability=Availability::Ready;
    PeripheralBattery row; row.handle={1,QStringLiteral("device")}; value.devices={row};
    QVERIFY(validatePeripheralSnapshot(value));
    value.devices[0].percentage=1; QVERIFY(!validatePeripheralSnapshot(value));
    value.devices[0].percentageKnown=true; value.devices[0].level=BatteryLevel::None;
    QVERIFY(validatePeripheralSnapshot(value));
    value.devices[0].percentage=std::numeric_limits<double>::quiet_NaN(); QVERIFY(!validatePeripheralSnapshot(value));
    value.devices={row}; value.truncated=true; QVERIFY(!validatePeripheralSnapshot(value));
    value.omittedCount=1; value.availability=Availability::Degraded; QVERIFY(validatePeripheralSnapshot(value));
    value.devices.push_back(row); QVERIFY(!validatePeripheralSnapshot(value));
 }
};
QTEST_GUILESS_MAIN(PeripheralProtocolTest)
#include "tst_peripheral_protocol.moc"
