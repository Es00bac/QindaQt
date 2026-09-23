// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_backend.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_service/vban_store.h>

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

class VbanOperationTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void typedCrudRequiresSeparateReceiveAuthorization();
};

void VbanOperationTests::typedCrudRequiresSeparateReceiveAuthorization()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("audio-vban.json"));
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend, nullptr,
                                          dir.filePath(QStringLiteral("presets")),
                                          dir.filePath(QStringLiteral("macros.json")), path);
    coordinator.start();
    backend.publish(audioSnapshot());

    VbanStream incoming;
    incoming.name = QStringLiteral("Peer");
    incoming.outgoing = false;
    incoming.host = QStringLiteral("192.0.2.10");
    incoming.outputNodeName =
        QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo");
    incoming.port = 6980;
    OperationRequest upsert;
    upsert.kind = OperationKind::UpsertVbanStream;
    upsert.vbanDefinition = incoming;
    QCOMPARE(coordinator.submit(upsert).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(VbanStore(path).load().size(), 1);
    QCOMPARE(coordinator.snapshot().console.vban.size(), 1);
    QVERIFY(!coordinator.snapshot().console.vban.first().enabled);
    QVERIFY(backend.vban.isEmpty());

    OperationRequest enable;
    enable.kind = OperationKind::SetVbanEnabled;
    enable.displayName = incoming.name;
    enable.enabled = true;
    QCOMPARE(coordinator.submit(enable).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.vban.size(), 1);
    QCOMPARE(backend.vban.first().target.serial, quint64(10));
    QVERIFY(coordinator.snapshot().console.vban.first().enabled);
    QVERIFY(!coordinator.snapshot().console.vban.first().active);

    incoming.host = QStringLiteral("192.0.2.11");
    upsert.vbanDefinition = incoming;
    QCOMPARE(coordinator.submit(upsert).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(VbanStore(path).load().first().host, QStringLiteral("192.0.2.11"));
    QVERIFY(!coordinator.snapshot().console.vban.first().enabled);
    QVERIFY(backend.vban.isEmpty());

    VbanStream second = incoming;
    second.name = QStringLiteral("Other");
    OperationRequest conflict = upsert;
    conflict.vbanDefinition = second;
    QCOMPARE(coordinator.submit(conflict).immediateResult.reasonCode,
             QStringLiteral("vban-port-in-use"));
    QCOMPARE(VbanStore(path).load().size(), 1);

    OperationRequest remove;
    remove.kind = OperationKind::DeleteVbanStream;
    remove.displayName = incoming.name;
    QCOMPARE(coordinator.submit(remove).immediateResult.status, OperationStatus::Succeeded);
    QVERIFY(VbanStore(path).load().isEmpty());
    QVERIFY(coordinator.snapshot().console.vban.isEmpty());
    QCOMPARE(coordinator.submit(enable).immediateResult.reasonCode,
             QStringLiteral("unknown-vban-stream"));
}

QTEST_MAIN(VbanOperationTests)
#include "tst_vban_operations.moc"
