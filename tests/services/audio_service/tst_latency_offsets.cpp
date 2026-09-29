// SPDX-License-Identifier: GPL-3.0-or-later

// Per-device latency offsets (ADR-0288) without a daemon: the Audio1-owned
// store, the pure projection and reconcile policy, and the coordinator's
// remember-and-declare operation against the fake backend.

#include "../../../src/services/audio_service/src/latency_policy_p.h"
#include "support/fake_audio_backend.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_service/latency_store.h>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

namespace {

constexpr qint64 kMs = 1'000'000;
const QString kSpeakers = QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo");

// The fake backend's hardware output as the graph worker publishes it for an
// ALSA sink: a known offset and the node's declared 0..2 s range.
Snapshot latencySnapshot(const quint64 revision = 3) {
  Snapshot snapshot = audioSnapshot(7, revision);
  snapshot.capabilities |= Capability::SetLatencyOffset;
  Device &speakers = snapshot.outputs[0];
  speakers.latencyOffsetKnown = true;
  speakers.latencyOffsetNs = 0;
  speakers.canSetLatencyOffset = true;
  speakers.latencyOffsetMaxNs = 2'000 * kMs;
  return snapshot;
}

OperationRequest setOffset(const quint64 serial, const qint64 offsetNs) {
  OperationRequest request;
  request.kind = OperationKind::SetLatencyOffset;
  request.primary = {.epoch = 7, .serial = serial};
  request.latencyOffsetNs = offsetNs;
  return request;
}

} // namespace

class LatencyOffsetTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void storeRoundTripsAndKeepsAnExplicitZero();
  void storeSkipsInvalidEntriesAndRefusesBadWrites();
  void projectionIsCanonical();
  void reconcileWritesOnceAndAnswersOneReset();
  void coordinatorRemembersAndDeclares();
  void coordinatorRefusesWhatTheDeviceCannotTake();
  void startDeclaresRememberedOffsets();
};

void LatencyOffsetTests::storeRoundTripsAndKeepsAnExplicitZero() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const LatencyStore store(dir.filePath(QStringLiteral("nested/audio-latency.json")));
  QVERIFY(store.load().isEmpty());
  QString reason;
  QVERIFY(store.set(kSpeakers, 120 * kMs, &reason));
  QVERIFY(store.set(QStringLiteral("bluez_output.headset"), -35 * kMs, &reason));
  QVERIFY(store.set(kSpeakers, 0, &reason));
  const QMap<QString, qint64> offsets = store.load();
  QCOMPARE(offsets.size(), 2);
  // A reset is remembered as the user's choice, not forgotten.
  QCOMPARE(offsets.value(kSpeakers, -1), qint64(0));
  QCOMPARE(offsets.value(QStringLiteral("bluez_output.headset")), -35 * kMs);
}

void LatencyOffsetTests::storeSkipsInvalidEntriesAndRefusesBadWrites() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("audio-latency.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(R"({"schemaVersion":1,"devices":[
      {"nodeName":"good","offsetNs":5000000},
      {"nodeName":"fraction","offsetNs":1.5},
      {"nodeName":"text","offsetNs":"5"},
      {"nodeName":"huge","offsetNs":9000000000},
      {"nodeName":"","offsetNs":0},
      {"nodeName":"good","offsetNs":7000000}]})");
  file.close();
  const LatencyStore store(path);
  const QMap<QString, qint64> offsets = store.load();
  QCOMPARE(offsets.keys(), QStringList{QStringLiteral("good")});
  QCOMPARE(offsets.value(QStringLiteral("good")), 5 * kMs);

  QString reason;
  QVERIFY(!store.set(QStringLiteral("good"), kMaxLatencyOffsetNs + 1, &reason));
  QCOMPARE(reason, QStringLiteral("invalid-latency-offset"));
  QVERIFY(!store.set(QStringLiteral("  "), 0, &reason));
  QCOMPARE(reason, QStringLiteral("invalid-latency-offset"));
  // A path that cannot be a directory fails without touching the document.
  const LatencyStore blocked(path + QStringLiteral("/under-a-file.json"));
  QVERIFY(!blocked.set(QStringLiteral("good"), 0, &reason));
  QCOMPARE(reason, QStringLiteral("latency-store-unwritable"));
  QCOMPARE(store.load().value(QStringLiteral("good")), 5 * kMs);
}

