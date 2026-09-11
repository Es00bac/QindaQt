// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_settings_test_support.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsAudio;
using namespace QindaQt::Apps::SettingsAudio::TestSupport;
using namespace QindaQt::Audio;

bool rowAvailable(const QVariantList &rows, int index, const QString &key);

class AudioSettingsModelAdversarialTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void clearsInventoryOnOwnerLossAndAcceptsReplacement();
  void epochReplacementFencesPendingOperationAndOldHandles();
  void ignoresForeignAndLateCompletions();
  void retainsStaleTruthWhenTheServiceStopsAnswering();
  void reloadRestartsDiscoveryWithoutReplayingMutations();

private:
  struct Fixture final {
    FakeAudioTransport transport;
    AudioClient client;
    AudioSettingsModel model;

    Fixture()
        : client(&transport), model(client) {
      client.setRequestTimeout(2'000);
      client.start();
      transport.announceOwner(QStringLiteral(":1.7"));
      QTRY_VERIFY_WITH_TIMEOUT(!transport.fetches.isEmpty(), 1'000);
      transport.reply(transport.fetches.constLast(), readyAudioSnapshot());
      QTRY_VERIFY_WITH_TIMEOUT(model.ready(), 1'000);
    }
  };
};

void AudioSettingsModelAdversarialTest::
    clearsInventoryOnOwnerLossAndAcceptsReplacement() {
  Fixture fixture;
  QVERIFY(fixture.model.setDefaultDevice(12));
  const auto retiredCall = fixture.transport.operations.constLast();
  QVERIFY(fixture.model.busy());

  fixture.transport.announceOwner(QString());
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.unavailable(), 1'000);
  QVERIFY(!fixture.model.stale());
  QVERIFY(fixture.model.serviceOwner().isEmpty());
  QVERIFY(fixture.model.serviceEpoch() == 0);
  QVERIFY(fixture.model.outputDevices().isEmpty());
  QVERIFY(fixture.model.inputDevices().isEmpty());
  QVERIFY(fixture.model.streams().isEmpty());
  QVERIFY(!fixture.model.busy());
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.errorText().contains(
                               QStringLiteral("could not be confirmed")),
                           1'000);

  // A late transport reply for the retired owner is not presentation truth.
  fixture.transport.finish(retiredCall,
                           audioResult(OperationKind::SetDefault,
                                       OperationStatus::Succeeded, 11, 2));
  QVERIFY(fixture.model.outputDevices().isEmpty());
  QVERIFY(!fixture.model.ready());

  fixture.transport.announceOwner(QStringLiteral(":1.42"));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.transport.fetches.isEmpty(), 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(),
                          readyAudioSnapshot(12, 1));
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.42"));
  QCOMPARE(fixture.model.serviceEpoch(), qulonglong(12));
  QCOMPARE(fixture.model.outputDevices().size(), 3);
}

void AudioSettingsModelAdversarialTest::
    epochReplacementFencesPendingOperationAndOldHandles() {
  Fixture fixture;
  QVERIFY(fixture.model.setStreamVolume(30, 0.8));
  QVERIFY(fixture.model.busy());

  // A new accepted epoch retires the dispatched mutation as uncertain. The
  // replacement publishes fresh serial identities so every retained serial
  // from the retired epoch is provably stale.
  Snapshot replacement = readyAudioSnapshot(12, 1);
  constexpr quint64 shift = 100;
  for (Device &device : replacement.outputs) {
    device.handle.serial += shift;
  }
  for (Device &device : replacement.inputs) {
    device.handle.serial += shift;
  }
  for (Stream &stream : replacement.streams) {
    stream.handle.serial += shift;
    if (stream.targetKnown) {
      stream.target.serial += shift;
    }
  }
  replacement.defaultOutput.serial += shift;
  replacement.defaultInput.serial += shift;

  fixture.transport.invalidate(QStringLiteral(":1.7"), 12, 1);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.transport.fetches.size() >= 2, 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(), replacement);
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.errorText().contains(
                               QStringLiteral("could not be confirmed")),
                           1'000);
  QVERIFY(fixture.model.serviceEpoch() == 12);

  // Serial 30 belongs to the retired epoch: the re-resolved dispatch is
  // refused locally instead of being sent as a stale handle.
  const qsizetype dispatched = fixture.transport.operations.size();
  QVERIFY(!fixture.model.setStreamVolume(30, 0.9));
  QVERIFY(!fixture.model.setDeviceVolume(10, 0.9));
  QVERIFY(!fixture.model.setDefaultDevice(10));
  QCOMPARE(fixture.transport.operations.size(), dispatched);
  QVERIFY(fixture.model.errorText().contains(QStringLiteral("refreshed")));
}

