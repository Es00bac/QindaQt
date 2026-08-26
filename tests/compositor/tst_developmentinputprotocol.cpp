// SPDX-License-Identifier: GPL-3.0-or-later
#include "developmentinputprotocol.h"

#include "qindaqt/compositor/controllimits.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using QindaQt::Compositor::ControlLimits;

namespace QindaQt::Compositor::KWinIntegration {
namespace {

QByteArray request(const QJsonArray &events, int schemaVersion = 1)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), schemaVersion},
                                     {QStringLiteral("events"), events}})
        .toJson(QJsonDocument::Compact);
}

QJsonObject decode(const QByteArray &reply)
{
    return QJsonDocument::fromJson(reply).object();
}

QString failureCode(const QByteArray &reply)
{
    return decode(reply).value(QStringLiteral("failure")).toObject()
        .value(QStringLiteral("code")).toString();
}

QString failureMessage(const QByteArray &reply)
{
    return decode(reply).value(QStringLiteral("failure")).toObject()
        .value(QStringLiteral("message")).toString();
}

class RecordingSink final : public DevelopmentInputSink
{
public:
    bool isAvailable() const override
    {
        ++availabilityQueries;
        return available;
    }

    bool inject(const DevelopmentInputBatch &batch) override
    {
        ++injections;
        lastBatch = batch;
        return succeeds;
    }

    mutable int availabilityQueries = 0;
    int injections = 0;
    bool available = true;
    bool succeeds = true;
    DevelopmentInputBatch lastBatch;
};

} // namespace

class DevelopmentInputProtocolTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesAndDispatchesAllowlistedEvents();
    void rejectsMalformedAndLimitFailures();
    void checksProductionGateBeforePayloadInspection();
    void reportsUnavailableSinkAndCapabilities();
};

void DevelopmentInputProtocolTest::parsesAndDispatchesAllowlistedEvents()
{
    const auto payload = request(
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
                     {QStringLiteral("x"), -120.5},
                     {QStringLiteral("y"), 1440.25}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("key")},
                     {QStringLiteral("key"), QStringLiteral("left-meta")},
                     {QStringLiteral("pressed"), true}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("key")},
                     {QStringLiteral("key"), QStringLiteral("left-shift")},
                     {QStringLiteral("pressed"), false}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("key")},
                     {QStringLiteral("key"), QStringLiteral("down")},
                     {QStringLiteral("pressed"), true}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("key")},
                     {QStringLiteral("key"), QStringLiteral("enter")},
                     {QStringLiteral("pressed"), false}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("button")},
                     {QStringLiteral("button"), QStringLiteral("left")},
                     {QStringLiteral("pressed"), true}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("button")},
                     {QStringLiteral("button"), QStringLiteral("right")},
                     {QStringLiteral("pressed"), false}}});

    DevelopmentInputFailure failure;
    const auto parsed = DevelopmentInputCodec::parse(payload, &failure);
    QVERIFY2(parsed.has_value(), qPrintable(failure.message));
    QCOMPARE(parsed->events.size(), 7);
    QCOMPARE(parsed->events.at(0).position, QPointF(-120.5, 1440.25));
    QCOMPARE(parsed->events.at(1).key, DevelopmentInputKey::LeftMeta);
    QVERIFY(parsed->events.at(1).pressed);
    QCOMPARE(parsed->events.at(2).key, DevelopmentInputKey::LeftShift);
    QVERIFY(!parsed->events.at(2).pressed);
    QCOMPARE(parsed->events.at(3).key, DevelopmentInputKey::Down);
    QVERIFY(parsed->events.at(3).pressed);
    QCOMPARE(parsed->events.at(4).key, DevelopmentInputKey::Enter);
    QVERIFY(!parsed->events.at(4).pressed);
    QCOMPARE(parsed->events.at(5).button, DevelopmentInputButton::Left);
    QCOMPARE(parsed->events.at(6).button, DevelopmentInputButton::Right);

    RecordingSink sink;
    DevelopmentInputController controller(true, &sink);
    const auto reply = decode(controller.injectTestInput(payload));
    QCOMPARE(reply.value(QStringLiteral("status")).toString(), QStringLiteral("injected"));
    QCOMPARE(reply.value(QStringLiteral("eventCount")).toInteger(), 7);
    QCOMPARE(reply.value(QStringLiteral("deviceId")).toString(),
             QStringLiteral("qindaqt-development-input"));
    QCOMPARE(sink.injections, 1);
    QCOMPARE(sink.lastBatch.events.size(), 7);
}

