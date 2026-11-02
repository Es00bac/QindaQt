// SPDX-License-Identifier: GPL-3.0-or-later
// Runs every case in tests/link/vectors/{frames,pairing}-v1.json against the
// C++ codec. The Java codec runs the same files.
#include <qindaqt/services/link_protocol/frame_codec.h>
#include <qindaqt/services/link_protocol/pairing_math.h>

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtTest/QtTest>

using namespace QindaQt::Link;

namespace {

QJsonObject loadVector(const char *name)
{
    QFile f(QStringLiteral(QINDAQT_LINK_VECTOR_DIR "/") + QLatin1String(name));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

Envelope fromJson(const QJsonObject &o)
{
    Envelope e;
    e.t = o.value(QStringLiteral("t")).toString();
    e.id = qint64(o.value(QStringLiteral("id")).toDouble());
    e.re = qint64(o.value(QStringLiteral("re")).toDouble());
    e.b = o.value(QStringLiteral("b")).toObject();
    return e;
}

DecodeStatus errorStatus(const QString &name)
{
    if (name == QLatin1String("bad-length")) return DecodeStatus::BadLength;
    if (name == QLatin1String("bad-payload")) return DecodeStatus::BadPayload;
    return DecodeStatus::BadEnvelope;
}

} // namespace

class LinkVectorsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void frames()
    {
        const QJsonObject v = loadVector("frames-v1.json");
        QVERIFY(!v.isEmpty());
        int ran = 0;
        for (const QJsonValue &cv : v.value(QStringLiteral("decode")).toArray()) {
            const QJsonObject c = cv.toObject();
            const QString name = c.value(QStringLiteral("name")).toString();
            const QJsonObject expect = c.value(QStringLiteral("expect")).toObject();
            const QByteArray bytes = QByteArray::fromHex(c.value(QStringLiteral("hex")).toString().toLatin1());
            // Whole buffer and byte-at-a-time must agree.
            for (const bool trickle : {false, true}) {
                FrameDecoder d;
                DecodeResult r;
                if (trickle) {
                    for (const char byte : bytes) {
                        d.append(QByteArray(1, byte));
                        r = d.next();
                        if (r.status != DecodeStatus::NeedsMore) break;
                    }
                } else {
                    d.append(bytes);
                    r = d.next();
                }
                if (expect.contains(QStringLiteral("envelope"))) {
                    QVERIFY2(r.status == DecodeStatus::Frame, qPrintable(name));
                    QVERIFY2(r.envelope == fromJson(expect.value(QStringLiteral("envelope")).toObject()),
                             qPrintable(name));
                } else if (expect.contains(QStringLiteral("error"))) {
                    QVERIFY2(r.status == errorStatus(expect.value(QStringLiteral("error")).toString()),
                             qPrintable(name));
                } else {
                    QVERIFY2(r.status == DecodeStatus::NeedsMore, qPrintable(name));
                }
            }
            ++ran;
        }
        for (const QJsonValue &cv : v.value(QStringLiteral("stream")).toArray()) {
            const QJsonObject c = cv.toObject();
            FrameDecoder d;
            d.append(QByteArray::fromHex(c.value(QStringLiteral("hex")).toString().toLatin1()));
            for (const QJsonValue &ev : c.value(QStringLiteral("expect")).toObject()
                                            .value(QStringLiteral("envelopes")).toArray()) {
                const DecodeResult r = d.next();
                QVERIFY(r.status == DecodeStatus::Frame);
                QVERIFY(r.envelope == fromJson(ev.toObject()));
            }
            QVERIFY(d.next().status == DecodeStatus::NeedsMore);
            ++ran;
        }
        for (const QJsonValue &cv : v.value(QStringLiteral("encode")).toArray()) {
            const QJsonObject c = cv.toObject();
            const Envelope e = fromJson(c.value(QStringLiteral("envelope")).toObject());
            FrameDecoder d;
            d.append(encodeFrame(e));
            const DecodeResult r = d.next();
            QVERIFY2(r.status == DecodeStatus::Frame, qPrintable(c.value(QStringLiteral("name")).toString()));
            QVERIFY(r.envelope == e);
            ++ran;
        }
        qInfo("frames-v1: %d cases", ran);
        QVERIFY(ran >= 40);
    }

    void pairing()
    {
        const QJsonObject v = loadVector("pairing-v1.json");
        int ran = 0;
        for (const QJsonValue &cv : v.value(QStringLiteral("cases")).toArray()) {
            const QJsonObject c = cv.toObject();
            auto hex = [&](const char *k) {
                return QByteArray::fromHex(c.value(QLatin1String(k)).toString().toLatin1());
            };
            QCOMPARE(pairingCommitment(hex("nd"), hex("spkiD"), hex("spkiP")).toHex(),
                     c.value(QStringLiteral("commitment")).toString().toLatin1());
            QCOMPARE(pairingCode(hex("spkiD"), hex("spkiP"), hex("nd"), hex("np")),
                     c.value(QStringLiteral("code")).toString());
            ++ran;
        }
        qInfo("pairing-v1: %d cases", ran);
        QCOMPARE(ran, 3);
    }

    void encoderRejectsOversize()
    {
        Envelope e;
        e.t = QStringLiteral("x");
        e.b.insert(QStringLiteral("blob"), QString(5 * 1024 * 1024, QLatin1Char('a')));
        QVERIFY(encodeFrame(e).isEmpty());
    }
};

QTEST_GUILESS_MAIN(LinkVectorsTest)
#include "tst_link_vectors.moc"