void LatencyOffsetTests::projectionIsCanonical() {
  const Capabilities capabilities = Capability::SetLatencyOffset;
  Device device;
  device.nodeName = kSpeakers;
  // No reported offset: absent, and valid only in that canonical form.
  LatencyPolicy::project(device, std::nullopt, LatencyPolicy::Range{0, 2'000 * kMs}, true);
  QVERIFY(!device.latencyOffsetKnown);
  QVERIFY(validDeviceLatency(device, capabilities));
  // Out of Audio1's window: unknown, never a clamped number.
  LatencyPolicy::project(device, 5'000 * kMs, LatencyPolicy::Range{0, 2'000 * kMs}, true);
  QVERIFY(!device.latencyOffsetKnown);
  QCOMPARE(device.latencyOffsetNs, qint64(0));
  // Bluetooth declares the whole signed range; Audio1 clips it.
  LatencyPolicy::project(device, -10 * kMs,
                         LatencyPolicy::Range{std::numeric_limits<qint64>::min(),
                                              std::numeric_limits<qint64>::max()},
                         true);
  QVERIFY(device.canSetLatencyOffset);
  QCOMPARE(device.latencyOffsetMinNs, kMinLatencyOffsetNs);
  QCOMPARE(device.latencyOffsetMaxNs, kMaxLatencyOffsetNs);
  QVERIFY(validDeviceLatency(device, capabilities));
  QVERIFY(!validDeviceLatency(device, {}));
  // Readable but not settable: no declared range yet, outside its own
  // declared range, no capability, or nothing to remember it by.
  LatencyPolicy::project(device, 40 * kMs, std::nullopt, true);
  QVERIFY(device.latencyOffsetKnown && !device.canSetLatencyOffset);
  QCOMPARE(device.latencyOffsetNs, 40 * kMs);
  QVERIFY(validDeviceLatency(device, capabilities));
  LatencyPolicy::project(device, -5 * kMs, LatencyPolicy::Range{0, 2'000 * kMs}, true);
  QVERIFY(device.latencyOffsetKnown && !device.canSetLatencyOffset);
  LatencyPolicy::project(device, 40 * kMs, LatencyPolicy::Range{0, 2'000 * kMs}, false);
  QVERIFY(!device.canSetLatencyOffset);
  device.nodeName.clear();
  LatencyPolicy::project(device, 40 * kMs, LatencyPolicy::Range{0, 2'000 * kMs}, true);
  QVERIFY(!device.canSetLatencyOffset);
  QCOMPARE(device.latencyOffsetMinNs, qint64(0));

  // Protocol admission shares the published range.
  device.nodeName = kSpeakers;
  LatencyPolicy::project(device, 0, LatencyPolicy::Range{0, 2'000 * kMs}, true);
  QVERIFY(latencyOffsetAdmitted(device, 2'000 * kMs));
  QVERIFY(!latencyOffsetAdmitted(device, -1));
  QVERIFY(!latencyOffsetAdmitted(device, 2'000 * kMs + 1));
}

void LatencyOffsetTests::reconcileWritesOnceAndAnswersOneReset() {
  LatencyPolicy::ReconcileState state;
  // A node that publishes no offset is never written.
  QVERIFY(!LatencyPolicy::shouldWrite(state, 120 * kMs, std::nullopt));
  // First sighting writes; rebuilds before the echo do not resend.
  QVERIFY(LatencyPolicy::shouldWrite(state, 120 * kMs, qint64(0)));
  QVERIFY(!LatencyPolicy::shouldWrite(state, 120 * kMs, qint64(0)));
  QVERIFY(!LatencyPolicy::shouldWrite(state, 120 * kMs, 120 * kMs));
  // A device (a Bluetooth route re-emitting its own offset) resets it: one
  // more write answers it.
  QVERIFY(LatencyPolicy::shouldWrite(state, 120 * kMs, qint64(0)));
  QVERIFY(!LatencyPolicy::shouldWrite(state, 120 * kMs, 120 * kMs));
  QVERIFY(LatencyPolicy::shouldWrite(state, 120 * kMs, 5 * kMs));
  // The budget is spent: a node that keeps overriding keeps its own value.
  QVERIFY(!LatencyPolicy::shouldWrite(state, 120 * kMs, qint64(0)));
  QCOMPARE(state.writes, LatencyPolicy::kMaxWritesPerDeclaration);
  // A new declaration from the user starts a fresh budget.
  QVERIFY(LatencyPolicy::shouldWrite(state, 80 * kMs, qint64(0)));
  QCOMPARE(state.writes, 1);
}

void LatencyOffsetTests::coordinatorRemembersAndDeclares() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString latencyPath = dir.filePath(QStringLiteral("audio-latency.json"));
  FakeAudioBackend backend;
  AudioOperationCoordinator coordinator(&backend, nullptr, dir.filePath(QStringLiteral("p")),
                                        dir.filePath(QStringLiteral("m.json")),
                                        dir.filePath(QStringLiteral("v.json")), latencyPath);
  coordinator.start();
  backend.publish(latencySnapshot());
  QCOMPARE(coordinator.snapshot().revision, quint64(3));

  const OperationSubmission submitted = coordinator.submit(setOffset(10, 120 * kMs));
  QVERIFY(!submitted.pending);
  QCOMPARE(submitted.immediateResult.status, OperationStatus::Succeeded);
  QVERIFY(validateOperationResult(submitted.immediateResult).accepted);
  // Never a graph operation: remembered and declared instead.
  QVERIFY(backend.operations.isEmpty());
  QCOMPARE(backend.latency,
           (QList<BackendLatencyOffset>{{.nodeName = kSpeakers, .offsetNs = 120 * kMs}}));
  QCOMPARE(LatencyStore(latencyPath).load().value(kSpeakers), 120 * kMs);
  // An unchanged declaration is not re-sent.
  const int calls = backend.latencyCalls;
  QCOMPARE(coordinator.submit(setOffset(10, 120 * kMs)).immediateResult.status,
           OperationStatus::Succeeded);
  QCOMPARE(backend.latencyCalls, calls);
  // Reset is remembered as 0 and declared.
  QCOMPARE(coordinator.submit(setOffset(10, 0)).immediateResult.status,
           OperationStatus::Succeeded);
  QCOMPARE(backend.latency.constFirst().offsetNs, qint64(0));
}

void LatencyOffsetTests::coordinatorRefusesWhatTheDeviceCannotTake() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString latencyPath = dir.filePath(QStringLiteral("audio-latency.json"));
  FakeAudioBackend backend;
  AudioOperationCoordinator coordinator(&backend, nullptr, dir.filePath(QStringLiteral("p")),
                                        dir.filePath(QStringLiteral("m.json")),
                                        dir.filePath(QStringLiteral("v.json")), latencyPath);
  coordinator.start();
  backend.publish(latencySnapshot());
  const auto refused = [&coordinator](const OperationRequest &request) {
    const OperationSubmission submitted = coordinator.submit(request);
    return std::pair{submitted.immediateResult.status, submitted.immediateResult.reasonCode};
  };
  QCOMPARE(refused(setOffset(10, -1)),
           std::pair(OperationStatus::Rejected, QStringLiteral("invalid-latency-offset")));
  QCOMPARE(refused(setOffset(10, 2'000 * kMs + 1)),
           std::pair(OperationStatus::Rejected, QStringLiteral("invalid-latency-offset")));
  // The virtual output publishes no offset.
  QCOMPARE(refused(setOffset(11, 0)),
           std::pair(OperationStatus::Unsupported, QStringLiteral("unsupported")));
  QCOMPARE(refused(setOffset(99, 0)),
           std::pair(OperationStatus::Rejected, QStringLiteral("stale-handle")));
  OperationRequest stale = setOffset(10, 0);
  stale.primary.epoch = 6;
  QCOMPARE(refused(stale), std::pair(OperationStatus::Rejected, QStringLiteral("stale-handle")));
  // Without the capability bit nothing is admitted.
  Snapshot withoutCapability = latencySnapshot(4);
  withoutCapability.capabilities &= ~Capabilities(Capability::SetLatencyOffset);
  withoutCapability.outputs[0].canSetLatencyOffset = false;
  withoutCapability.outputs[0].latencyOffsetMaxNs = 0;
  backend.publish(withoutCapability);
  QCOMPARE(refused(setOffset(10, 0)),
           std::pair(OperationStatus::Unsupported, QStringLiteral("unsupported")));
  QVERIFY(backend.latency.isEmpty());
  QVERIFY(!QFile::exists(latencyPath));
}

void LatencyOffsetTests::startDeclaresRememberedOffsets() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString latencyPath = dir.filePath(QStringLiteral("audio-latency.json"));
  QString reason;
  QVERIFY(LatencyStore(latencyPath).set(kSpeakers, 75 * kMs, &reason));
  FakeAudioBackend backend;
  AudioOperationCoordinator coordinator(&backend, nullptr, dir.filePath(QStringLiteral("p")),
                                        dir.filePath(QStringLiteral("m.json")),
                                        dir.filePath(QStringLiteral("v.json")), latencyPath);
  // Declared from the first moment the backend runs, before any snapshot,
  // so a device already present gets its offset back after a restart.
  coordinator.start();
  QCOMPARE(backend.latency,
           (QList<BackendLatencyOffset>{{.nodeName = kSpeakers, .offsetNs = 75 * kMs}}));
  coordinator.stop();
  const int calls = backend.latencyCalls;
  coordinator.start();
  QCOMPARE(backend.latencyCalls, calls + 1);
}

QTEST_GUILESS_MAIN(LatencyOffsetTests)
#include "tst_latency_offsets.moc"