void DevelopmentInputProtocolTest::rejectsMalformedAndLimitFailures()
{
    DevelopmentInputFailure failure;
    QVERIFY(!DevelopmentInputCodec::parse(QByteArrayLiteral("not-json"), &failure));
    QCOMPARE(failure.code, QStringLiteral("malformed-input-request"));
    QCOMPARE(failure.message, QStringLiteral("input request does not match schema version 1"));

    const auto unknownKey = request(
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("key")},
                     {QStringLiteral("key"), QStringLiteral("right-meta")},
                     {QStringLiteral("pressed"), true}}});
    QVERIFY(!DevelopmentInputCodec::parse(unknownKey, &failure));
    QCOMPARE(failure.code, QStringLiteral("malformed-input-request"));

    const auto unknownButton = request(
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("button")},
                     {QStringLiteral("button"), QStringLiteral("middle")},
                     {QStringLiteral("pressed"), true}}});
    QVERIFY(!DevelopmentInputCodec::parse(unknownButton, &failure));
    QCOMPARE(failure.code, QStringLiteral("malformed-input-request"));

    const auto unbounded = request(
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
                     {QStringLiteral("x"),
                      DevelopmentInputCodec::MaxLogicalCoordinateMagnitude + 1.0},
                     {QStringLiteral("y"), 0.0}}});
    QVERIFY(!DevelopmentInputCodec::parse(unbounded, &failure));

    auto extraField = QJsonDocument::fromJson(request(
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("button")},
                     {QStringLiteral("button"), QStringLiteral("left")},
                     {QStringLiteral("pressed"), true}}}))
                          .object();
    extraField.insert(QStringLiteral("ignored"), true);
    QVERIFY(!DevelopmentInputCodec::parse(
        QJsonDocument(extraField).toJson(QJsonDocument::Compact), &failure));

    QJsonArray tooMany;
    for (qsizetype index = 0; index <= DevelopmentInputCodec::MaxEvents; ++index) {
        tooMany.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("key")},
                                   {QStringLiteral("key"), QStringLiteral("left-shift")},
                                   {QStringLiteral("pressed"), index % 2 == 0}});
    }
    QVERIFY(!DevelopmentInputCodec::parse(request(tooMany), &failure));
    QCOMPARE(failure.code, QStringLiteral("request-too-large"));
    QCOMPARE(failure.message, QStringLiteral("input request may contain at most 64 events"));

    QVERIFY(!DevelopmentInputCodec::parse(
        QByteArray(ControlLimits::MaxRequestBytes + 1, 'x'), &failure));
    QCOMPARE(failure.code, QStringLiteral("request-too-large"));
    QCOMPARE(failure.message,
             QStringLiteral("input request exceeds the 262144-byte limit"));
}

void DevelopmentInputProtocolTest::checksProductionGateBeforePayloadInspection()
{
    RecordingSink sink;
    DevelopmentInputController disabled(false, &sink);

    const auto malformed = disabled.injectTestInput(QByteArrayLiteral("not-json"));
    QCOMPARE(failureCode(malformed), QStringLiteral("control-disabled"));
    QCOMPARE(failureMessage(malformed),
             QStringLiteral("external compositor mutations are disabled"));

    const auto oversized = disabled.injectTestInput(
        QByteArray(ControlLimits::MaxRequestBytes + 1, 'x'));
    QCOMPARE(failureCode(oversized), QStringLiteral("control-disabled"));
    QCOMPARE(failureMessage(oversized), failureMessage(malformed));
    QCOMPARE(sink.availabilityQueries, 0);
    QCOMPARE(sink.injections, 0);

    // Production constructs the controller with no provider. Its response is
    // still the disabled gate, including for a malformed payload.
    DevelopmentInputController production(false, nullptr);
    QCOMPARE(failureCode(production.injectTestInput(QByteArrayLiteral("{"))),
             QStringLiteral("control-disabled"));
    QVERIFY(!production.capabilities().value(QStringLiteral("available")).toBool());
}

void DevelopmentInputProtocolTest::reportsUnavailableSinkAndCapabilities()
{
    const auto valid = request(
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("button")},
                     {QStringLiteral("button"), QStringLiteral("left")},
                     {QStringLiteral("pressed"), false}}});
    DevelopmentInputController missing(true, nullptr);
    const auto reply = missing.injectTestInput(valid);
    QCOMPARE(failureCode(reply), QStringLiteral("input-injection-unavailable"));
    QCOMPARE(failureMessage(reply),
             QStringLiteral("development input injector is unavailable"));

    RecordingSink sink;
    DevelopmentInputController controller(true, &sink);
    const auto capabilities = controller.capabilities();
    QVERIFY(capabilities.value(QStringLiteral("enabled")).toBool());
    QVERIFY(capabilities.value(QStringLiteral("available")).toBool());
    QCOMPARE(capabilities.value(QStringLiteral("schemaVersion")).toInteger(), 1);
    QCOMPARE(capabilities.value(QStringLiteral("maxEvents")).toInteger(), 64);
    QCOMPARE(capabilities.value(QStringLiteral("deviceId")).toString(),
             QStringLiteral("qindaqt-development-input"));
    QCOMPARE(capabilities.value(QStringLiteral("eventTypes")).toArray(),
             QJsonArray({QStringLiteral("pointer-absolute"), QStringLiteral("key"),
                         QStringLiteral("button")}));

    sink.succeeds = false;
    QCOMPARE(failureCode(controller.injectTestInput(valid)),
             QStringLiteral("input-injection-unavailable"));
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::DevelopmentInputProtocolTest)

#include "tst_developmentinputprotocol.moc"
