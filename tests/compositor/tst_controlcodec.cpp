// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/controlcodec.h"

#include "testfixtures.h"

#include <QJsonArray>
#include <QtTest>

using namespace QindaQt::Compositor;

class ControlCodecTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesVersionedRequest();
    void rejectsUnsafeRevisionAndMalformedOperations();
    void serializesStableReplyAndCapabilities();
};

void ControlCodecTest::parsesVersionedRequest()
{
    const QJsonArray operations{
        QJsonObject{{QStringLiteral("type"), QStringLiteral("detach-window")},
                    {QStringLiteral("windowId"), QStringLiteral("window-a")}},
    };
    ControlFailure failure;
    const auto parsed = ControlCodec::parseRequest(Test::requestJson(42, operations), &failure);
    QVERIFY2(parsed.has_value(), qPrintable(failure.message));
    QCOMPARE(parsed->protocol, ProtocolVersion({1, 0}));
    QCOMPARE(parsed->transactionId, QStringLiteral("transaction-a"));
    QCOMPARE(parsed->containerId, QStringLiteral("container-a"));
    QCOMPARE(parsed->expectedRevision, quint64(42));
    QCOMPARE(parsed->operations.size(), 1);
}

void ControlCodecTest::rejectsUnsafeRevisionAndMalformedOperations()
{
    auto numericRevision = Test::requestJson(0, {QJsonObject{{QStringLiteral("type"),
                                                              QStringLiteral("detach-window")}}});
    numericRevision.insert(QStringLiteral("expectedRevision"), 9.007199254740993e15);
    ControlFailure failure;
    QVERIFY(!ControlCodec::parseRequest(numericRevision, &failure));
    QCOMPARE(failure.code, QStringLiteral("malformed-request"));

    auto signedRevision = Test::requestJson(0, {QJsonObject{{QStringLiteral("type"),
                                                             QStringLiteral("detach-window")}}});
    signedRevision.insert(QStringLiteral("expectedRevision"), QStringLiteral("+1"));
    QVERIFY(!ControlCodec::parseRequest(signedRevision, &failure));

    auto malformedOperation = Test::requestJson(0, {QStringLiteral("not-an-object")});
    QVERIFY(!ControlCodec::parseRequest(malformedOperation, &failure));
    QCOMPARE(failure.operationIndex, 0);

    auto emptyBatch = Test::requestJson(0, {});
    QVERIFY(!ControlCodec::parseRequest(emptyBatch, &failure));

    auto longIdentifier = Test::requestJson(
        0, {QJsonObject{{QStringLiteral("type"), QStringLiteral("detach-window")}}});
    longIdentifier.insert(QStringLiteral("transactionId"), QString(257, QLatin1Char('x')));
    QVERIFY(!ControlCodec::parseRequest(longIdentifier, &failure));
    QCOMPARE(failure.code, QStringLiteral("request-too-large"));

    QJsonArray oversizedBatch;
    for (int index = 0; index < 129; ++index) {
        oversizedBatch.append(QJsonObject{{QStringLiteral("type"),
                                           QStringLiteral("detach-window")}});
    }
    QVERIFY(!ControlCodec::parseRequest(Test::requestJson(0, oversizedBatch), &failure));
    QCOMPARE(failure.code, QStringLiteral("request-too-large"));
}

void ControlCodecTest::serializesStableReplyAndCapabilities()
{
    const ControlReply reply{{},
                             QStringLiteral("transaction-a"),
                             QStringLiteral("container-a"),
                             ReplyStatus::Conflict,
                             9007199254740993ULL,
                             {},
                             {QStringLiteral("revision-conflict"),
                              QStringLiteral("retry against the current revision"),
                              -1}};
    const auto json = ControlCodec::replyToJson(reply);
    QCOMPARE(json.value(QStringLiteral("status")).toString(), QStringLiteral("conflict"));
    QCOMPARE(json.value(QStringLiteral("revision")).toString(),
             QStringLiteral("9007199254740993"));
    QCOMPARE(json.value(QStringLiteral("failure")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("revision-conflict"));

    const auto capabilities = ControlCodec::capabilities();
    QCOMPARE(capabilities.value(QStringLiteral("interface")).toString(),
             QStringLiteral("org.qindaqt.Compositor1"));
    QVERIFY(capabilities.value(QStringLiteral("transactional")).toBool());
    QCOMPARE(capabilities.value(QStringLiteral("protocol")).toObject()
                 .value(QStringLiteral("minor")).toInt(), 1);
    QVERIFY(capabilities.value(QStringLiteral("operations")).toArray()
                .contains(QStringLiteral("split-window")));
    QCOMPARE(capabilities.value(QStringLiteral("limits")).toObject()
                 .value(QStringLiteral("maxOperations")).toInteger(),
             128);
}

QTEST_GUILESS_MAIN(ControlCodecTest)
#include "tst_controlcodec.moc"
