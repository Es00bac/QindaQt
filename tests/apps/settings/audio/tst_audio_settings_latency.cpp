// SPDX-License-Identifier: GPL-3.0-or-later

// Settings Audio latency-offset intent (ADR-0288): the schema-13 device fields
// project as milliseconds with absence kept absent, dispatch shares Audio1's
// admission rule, a dispatched target stays on screen until readback, rapid
// edits keep one queued successor, and a refusal returns the row to device
// truth. Real AudioClient over the injected fake transport; no bus.

#include "audio_settings_test_support.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsAudio;
using namespace QindaQt::Apps::SettingsAudio::TestSupport;
using namespace QindaQt::Audio;

namespace {

constexpr qint64 kMs = 1'000'000;
const QString kOwner = QStringLiteral(":1.7");

// Output 10 behaves like an ALSA sink (0..2 s, at 40 ms), output 12 like a
// Bluetooth sink (signed range, at 0); input 20 publishes no offset at all.
Snapshot latencySnapshot(const quint64 revision = 2,
                         const qint64 speakersNs = 40 * kMs) {
  Snapshot snapshot = readyAudioSnapshot(11, revision);
  snapshot.capabilities |= Capability::SetLatencyOffset;
  Device &speakers = snapshot.outputs[0];
  speakers.nodeName = QStringLiteral("alsa_output.desk");
  speakers.latencyOffsetKnown = true;
  speakers.latencyOffsetNs = speakersNs;
  speakers.canSetLatencyOffset = true;
  speakers.latencyOffsetMaxNs = 2'000 * kMs;
  Device &headphones = snapshot.outputs[1];
  headphones.nodeName = QStringLiteral("bluez_output.headphones");
  headphones.latencyOffsetKnown = true;
  headphones.canSetLatencyOffset = true;
  headphones.latencyOffsetMinNs = -2'000 * kMs;
  headphones.latencyOffsetMaxNs = 2'000 * kMs;
  return snapshot;
}

} // namespace

class AudioSettingsLatencyTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsKnownSettableAndAbsentOffsets();
  void dispatchesNanosecondsAndHoldsTargetUntilReadback();
  void refusesOutsideTheDeviceRangeWithoutDispatch();
  void queuesOneSuccessorWhileARequestIsOut();
  void refusalReturnsTheRowToDeviceTruth();

