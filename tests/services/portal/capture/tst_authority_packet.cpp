// SPDX-License-Identifier: GPL-3.0-or-later
#include "authority/packet.h"
#include <QtTest>
#include <array>
using namespace QindaQt::Services::Portal::CaptureAuthority;
class AuthorityPacketTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void normativeHelloBytes() {
        const auto golden = QByteArray::fromHex("5143433101000100080706050403020100000000000000000000000000000000");
        QCOMPARE(encode({Wire::Message::Hello, 0x0102030405060708ULL, 0, {}, 0}), golden);
        const auto packet = decode(golden); QVERIFY(packet); QCOMPARE(packet->generation, 0x0102030405060708ULL); QCOMPARE(packet->job, 0ULL);
    }
    void rejectMalformedEnvelope() {
        const auto good = encode({Wire::Message::Hello, 1, 0, {}, 0});
        for (const int byte : {0, 4, 6, 24, 28, 30}) { auto corrupt = good; corrupt[byte] = '\xff'; QVERIFY(!decode(corrupt)); }
        QVERIFY(!decode(good.left(31))); QVERIFY(!decode(good+'\0')); QVERIFY(!decode(QByteArray(16417, '\0')));
        auto zeroGeneration = good; zeroGeneration[8] = '\0'; QVERIFY(!decode(zeroGeneration));
    }
    void exactNameLengthsAndSyntax() {
        QCOMPARE(readyPayload(":1.42"), QByteArray::fromHex("05003a312e3432"));
        QVERIFY(encode({Wire::Message::Ready, 1, 0, readyPayload(":1.42"), 0}).size() > 32);
        for (const auto &name : {QString{}, QString("org.test.Caller"), QString(":1"), QString(":1..2"), QString::fromUtf8(":é.1"), QString(":1.")+QString(254, 'a')}) QVERIFY(readyPayload(name).isEmpty());
        auto payload = readyPayload(":1.42"); payload[0] = '\x06'; QVERIFY(encode({Wire::Message::Ready, 1, 0, payload, 0}).isEmpty());
        QVERIFY(!validUniqueName(QByteArray(":1.2\0x", 7)));
    }
    void scopesAndCallerIdentityAreBounded() {
        const auto payload = startPayload(Wire::Scope::Screenshot, ":1.2", ":1.3");
        QCOMPARE(payload, QByteArray::fromHex("01000400040000003a312e323a312e33"));
        QVERIFY(decode(encode({Wire::Message::StartJob, 1, 1, payload, 0})));
        QVERIFY(startPayload(static_cast<Wire::Scope>(3), ":1.2", ":1.3").isEmpty());
        auto reserved = payload; reserved[6] = '\x01'; QVERIFY(encode({Wire::Message::StartJob, 1, 1, reserved, 0}).isEmpty());
        QVERIFY(encode({Wire::Message::StartJob, 1, 0, payload, 0}).isEmpty());
        QVERIFY(encode({Wire::Message::StartJob, 1, 1, payload+'\0', 0}).isEmpty());
    }
    void descriptorsAndTerminalPayloads() {
        QVERIFY(decode(encode({Wire::Message::JobStarted, 1, 1, {}, 2})));
        for (const quint16 count : std::array<quint16, 3>{0, 1, 3}) QVERIFY(encode({Wire::Message::JobStarted, 1, 1, {}, count}).isEmpty());
        QVERIFY(encode({Wire::Message::CaptureReady, 1, 1, {}, 1}).isEmpty());
        QVERIFY(encode({Wire::Message::RevokeJob, 1, 1, "extra", 0}).isEmpty());
        QVERIFY(decode(encode({Wire::Message::Error, 1, 1, QByteArray::fromHex("0400"), 0})));
        QVERIFY(encode({Wire::Message::Error, 1, 1, QByteArray::fromHex("0800"), 0}).isEmpty());
    }
};
QTEST_GUILESS_MAIN(AuthorityPacketTest)
#include "tst_authority_packet.moc"
