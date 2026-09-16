// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0185: the VBAN header, encoded and decoded by the same pure codec. A
// packet from the reference console must decode; anything else must not.

#include "../../../src/services/audio_service/src/vban_packet_p.h"

#include <QtTest>

using namespace QindaQt::Audio::Vban;

class VbanPacketTests final : public QObject
{
    Q_OBJECT
private slots:
    void aHeaderRoundTrips();
    void theReferenceLayoutIsHonoured();
    void foreignAndMalformedPacketsAreRefused();
};

void VbanPacketTests::aHeaderRoundTrips()
{
    Header header;
    header.samplesPerFrame = 256;
    header.channels = 2;
    header.dataType = kFormatPcm16;
    header.streamName = QStringLiteral("Stream1");
    header.frameCounter = 0x01020304;
    QByteArray packet = encodeHeader(header);
    QCOMPARE(packet.size(), kHeaderBytes);
    packet.append(QByteArray(256 * 2 * 2, '\0'));
    const auto decoded = decodeHeader(packet);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->samplesPerFrame, 256);
    QCOMPARE(decoded->channels, 2);
    QCOMPARE(decoded->dataType, kFormatPcm16);
    QCOMPARE(decoded->streamName, QStringLiteral("Stream1"));
    QCOMPARE(decoded->frameCounter, 0x01020304u);
}

void VbanPacketTests::theReferenceLayoutIsHonoured()
{
    Header header;
    header.samplesPerFrame = 64;
    header.channels = 1;
    header.streamName = QStringLiteral("A-very-long-stream-name-here");
    header.frameCounter = 7;
    const QByteArray packet = encodeHeader(header);
    // "VBAN", then rate index 3 (48 kHz) with sub-protocol 0, then counts - 1,
    // then PCM16 with codec 0, then the name in 16 bytes, then the counter LE.
    QCOMPARE(packet.left(4), QByteArray("VBAN"));
    QCOMPARE(static_cast<uint8_t>(packet[4]), 3);
    QCOMPARE(static_cast<uint8_t>(packet[5]), 63);
    QCOMPARE(static_cast<uint8_t>(packet[6]), 0);
    QCOMPARE(static_cast<uint8_t>(packet[7]), 1);
    QCOMPARE(packet.mid(8, 16), QByteArray("A-very-long-stre"));
    QCOMPARE(static_cast<uint8_t>(packet[24]), 7);
    QCOMPARE(static_cast<uint8_t>(packet[27]), 0);
}

void VbanPacketTests::foreignAndMalformedPacketsAreRefused()
{
    Header header;
    header.samplesPerFrame = 8;
    header.channels = 2;
    QByteArray good = encodeHeader(header);
    good.append(QByteArray(8 * 2 * 2, '\0'));
    QVERIFY(decodeHeader(good).has_value());
    // Wrong magic, wrong length, a rate we do not speak, a type we do not take.
    QByteArray magic = good;
    magic[0] = 'X';
    QVERIFY(!decodeHeader(magic).has_value());
    QVERIFY(!decodeHeader(good.left(good.size() - 1)).has_value());
    QByteArray rate = good;
    rate[4] = static_cast<char>(16);   // 44.1 kHz
    QVERIFY(!decodeHeader(rate).has_value());
    QByteArray type = good;
    type[7] = static_cast<char>(3);    // PCM32 int: not accepted
    QVERIFY(!decodeHeader(type).has_value());
    QVERIFY(!decodeHeader(QByteArray()).has_value());
}

QTEST_APPLESS_MAIN(VbanPacketTests)
#include "tst_vban_packet.moc"