private:
  struct Fixture final {
    FakeAudioTransport transport;
    AudioClient client;
    AudioSettingsModel model;
    Fixture() : client(&transport), model(client) {
      client.setRequestTimeout(2'000);
      client.start();
      transport.announceOwner(kOwner);
      QTRY_VERIFY_WITH_TIMEOUT(!transport.fetches.isEmpty(), 1'000);
      transport.reply(transport.fetches.constLast(), latencySnapshot());
      QTRY_VERIFY_WITH_TIMEOUT(model.ready(), 1'000);
    }
    QVariantMap output(const int index) const {
      return model.outputDevices().at(index).toMap();
    }
    // Completes the operation and answers the readback fetch AudioClient
    // starts for it with `readback`.
    void succeed(const FakeAudioTransport::Operation &operation,
                 const Snapshot &readback) {
      const qsizetype fetches = transport.fetches.size();
      transport.finish(operation, audioResult(OperationKind::SetLatencyOffset,
                                              OperationStatus::Succeeded, 11,
                                              readback.revision - 1));
      QTRY_VERIFY_WITH_TIMEOUT(transport.fetches.size() > fetches, 1'000);
      transport.reply(transport.fetches.constLast(), readback);
    }
  };
};

void AudioSettingsLatencyTest::projectsKnownSettableAndAbsentOffsets() {
  Fixture fixture;
  const QVariantMap speakers = fixture.output(0);
  QCOMPARE(speakers.value(QStringLiteral("latencyKnown")).toBool(), true);
  QCOMPARE(speakers.value(QStringLiteral("latencyAvailable")).toBool(), true);
  QCOMPARE(speakers.value(QStringLiteral("latencyMs")).toInt(), 40);
  QCOMPARE(speakers.value(QStringLiteral("latencyDisplayMs")).toInt(), 40);
  QCOMPARE(speakers.value(QStringLiteral("latencyMinMs")).toInt(), 0);
  QCOMPARE(speakers.value(QStringLiteral("latencyMaxMs")).toInt(), 2'000);
  QCOMPARE(speakers.value(QStringLiteral("latencyText")).toString(),
           QStringLiteral("40 ms"));
  QCOMPARE(fixture.output(1).value(QStringLiteral("latencyMinMs")).toInt(), -2'000);

  // AGENT-GUARD proof: no reported offset is absent, never a 0 ms reading.
  const QVariantMap microphone = fixture.model.inputDevices().constFirst().toMap();
  QCOMPARE(microphone.value(QStringLiteral("latencyKnown")).toBool(), false);
  QCOMPARE(microphone.value(QStringLiteral("latencyAvailable")).toBool(), false);
  QVERIFY(!microphone.value(QStringLiteral("latencyText")).toString().contains(
      QStringLiteral("0 ms")));

  // Without the capability bit the control is read-only everywhere.
  Snapshot withoutCapability = latencySnapshot(3);
  withoutCapability.capabilities &= ~Capabilities(Capability::SetLatencyOffset);
  for (Device &device : withoutCapability.outputs) {
    device.canSetLatencyOffset = false;
    device.latencyOffsetMinNs = 0;
    device.latencyOffsetMaxNs = 0;
  }
  fixture.transport.invalidate(kOwner, 11, 3);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.transport.fetches.size() >= 2, 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(), withoutCapability);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(3), 1'000);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyAvailable")).toBool(), false);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyMs")).toInt(), 40);
  QVERIFY(!fixture.model.setDeviceLatencyOffset(10, 60));
  QVERIFY(fixture.transport.operations.isEmpty());
}

void AudioSettingsLatencyTest::dispatchesNanosecondsAndHoldsTargetUntilReadback() {
  Fixture fixture;
  QVERIFY(fixture.model.setDeviceLatencyOffset(10, 120));
  QCOMPARE(fixture.transport.operations.size(), 1);
  const auto operation = fixture.transport.operations.constFirst();
  QCOMPARE(operation.request.kind, OperationKind::SetLatencyOffset);
  QCOMPARE(operation.request.primary, (Handle{.epoch = 11, .serial = 10}));
  QCOMPARE(operation.request.latencyOffsetNs, 120 * kMs);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyDisplayMs")).toInt(), 120);
  // The readout stays service truth while the control shows the target.
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyMs")).toInt(), 40);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyPending")).toBool(), true);

  // Success before the graph echo keeps the target on screen...
  fixture.succeed(operation, latencySnapshot(3));
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(3), 1'000);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyDisplayMs")).toInt(), 120);
  // ...until a snapshot reports the applied value.
  fixture.transport.invalidate(kOwner, 11, 4);
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.transport.fetches.isEmpty(), 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(),
                          latencySnapshot(4, 120 * kMs));
  QTRY_COMPARE_WITH_TIMEOUT(
      fixture.output(0).value(QStringLiteral("latencyPending")).toBool(), false, 1'000);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyMs")).toInt(), 120);

  // A signed target on the Bluetooth-like device is admitted.
  QVERIFY(fixture.model.setDeviceLatencyOffset(12, -35));
  QCOMPARE(fixture.transport.operations.constLast().request.latencyOffsetNs, -35 * kMs);
}

void AudioSettingsLatencyTest::refusesOutsideTheDeviceRangeWithoutDispatch() {
  Fixture fixture;
  QSignalSpy rejected(&fixture.model, &AudioSettingsModel::actionRejected);
  QVERIFY(!fixture.model.setDeviceLatencyOffset(10, -5));     // ALSA: 0..2 s
  QVERIFY(!fixture.model.setDeviceLatencyOffset(12, 2'001));  // protocol window
  QVERIFY(!fixture.model.setDeviceLatencyOffset(20, 10));     // no offset
  QVERIFY(!fixture.model.setDeviceLatencyOffset(99, 10));     // no device
  QVERIFY(fixture.transport.operations.isEmpty());
  QCOMPARE(rejected.size(), 4);
  QCOMPARE(rejected.at(0).constFirst().toString(),
           QStringLiteral("invalid-latency-offset"));
  QCOMPARE(rejected.at(2).constFirst().toString(), QStringLiteral("unsupported"));
  QCOMPARE(rejected.at(3).constFirst().toString(), QStringLiteral("stale-handle"));
  QVERIFY(!fixture.model.errorText().isEmpty());
}

void AudioSettingsLatencyTest::queuesOneSuccessorWhileARequestIsOut() {
  Fixture fixture;
  QVERIFY(fixture.model.setDeviceLatencyOffset(10, 50));
  QVERIFY(fixture.model.setDeviceLatencyOffset(10, 55));
  QVERIFY(fixture.model.setDeviceLatencyOffset(10, 60));
  // Only the first left; the latest replaced the queue and is displayed.
  QCOMPARE(fixture.transport.operations.size(), 1);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyDisplayMs")).toInt(), 60);
  QVERIFY(fixture.model.errorText().isEmpty());

  fixture.succeed(fixture.transport.operations.constFirst(), latencySnapshot(3));
  QTRY_COMPARE_WITH_TIMEOUT(fixture.transport.operations.size(), 2, 1'000);
  QCOMPARE(fixture.transport.operations.constLast().request.latencyOffsetNs, 60 * kMs);
  QCOMPARE(fixture.output(0).value(QStringLiteral("latencyDisplayMs")).toInt(), 60);
}

void AudioSettingsLatencyTest::refusalReturnsTheRowToDeviceTruth() {
  Fixture fixture;
  QVERIFY(fixture.model.setDeviceLatencyOffset(12, 150));
  QCOMPARE(fixture.output(1).value(QStringLiteral("latencyDisplayMs")).toInt(), 150);
  fixture.transport.finish(
      fixture.transport.operations.constFirst(),
      audioResult(OperationKind::SetLatencyOffset, OperationStatus::Rejected, 11, 2,
                  QStringLiteral("invalid-latency-offset")));
  QTRY_COMPARE_WITH_TIMEOUT(
      fixture.output(1).value(QStringLiteral("latencyPending")).toBool(), false, 1'000);
  QCOMPARE(fixture.output(1).value(QStringLiteral("latencyDisplayMs")).toInt(), 0);
  QVERIFY(!fixture.model.errorText().isEmpty());
  // The refusal fenced nothing: the next intent dispatches.
  QVERIFY(fixture.model.setDeviceLatencyOffset(12, 20));
  QCOMPARE(fixture.transport.operations.size(), 2);
}

QTEST_GUILESS_MAIN(AudioSettingsLatencyTest)
#include "tst_audio_settings_latency.moc"
