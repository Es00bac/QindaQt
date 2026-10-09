// SPDX-License-Identifier: GPL-3.0-or-later

// Stage identity is separate from DSP/PCM qualification. These contract rows
// catch accidental summation/clamping, cross-bus writes, codec drift and lost
// coordinator fields without opening any PipeWire connection.
#include "support/fake_audio_backend.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

namespace {
OperationRequest operation(OperationKind kind, const QString &id, double gain = 0.0)
{
    OperationRequest request;
    request.kind = kind;
    request.consoleId = id;
    request.gainDb = gain;
    return request;
}
bool apply(ConsoleModel &model, OperationRequest request)
{
    QString reason;
    return model.apply(request, &reason);
}
} // namespace

class ConsoleGainDeclarationTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void stagesRemainIndependentAcrossDestinations();
    void eachStageBottomStopSurvivesPersistence_data();
    void eachStageBottomStopSurvivesPersistence();
    void muteSoloAndPanKeepStagesAttached();
    void coordinatorForwardsTheThreeStages();
};

void ConsoleGainDeclarationTests::stagesRemainIndependentAcrossDestinations()
{
    ConsoleModel model;
    auto send = operation(OperationKind::SetStripSend, QStringLiteral("strip.hw.1"), -3.0);
    send.enabled = true;
    send.busIndex = 0;
    QVERIFY(apply(model, send));
    send.busIndex = 5;
    send.gainDb = -9.0;
    QVERIFY(apply(model, send));
    QVERIFY(apply(model, operation(OperationKind::SetStripGain,
                                  QStringLiteral("strip.hw.1"), 12.0)));
    QVERIFY(apply(model, operation(OperationKind::SetBusGain,
                                  QStringLiteral("bus.a1"), -6.0)));
    const auto routes = model.routing();
    QCOMPARE(routes.size(), 2);
    QCOMPARE(routes.at(0).gainDb, -3.0);
    QCOMPARE(routes.at(1).gainDb, -9.0);
    QCOMPARE(routes.at(0).stripGainDb, 12.0);
    QCOMPARE(routes.at(1).stripGainDb, 12.0);
    QCOMPARE(routes.at(0).busGainDb, -6.0);
    QCOMPARE(routes.at(1).busGainDb, 0.0);
}

void ConsoleGainDeclarationTests::eachStageBottomStopSurvivesPersistence_data()
{
    QTest::addColumn<int>("stage");
    QTest::newRow("strip-bottom") << 0;
    QTest::newRow("send-bottom") << 1;
    QTest::newRow("bus-bottom") << 2;
}

void ConsoleGainDeclarationTests::eachStageBottomStopSurvivesPersistence()
{
    QFETCH(int, stage);
    ConsoleModel model;
    auto send = operation(OperationKind::SetStripSend, QStringLiteral("strip.virtual.2"),
                          stage == 1 ? kMinGainDb : kMaxGainDb);
    send.enabled = true;
    send.busIndex = 6;
    QVERIFY(apply(model, send));
    QVERIFY(apply(model, operation(OperationKind::SetStripGain,
                                  QStringLiteral("strip.virtual.2"),
                                  stage == 0 ? kMinGainDb : kMaxGainDb)));
    QVERIFY(apply(model, operation(OperationKind::SetBusGain,
                                  QStringLiteral("bus.b2"),
                                  stage == 2 ? kMinGainDb : kMaxGainDb)));
    ConsoleModel restored;
    restored.loadJson(model.toJson());
    QVERIFY(restored.routing() == model.routing());
    const auto edge = restored.routing().constFirst();
    QCOMPARE(edge.stripGainDb, stage == 0 ? kMinGainDb : kMaxGainDb);
    QCOMPARE(edge.gainDb, stage == 1 ? kMinGainDb : kMaxGainDb);
    QCOMPARE(edge.busGainDb, stage == 2 ? kMinGainDb : kMaxGainDb);
}

void ConsoleGainDeclarationTests::muteSoloAndPanKeepStagesAttached()
{
    ConsoleModel model;
    for (const QString &id : {QStringLiteral("strip.hw.1"), QStringLiteral("strip.hw.2")}) {
        auto send = operation(OperationKind::SetStripSend, id, -3.0);
        send.enabled = true;
        send.busIndex = 0;
        QVERIFY(apply(model, send));
        QVERIFY(apply(model, operation(OperationKind::SetStripGain, id, 6.0)));
    }
    QVERIFY(apply(model, operation(OperationKind::SetBusGain, QStringLiteral("bus.a1"), 12.0)));
    auto pan = operation(OperationKind::SetStripPan, QStringLiteral("strip.hw.1"));
    pan.pan = -0.5;
    QVERIFY(apply(model, pan));
    auto solo = operation(OperationKind::SetStripSolo, QStringLiteral("strip.hw.2"));
    solo.enabled = true;
    QVERIFY(apply(model, solo));
    auto routes = model.routing();
    QCOMPARE(routes.size(), 2);
    QVERIFY(!routes.at(0).audible);
    QVERIFY(routes.at(1).audible);
    QCOMPARE(routes.at(0).pan, -0.5);
    QCOMPARE(routes.at(0).stripGainDb, 6.0);
    QCOMPARE(routes.at(0).busGainDb, 12.0);
    auto mute = operation(OperationKind::SetBusMute, QStringLiteral("bus.a1"));
    mute.muted = true;
    QVERIFY(apply(model, mute));
    routes = model.routing();
    QVERIFY(!routes.at(0).audible);
    QVERIFY(!routes.at(1).audible);
    QCOMPARE(routes.at(1).gainDb, -3.0);
    QCOMPARE(routes.at(1).stripGainDb, 6.0);
    QCOMPARE(routes.at(1).busGainDb, 12.0);
}

void ConsoleGainDeclarationTests::coordinatorForwardsTheThreeStages()
{
    QTemporaryDir files;
    QVERIFY(files.isValid());
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend, nullptr,
        files.filePath(QStringLiteral("presets")), files.filePath(QStringLiteral("macros.json")),
        files.filePath(QStringLiteral("vban.json")), files.filePath(QStringLiteral("latency.json")));
    coordinator.start();
    backend.publish(audioSnapshot());
    auto send = operation(OperationKind::SetStripSend, QStringLiteral("strip.hw.1"), -3.0);
    send.enabled = true;
    send.busIndex = 0;
    QCOMPARE(coordinator.submit(send).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(coordinator.submit(operation(OperationKind::SetStripGain,
        QStringLiteral("strip.hw.1"), -6.0)).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(coordinator.submit(operation(OperationKind::SetBusGain,
        QStringLiteral("bus.a1"), 12.0)).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.routing.size(), 1);
    const auto edge = backend.routing.constFirst();
    QCOMPARE(edge.gainDb, -3.0);
    QCOMPARE(edge.stripGainDb, -6.0);
    QCOMPARE(edge.busGainDb, 12.0);
    QCOMPARE(edge.source, (Handle{7, 20}));
    QCOMPARE(edge.target, (Handle{7, 10}));
}

QTEST_GUILESS_MAIN(ConsoleGainDeclarationTests)
#include "tst_console_gain_declaration.moc"