void AudioSettingsModelAdversarialTest::ignoresForeignAndLateCompletions() {
  Fixture fixture;
  const QString before = fixture.model.errorText();
  const QString statusBefore = fixture.model.operationStatusText();

  // A completion for a request this model never tracked cannot become
  // presentation truth, whatever its lineage claims.
  QVERIFY(QMetaObject::invokeMethod(
      &fixture.client, "operationCompleted", Q_ARG(quint64, 4'242),
      Q_ARG(OperationResult,
            audioResult(OperationKind::SetVolume, OperationStatus::Succeeded,
                        11, 2))));
  QCoreApplication::processEvents();
  QCOMPARE(fixture.model.errorText(), before);
  QCOMPARE(fixture.model.operationStatusText(), statusBefore);

  QVERIFY(fixture.model.setDeviceMuted(20, true));
  const auto tracked = fixture.transport.operations.constLast();
  // A mismatched transport reply for another request id is dropped by the
  // client and never reaches the projection.
  OperationRequest foreign{.kind = OperationKind::SetMute,
                            .primary = Handle{11, 20},
                            .secondary = {},
                            .volume = 0.0,
                            .muted = false};
  fixture.transport.finish({QStringLiteral(":1.7"), 99'999, foreign},
                           audioResult(OperationKind::SetMute,
                                       OperationStatus::Succeeded, 11, 2));
  QVERIFY(fixture.model.busy());
  fixture.transport.finish(tracked,
                           audioResult(OperationKind::SetMute,
                                       OperationStatus::Rejected, 11, 2,
                                       QStringLiteral("stale-handle")));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(
      fixture.model.errorText().contains(QStringLiteral("refreshed")), 1'000);
}

void AudioSettingsModelAdversarialTest::
    retainsStaleTruthWhenTheServiceStopsAnswering() {
  Fixture fixture;
  const QVariantList outputsBefore = fixture.model.outputDevices();

  // A fresh invalidation schedules a new authoritative fetch; failing that
  // fetch leaves the previously accepted snapshot retained as stale truth.
  fixture.transport.invalidate(QStringLiteral(":1.7"), 11, 99);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.transport.fetches.size(), 2, 1'000);
  fixture.transport.fail(fixture.transport.fetches.constLast(),
                         QStringLiteral("transport-failed"));
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.stale(), 1'000);
  QVERIFY(!fixture.model.ready());
  QCOMPARE(fixture.model.outputDevices().size(), outputsBefore.size());
  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.7"));
  QVERIFY(fixture.model.statusText().contains(QStringLiteral("stale")));
  // The retained snapshot keeps its own admission truth: it was accepted as
  // Ready, so the client still admits operations against it and the route
  // keeps availability equal to that admission.
  QVERIFY(rowAvailable(fixture.model.outputDevices(), 1,
                       QStringLiteral("setDefaultAvailable")));
  QVERIFY(fixture.model.setDefaultDevice(12));
  QCOMPARE(fixture.transport.operations.size(), 1);
}

void AudioSettingsModelAdversarialTest::
    reloadRestartsDiscoveryWithoutReplayingMutations() {
  Fixture fixture;
  QVERIFY(fixture.model.setDeviceVolume(10, 0.5));
  const qsizetype dispatched = fixture.transport.operations.size();
  QVERIFY(!fixture.model.reload());
  QCOMPARE(fixture.transport.operations.size(), dispatched);

  fixture.transport.finish(
      fixture.transport.operations.constLast(),
      audioResult(OperationKind::SetVolume, OperationStatus::Uncertain, 11, 2,
                  QStringLiteral("operation-timeout")));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);

  const int stopsBefore = fixture.transport.stopCalls;
  QVERIFY(fixture.model.reload());
  QCOMPARE(fixture.transport.stopCalls, stopsBefore + 1);
  QCOMPARE(fixture.transport.startCalls, 2);
  QCOMPARE(fixture.transport.operations.size(), dispatched);
  QVERIFY(fixture.model.loading());
  QVERIFY(fixture.model.outputDevices().isEmpty());
  QVERIFY(fixture.model.operationStatusText().contains(
      QStringLiteral("Reconnecting")));

  fixture.transport.announceOwner(QStringLiteral(":1.7"));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.transport.fetches.isEmpty(), 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(),
                          readyAudioSnapshot(11, 2));
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  QCOMPARE(fixture.transport.operations.size(), dispatched);
}

bool rowAvailable(const QVariantList &rows, const int index,
                  const QString &key) {
  return rows.at(index).toMap().value(key).toBool();
}

QTEST_MAIN(AudioSettingsModelAdversarialTest)
#include "tst_audio_settings_model_adversarial.moc"