// SPDX-License-Identifier: GPL-3.0-or-later

// This fixture deliberately uses only the immutable 5b7 public operation API.
// It can run against that original production cohort: a display/model echo
// without a changed backend declaration must fail the same executable rows.
#include "support/fake_audio_backend.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

class ConsoleGainRepublicationTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void faderChangesReachTheExistingBackendBoundary_data();
    void faderChangesReachTheExistingBackendBoundary();
};

void ConsoleGainRepublicationTests::faderChangesReachTheExistingBackendBoundary_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<double>("gainDb");
    for (const double gain : {-60.0, -6.0, 12.0}) {
        const QByteArray value = QByteArray::number(gain);
        QTest::newRow(("strip-" + value).constData())
            << static_cast<int>(OperationKind::SetStripGain) << gain;
        QTest::newRow(("bus-" + value).constData())
            << static_cast<int>(OperationKind::SetBusGain) << gain;
    }
}

void ConsoleGainRepublicationTests::faderChangesReachTheExistingBackendBoundary()
{
    QFETCH(int, kind);
    QFETCH(double, gainDb);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend, nullptr,
        files.filePath(QStringLiteral("presets")),
        files.filePath(QStringLiteral("macros.json")),
        files.filePath(QStringLiteral("vban.json")),
        files.filePath(QStringLiteral("latency.json")));
    coordinator.start();
    backend.publish(audioSnapshot());

    OperationRequest send;
    send.kind = OperationKind::SetStripSend;
    send.consoleId = QStringLiteral("strip.hw.1");
    send.busIndex = 0;
    send.enabled = true;
    send.gainDb = -3.0;
    QCOMPARE(coordinator.submit(send).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.routing.size(), 1);
    const BackendRoutingEdge initial = backend.routing.constFirst();
    QVERIFY(initial.source.isValid());
    QVERIFY(initial.target.isValid());

    OperationRequest fader;
    fader.kind = static_cast<OperationKind>(kind);
    fader.consoleId = fader.kind == OperationKind::SetStripGain
        ? QStringLiteral("strip.hw.1") : QStringLiteral("bus.a1");
    fader.gainDb = gainDb;
    const int before = backend.routingCalls;
    QCOMPARE(coordinator.submit(fader).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.routingCalls, before + 1);
    QCOMPARE(backend.routing.size(), 1);
    QCOMPARE(backend.routing.constFirst().gainDb, -3.0);
    QCOMPARE(backend.routing.constFirst().source, initial.source);
    QCOMPARE(backend.routing.constFirst().target, initial.target);
    QVERIFY2(validateSnapshot(coordinator.snapshot()).accepted,
             qPrintable(validateSnapshot(coordinator.snapshot()).reasonCode));

    // Repeating a configured value does not invent another graph declaration.
    QCOMPARE(coordinator.submit(fader).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.routingCalls, before + 1);
    fader.gainDb = 0.0;
    QCOMPARE(coordinator.submit(fader).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.routingCalls, before + 2);
}

QTEST_GUILESS_MAIN(ConsoleGainRepublicationTests)
#include "tst_console_gain_republication.moc"
