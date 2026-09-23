// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_settings_test_support.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsAudio;
using namespace QindaQt::Apps::SettingsAudio::TestSupport;
using namespace QindaQt::Audio;

class AudioStreamRoutingTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void playbackMoveUsesExactHandlesAndAuthoritativeReadback();
  void recordingChoicesAndCapabilityRefusals();
  void removalAndOwnerReplacementRetireOldChoices();

private:
  struct Fixture final {
    FakeAudioTransport transport;
    AudioClient client{&transport};
    AudioSettingsModel model{client};

    Fixture() {
      client.setRequestTimeout(2'000);
      client.start();
      transport.announceOwner(QStringLiteral(":1.7"));
      QTRY_COMPARE(transport.fetches.size(), 1);
      transport.reply(transport.fetches.constLast(), readyAudioSnapshot());
      QTRY_VERIFY(model.ready());
    }

    void publish(const Snapshot &snapshot, const QString &owner =
                 QStringLiteral(":1.7")) {
      const qsizetype previousFetches = transport.fetches.size();
      transport.invalidate(owner, snapshot.epoch, snapshot.revision);
      QTRY_VERIFY(transport.fetches.size() > previousFetches);
      transport.reply(transport.fetches.constLast(), snapshot);
      QTRY_COMPARE(model.serviceRevision(), qulonglong(snapshot.revision));
    }
  };
};

void AudioStreamRoutingTest::playbackMoveUsesExactHandlesAndAuthoritativeReadback() {
  Fixture f;
  const QVariantMap before = f.model.streams().at(0).toMap();
  QCOMPARE(before.value(QStringLiteral("targetSerial")).toULongLong(), 10ULL);
  QCOMPARE(before.value(QStringLiteral("targetName")).toString(),
           QStringLiteral("Desk Speakers"));
  QCOMPARE(before.value(QStringLiteral("targetChoices")).toList().size(), 3);
  QVERIFY(before.value(QStringLiteral("moveAvailable")).toBool());
  QCOMPARE(before.value(QStringLiteral("targetKindText")).toString(),
           QStringLiteral("Output device"));

  QVERIFY(f.model.moveStream(30, 12));
  QCOMPARE(f.transport.operations.size(), 1);
  const auto move = f.transport.operations.constLast();
  QCOMPARE(move.request.kind, OperationKind::MoveStream);
  QCOMPARE(move.request.primary, (Handle{11, 30}));
  QCOMPARE(move.request.secondary, (Handle{11, 12}));
  QVERIFY(!f.model.streams().at(0).toMap()
               .value(QStringLiteral("moveAvailable")).toBool());
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetSerial")).toULongLong(), 10ULL);
  QVERIFY(!f.model.moveStream(30, 14));
  QCOMPARE(f.transport.operations.size(), 1);

  f.transport.finish(move, audioResult(OperationKind::MoveStream,
                                       OperationStatus::Succeeded, 11, 2));
  QTRY_VERIFY(f.model.operationStatusText().contains(
      QStringLiteral("Checking")));
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetSerial")).toULongLong(), 10ULL);
  QVERIFY(!f.model.streams().at(0).toMap()
               .value(QStringLiteral("moveAvailable")).toBool());
  QTRY_COMPARE(f.transport.fetches.size(), 2);
  Snapshot after = readyAudioSnapshot(11, 3);
  after.streams[0].target = {.epoch = 11, .serial = 12};
  f.transport.reply(f.transport.fetches.constLast(), after);
  QTRY_COMPARE(f.model.streams().at(0).toMap()
                   .value(QStringLiteral("targetSerial")).toULongLong(), 12ULL);
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetName")).toString(),
           QStringLiteral("Surround Headphones"));
  QVERIFY(f.model.streams().at(0).toMap()
              .value(QStringLiteral("moveAvailable")).toBool());
  QVERIFY(f.model.errorText().isEmpty());

  // A service success without matching graph readback is not presented as
  // a changed target. The next accepted snapshot retains the old selection
  // and reports a bounded mismatch instead.
  QVERIFY(f.model.moveStream(30, 10));
  const auto ineffective = f.transport.operations.constLast();
  f.transport.finish(ineffective, audioResult(OperationKind::MoveStream,
                                               OperationStatus::Succeeded, 11, 3));
  // AudioClient starts its post-result fetch before queued operationCompleted.
  // Reply immediately, so the snapshot publishes before the route sees the
  // success callback. The dispatch-sequence fence must still consume it.
  QTRY_COMPARE(f.transport.fetches.size(), 3);
  after.revision = 4;
  f.transport.reply(f.transport.fetches.constLast(), after);
  QTRY_VERIFY2(f.model.errorText().contains(QStringLiteral("did not take effect")),
               qPrintable(QStringLiteral("error=%1 status=%2 revision=%3 target=%4")
                   .arg(f.model.errorText(), f.model.operationStatusText())
                   .arg(f.model.serviceRevision())
                   .arg(f.model.streams().at(0).toMap()
                            .value(QStringLiteral("targetSerial")).toULongLong())));
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetSerial")).toULongLong(), 12ULL);

  Snapshot removed = after;
  removed.revision = 5;
  removed.outputs.removeAt(1);
  removed.streams[0].target = {};
  removed.streams[0].targetKnown = false;
  QVERIFY(validateSnapshot(removed).accepted);
  f.publish(removed);
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetSerial")).toULongLong(), 0ULL);
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetChoices")).toList().size(), 2);
  QVERIFY(!f.model.moveStream(30, 12));
  QCOMPARE(f.transport.operations.size(), 2);
}

