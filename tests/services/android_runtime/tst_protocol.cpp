// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_fixtures.h"
#include <QTest>
using namespace RuntimeFixtures;
class ProtocolTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void snapshotAndOperationRoundTrips();
    void hostileLengthsTruncationTagsAndTrailingBytesRefuse();
    void invalidTypedValuesCannotPublish();
};
void ProtocolTest::snapshotAndOperationRoundTrips() {
    const auto value=snapshot();
    const auto encoded=encodePacket({PacketKind::Snapshot,key(),value});
    QVERIFY(!encoded.isEmpty());
    const auto decoded=decodePacket(encoded); QVERIFY(decoded);
    QCOMPARE(std::get<RuntimeSnapshot>(decoded->value),value);
    const auto operation=request(value);
    const auto launch=decodePacket(encodePacket({PacketKind::Launch,key(),operation})); QVERIFY(launch);
    const auto copy=std::get<LaunchRequest>(launch->value);
    QCOMPARE(copy.application,operation.application); QCOMPARE(copy.snapshotRevision,quint64(1));
    const LaunchResult result{operation.operationId,operation.attachmentId,operation.application,LaunchPhase::Completed,{}};
    const auto reply=decodePacket(encodePacket({PacketKind::Result,key(),result})); QVERIFY(reply);
    QCOMPARE(std::get<LaunchResult>(reply->value).phase,LaunchPhase::Completed);
    QVERIFY(decodePacket(encodePacket({PacketKind::Refresh,key(),{}})));
}
void ProtocolTest::hostileLengthsTruncationTagsAndTrailingBytesRefuse() {
    const auto valid=encodePacket({PacketKind::Snapshot,key(),snapshot()});
    for (qsizetype size=0;size<valid.size();++size) QVERIFY(!decodePacket(valid.left(size)));
    QVERIFY(!decodePacket(valid+QByteArray(1,'x')));
    auto invalid=valid; invalid[4]=char(99); QVERIFY(!decodePacket(invalid));
    invalid=valid; invalid[37]=char(255); invalid[38]=char(255); QVERIFY(!decodePacket(invalid));
    QVERIFY(!decodePacket(QByteArray(maxPacketBytes+1,'x')));
}
void ProtocolTest::invalidTypedValuesCannotPublish() {
    auto value=snapshot(); value.registrations.first().binding.registrationRevision=0;
    QVERIFY(encodePacket({PacketKind::Snapshot,key(),value}).isEmpty());
    auto operation=request(snapshot()); operation.operationId=QStringLiteral("arbitrary");
    QVERIFY(encodePacket({PacketKind::Launch,key(),operation}).isEmpty());
    QVERIFY(encodePacket({PacketKind::Refresh,QByteArray(31,'x'),{}}).isEmpty());
    value=snapshot(); value.message=QString(1025,QLatin1Char('x'));
    QVERIFY(encodePacket({PacketKind::Snapshot,key(),value}).isEmpty());
}
QTEST_GUILESS_MAIN(ProtocolTest)
#include "tst_protocol.moc"
