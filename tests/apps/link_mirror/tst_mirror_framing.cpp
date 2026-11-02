// SPDX-License-Identifier: GPL-3.0-or-later
// Framing and message bodies of QindaLink mirror protocol 1 (docs/wiki/reference/qindalink-mirror-v1.md).

#include "qindaqt/link_mirror/mirror_messages.h"
#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace qindaqt::link_mirror;

namespace {

QByteArray header(quint8 channel, quint8 type, quint32 length, quint8 reserved = 0)
{
    QByteArray bytes(8, '\0');
    bytes[0] = static_cast<char>(channel);
    bytes[1] = static_cast<char>(type);
    bytes[2] = static_cast<char>(reserved);
    bytes[4] = static_cast<char>((length >> 24) & 0xffU);
    bytes[5] = static_cast<char>((length >> 16) & 0xffU);
    bytes[6] = static_cast<char>((length >> 8) & 0xffU);
    bytes[7] = static_cast<char>(length & 0xffU);
    return bytes;
}

QJsonObject body(const QByteArray &frame)
{
    return QJsonDocument::fromJson(frame.mid(kHeaderSize)).object();
}

} // namespace

class MirrorFramingTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void encodesTheDocumentedHeader()
    {
        const QByteArray frame = encodeFrame(Channel::Control, control::Hello, "{}");
        QCOMPARE(frame.toHex(), QByteArray("0101000000000002") + QByteArray("{}").toHex());
    }

    void rejectsEmptyAndOversizedPayloads()
    {
        QVERIFY(encodeFrame(Channel::Control, control::Sync, "").isEmpty());
        QVERIFY(encodeFrame(Channel::Control, control::Text, QByteArray(kMaxControlPayload + 1, 'x')).isEmpty());
        QVERIFY(!encodeFrame(Channel::Video, video::Packet, QByteArray(kMaxControlPayload + 1, 'x')).isEmpty());
    }

    void decodesFramesFedOneByteAtATime()
    {
        const QByteArray stream = encodeFrame(Channel::Control, control::Ready, "{\"version\":1}")
                                  + encodeFrame(Channel::Inventory, inventory::End, "{\"count\":0}");
        FrameDecoder decoder;
        QList<Frame> frames;
        Frame frame;
        for (const char byte : stream) {
            decoder.append(QByteArrayView(&byte, 1));
            while (decoder.next(frame) == DecodeStatus::Frame) {
                frames.append(frame);
            }
        }
        QCOMPARE(frames.size(), 2);
        QCOMPARE(frames.at(0).channel, Channel::Control);
        QCOMPARE(frames.at(0).type, control::Ready);
        QCOMPARE(frames.at(1).channel, Channel::Inventory);
        QCOMPARE(frames.at(1).payload, QByteArray("{\"count\":0}"));
        QCOMPARE(decoder.buffered(), 0);
    }

    void classifiesMalformedHeaders_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<DecodeStatus>("status");
        QTest::newRow("unknown channel") << header(0x04, 1, 2) << DecodeStatus::BadChannel;
        QTest::newRow("channel zero") << header(0x00, 1, 2) << DecodeStatus::BadChannel;
        QTest::newRow("reserved set") << header(0x01, 1, 2, 1) << DecodeStatus::BadReserved;
        QTest::newRow("zero length") << header(0x01, 1, 0) << DecodeStatus::BadLength;
        QTest::newRow("control over 1 MiB") << header(0x01, 1, kMaxControlPayload + 1) << DecodeStatus::BadLength;
        QTest::newRow("inventory over 1 MiB") << header(0x03, 1, kMaxControlPayload + 1) << DecodeStatus::BadLength;
        QTest::newRow("video over 16 MiB") << header(0x02, 2, kMaxVideoPayload + 1) << DecodeStatus::BadLength;
        QTest::newRow("video at 16 MiB waits") << header(0x02, 2, kMaxVideoPayload) << DecodeStatus::NeedsMore;
        QTest::newRow("short header") << header(0x01, 1, 2).left(7) << DecodeStatus::NeedsMore;
    }

    void classifiesMalformedHeaders()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(DecodeStatus, status);
        FrameDecoder decoder;
        decoder.append(bytes);
        Frame frame;
        QCOMPARE(decoder.next(frame), status);
        QCOMPARE(decoder.failed(), status != DecodeStatus::NeedsMore);
    }

    void errorsAreSticky()
    {
        FrameDecoder decoder;
        decoder.append(header(0x09, 1, 2));
        Frame frame;
        QCOMPARE(decoder.next(frame), DecodeStatus::BadChannel);
        decoder.append(encodeFrame(Channel::Control, control::Ready, "{}"));
        QCOMPARE(decoder.next(frame), DecodeStatus::BadChannel);
    }

    void roundTripsVideoPackets()
    {
        VideoPacket packet;
        packet.key = true;
        packet.ptsUs = 0x0102030405060708LL;
        packet.data = QByteArray::fromHex("00000001654d");
        const QByteArray payload = encodeVideoPacket(packet);
        QCOMPARE(payload.left(9).toHex(), QByteArray("020102030405060708"));
        const auto parsed = parseVideoPacket(payload);
        QVERIFY(parsed);
        QVERIFY(parsed->key);
        QVERIFY(!parsed->config);
        QCOMPARE(parsed->ptsUs, packet.ptsUs);
        QCOMPARE(parsed->data, packet.data);
    }

    void rejectsBadVideoPackets()
    {
        QVERIFY(!parseVideoPacket(QByteArray(9, '\0'))); // header only, no bitstream
        QByteArray reserved = QByteArray(10, '\0');
        reserved[0] = 0x04;
        QVERIFY(!parseVideoPacket(reserved));
        QByteArray config = QByteArray(10, '\0');
        config[0] = 0x01;
        QVERIFY(parseVideoPacket(config)->config);
    }

    void buildsControlBodies()
    {
        const QByteArray hello = helloFrame(QStringLiteral("00112233445566778899aabbccddeeff"));
        QCOMPARE(hello.at(0), char(0x01));
        QCOMPARE(hello.at(1), char(control::Hello));
        QCOMPARE(body(hello).value(QStringLiteral("version")).toInt(), kProtocolVersion);
        StartRequest request;
        request.packageName = QStringLiteral("com.example.clock");
        request.size = QSize(720, 1440);
        request.dpi = 240;
        const QJsonObject start = body(startFrame(request));
        QCOMPARE(start.value(QStringLiteral("package")).toString(), request.packageName);
        QCOMPARE(start.value(QStringLiteral("width")).toInt(), 720);
        QCOMPARE(start.value(QStringLiteral("codec")).toString(), QStringLiteral("h264"));
        const QJsonObject touch = body(touchFrame(TouchAction::Up, 0, QPointF(10.5, 20), 0.0));
        QCOMPARE(touch.value(QStringLiteral("action")).toString(), QStringLiteral("up"));
        QCOMPARE(touch.value(QStringLiteral("x")).toDouble(), 10.5);
        const QByteArray list = inventoryListFrame({QStringLiteral("com.example.clock")}, 96);
        QCOMPARE(list.at(0), char(0x03));
        QCOMPARE(body(list).value(QStringLiteral("packages")).toArray().size(), 1);
    }

    void parsesServerBodies()
    {
        const auto display = parseDisplay(R"({"displayId":7,"width":720,"height":1440,"dpi":240,"flags":97609})");
        QVERIFY(display);
        QCOMPARE(display->size, QSize(720, 1440));
        QVERIFY(!parseDisplay(R"({"displayId":7,"width":0,"height":1440})"));
        QVERIFY(!parseDisplay(R"({"displayId":"7","width":720,"height":1440})"));
        QVERIFY(parseVideoFormat(R"({"codec":"hevc","width":8,"height":8})"));
        QVERIFY(!parseVideoFormat(R"({"codec":"vp9","width":8,"height":8})"));
        const auto error = parseError(R"({"code":"no-launcher-activity","message":"m","fatal":false})");
        QVERIFY(error && !error->fatal);
        QVERIFY(!parseReady("not json"));
        QCOMPARE(parseInventoryEnd(R"({"count":3})").value_or(-1), 3);
    }

    void decodesAppIcons()
    {
        QImage icon(4, 4, QImage::Format_ARGB32);
        icon.fill(Qt::red);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        QVERIFY(icon.save(&buffer, "PNG"));
        const QJsonObject json{{QStringLiteral("package"), QStringLiteral("com.example.clock")},
                               {QStringLiteral("label"), QStringLiteral("Clock")},
                               {QStringLiteral("png"), QString::fromLatin1(png.toBase64())}};
        const auto app = parseApp(QJsonDocument(json).toJson(QJsonDocument::Compact));
        QVERIFY(app);
        QCOMPARE(app->label, QStringLiteral("Clock"));
        QCOMPARE(app->icon.size(), QSize(4, 4));
    }
};

QTEST_GUILESS_MAIN(MirrorFramingTest)
#include "tst_mirror_framing.moc"