void AudioStreamRoutingTest::recordingChoicesAndCapabilityRefusals() {
  Fixture f;
  Snapshot withSecondInput = readyAudioSnapshot(11, 3);
  Device usb = withSecondInput.inputs.first();
  usb.handle.serial = 22;
  usb.name = QStringLiteral("usb-mic");
  usb.description = QStringLiteral("USB Microphone");
  usb.isDefault = false;
  withSecondInput.inputs.append(usb);
  QVERIFY(validateSnapshot(withSecondInput).accepted);
  f.publish(withSecondInput);

  const QVariantMap row = f.model.streams().at(1).toMap();
  const QVariantList choices = row.value(QStringLiteral("targetChoices")).toList();
  QCOMPARE(choices.size(), 2);
  QCOMPARE(choices.at(0).toMap().value(QStringLiteral("serial")).toULongLong(),
           20ULL);
  QCOMPARE(choices.at(1).toMap().value(QStringLiteral("serial")).toULongLong(),
           22ULL);
  QCOMPARE(row.value(QStringLiteral("targetKindText")).toString(),
           QStringLiteral("Input device"));
  QVERIFY(row.value(QStringLiteral("moveAvailable")).toBool());

  QVERIFY(!f.model.moveStream(40, 12)); // output is incompatible with capture
  QVERIFY(f.model.errorText().contains(QStringLiteral("cannot be changed")));
  QVERIFY(!f.model.moveStream(30, 20)); // input is incompatible with playback
  QVERIFY(!f.model.moveStream(40, 20)); // same target is not a move
  QVERIFY(!f.model.moveStream(999, 22));
  QVERIFY(!f.model.moveStream(40, 999));
  QVERIFY(f.transport.operations.isEmpty());
  QVERIFY(f.model.moveStream(40, 22));
  QCOMPARE(f.transport.operations.constLast().request.primary,
           (Handle{11, 40}));
  QCOMPARE(f.transport.operations.constLast().request.secondary,
           (Handle{11, 22}));
  f.transport.finish(f.transport.operations.constLast(),
                     audioResult(OperationKind::MoveStream,
                                 OperationStatus::Rejected, 11, 3,
                                 QStringLiteral("unsupported")));
  QTRY_VERIFY(f.model.errorText().contains(QStringLiteral("not permitted")));
  QCOMPARE(f.model.streams().at(1).toMap()
               .value(QStringLiteral("targetSerial")).toULongLong(), 20ULL);

  Snapshot restricted = withSecondInput;
  restricted.revision = 4;
  restricted.capabilities &= ~Capabilities(Capability::MoveStream);
  for (Stream &stream : restricted.streams) stream.canMove = false;
  QVERIFY(validateSnapshot(restricted).accepted);
  f.publish(restricted);
  QVERIFY(!f.model.streams().at(0).toMap()
               .value(QStringLiteral("moveAvailable")).toBool());
  QVERIFY(!f.model.streams().at(1).toMap()
               .value(QStringLiteral("moveAvailable")).toBool());
  QVERIFY(!f.model.moveStream(40, 22));
  QCOMPARE(f.transport.operations.size(), 1);

  Snapshot streamRestricted = withSecondInput;
  streamRestricted.revision = 5;
  streamRestricted.streams[1].canMove = false;
  QVERIFY(validateSnapshot(streamRestricted).accepted);
  f.publish(streamRestricted);
  QVERIFY(f.model.streams().at(0).toMap()
              .value(QStringLiteral("moveAvailable")).toBool());
  QVERIFY(!f.model.streams().at(1).toMap()
               .value(QStringLiteral("moveAvailable")).toBool());
  QVERIFY(!f.model.moveStream(40, 22));
  QCOMPARE(f.transport.operations.size(), 1);
}

void AudioStreamRoutingTest::removalAndOwnerReplacementRetireOldChoices() {
  Fixture f;
  QVERIFY(f.model.moveStream(30, 12));
  const auto oldMove = f.transport.operations.constLast();
  f.transport.announceOwner(QString());
  QTRY_VERIFY(f.model.streams().isEmpty());
  QVERIFY(f.model.serviceOwner().isEmpty());
  f.transport.announceOwner(QStringLiteral(":1.42"));
  QTRY_COMPARE(f.transport.fetches.size(), 2);
  Snapshot replacement = readyAudioSnapshot(12, 1);
  replacement.outputs.removeAt(1);
  replacement.streams[0].target = {};
  replacement.streams[0].targetKnown = false;
  QVERIFY(validateSnapshot(replacement).accepted);
  f.transport.reply(f.transport.fetches.constLast(), replacement);
  QTRY_VERIFY(f.model.ready());
  QCOMPARE(f.model.serviceOwner(), QStringLiteral(":1.42"));
  const QVariantMap row = f.model.streams().at(0).toMap();
  QCOMPARE(row.value(QStringLiteral("targetSerial")).toULongLong(), 0ULL);
  QCOMPARE(row.value(QStringLiteral("targetName")).toString(),
           QStringLiteral("Unknown device"));
  QCOMPARE(row.value(QStringLiteral("targetChoices")).toList().size(), 2);
  QVERIFY(!f.model.moveStream(30, 12));
  f.transport.finish(oldMove, audioResult(OperationKind::MoveStream,
                                          OperationStatus::Succeeded, 11, 2));
  QCoreApplication::processEvents();
  QCOMPARE(f.model.streams().at(0).toMap()
               .value(QStringLiteral("targetSerial")).toULongLong(), 0ULL);
  QCOMPARE(f.transport.operations.size(), 1);
  QVERIFY(f.model.moveStream(30, 14));
  QCOMPARE(f.transport.operations.constLast().request.primary,
           (Handle{12, 30}));
  QCOMPARE(f.transport.operations.constLast().request.secondary,
           (Handle{12, 14}));
}

QTEST_MAIN(AudioStreamRoutingTest)
#include "tst_audio_stream_routing.moc"
