// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_settings_test_support.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <QtTest>

#include <limits>
#include <memory>

using namespace QindaQt::Apps::SettingsAudio;
using namespace QindaQt::Apps::SettingsAudio::TestSupport;
using namespace QindaQt::Audio;

class AudioSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsBoundedAuthoritativeInventory();
  void dispatchesOnlyAdmittedIntents();
  void availabilityEqualsClientAdmission();
  void reportsFailuresWithoutCredentialOrRadioSurface();
  void disablesActionsWhenCapabilitiesDisappear();
  void keepsDegradedTruthAdmittedAndLabeled();
  void uncertainOutcomesAreVisibleAndNeverReplayed();

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

  bool rowFlag(const QVariantList &rows, const int index,
               const char *const key) const;
};

bool AudioSettingsModelTest::rowFlag(const QVariantList &rows, const int index,
                                     const char *const key) const {
  return rows.at(index).toMap().value(QString::fromLatin1(key)).toBool();
}

void AudioSettingsModelTest::projectsBoundedAuthoritativeInventory() {
  Fixture fixture;
  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.7"));
  QCOMPARE(fixture.model.serviceEpoch(), qulonglong(11));
  QCOMPARE(fixture.model.serviceRevision(), qulonglong(2));
  QVERIFY(fixture.model.statusText().contains(QStringLiteral("volume")));
  QCOMPARE(fixture.model.defaultOutputName(), QStringLiteral("Desk Speakers"));
  QCOMPARE(fixture.model.defaultInputName(), QStringLiteral("Desk Microphone"));

  const QVariantList outputs = fixture.model.outputDevices();
  QCOMPARE(outputs.size(), 2);
  const QVariantMap speakers = outputs.at(0).toMap();
  QCOMPARE(speakers.value(QStringLiteral("serial")), qulonglong(10));
  QCOMPARE(speakers.value(QStringLiteral("displayName")),
           QStringLiteral("Desk Speakers"));
  QVERIFY(speakers.value(QStringLiteral("isDefault")).toBool());
  QCOMPARE(speakers.value(QStringLiteral("volumePercent")).toInt(), 50);
  QVERIFY(speakers.value(QStringLiteral("setDefaultAvailable")).toBool());
  QVERIFY(speakers.value(QStringLiteral("volumeAvailable")).toBool());
  QVERIFY(speakers.value(QStringLiteral("muteAvailable")).toBool());
  const QVariantMap headphones = outputs.at(1).toMap();
  QCOMPARE(headphones.value(QStringLiteral("serial")), qulonglong(12));
  QVERIFY(!headphones.value(QStringLiteral("isDefault")).toBool());
  QCOMPARE(headphones.value(QStringLiteral("volumePercent")).toInt(), 25);
  QVERIFY(headphones.value(QStringLiteral("setDefaultAvailable")).toBool());

  const QVariantList inputs = fixture.model.inputDevices();
  QCOMPARE(inputs.size(), 1);
  QVERIFY(inputs.at(0).toMap().value(QStringLiteral("isDefault")).toBool());

  const QVariantList streams = fixture.model.streams();
  QCOMPARE(streams.size(), 2);
  const QVariantMap playback = streams.at(0).toMap();
  QCOMPARE(playback.value(QStringLiteral("applicationName")),
           QStringLiteral("Player"));
  QCOMPARE(playback.value(QStringLiteral("directionText")),
           QStringLiteral("Playback"));
  QCOMPARE(playback.value(QStringLiteral("targetName")),
           QStringLiteral("Desk Speakers"));
  QVERIFY(playback.value(QStringLiteral("volumeAvailable")).toBool());
  const QVariantMap capture = streams.at(1).toMap();
  QCOMPARE(capture.value(QStringLiteral("directionText")),
           QStringLiteral("Recording"));
  QVERIFY(capture.value(QStringLiteral("muted")).toBool());
  QCOMPARE(capture.value(QStringLiteral("targetName")),
           QStringLiteral("Desk Microphone"));
}

void AudioSettingsModelTest::dispatchesOnlyAdmittedIntents() {
  Fixture fixture;

  QVERIFY(fixture.model.setDefaultDevice(12));
  QCOMPARE(fixture.transport.operations.size(), 1);
  const auto setDefault = fixture.transport.operations.constLast();
  QCOMPARE(setDefault.owner, QStringLiteral(":1.7"));
  QCOMPARE(setDefault.request.kind, OperationKind::SetDefault);
  QCOMPARE(setDefault.request.primary, (Handle{11, 12}));
  QVERIFY(fixture.model.busy());
  QVERIFY(!fixture.model.reloadAvailable());
  QVERIFY(!rowFlag(fixture.model.outputDevices(), 1, "setDefaultAvailable"));

  const auto completion = audioResult(OperationKind::SetDefault,
                                      OperationStatus::Succeeded, 11, 2);
  fixture.transport.finish(setDefault, completion);
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.operationStatusText().contains(
                               QStringLiteral("refreshing audio information")),
                           1'000);
  // The client schedules its own authoritative refetch after a result.
  QTRY_VERIFY_WITH_TIMEOUT(fixture.transport.fetches.size() >= 2, 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(),
                          readyAudioSnapshot(11, 3));
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(3),
                            1'000);
  QVERIFY(rowFlag(fixture.model.outputDevices(), 1, "setDefaultAvailable"));

  QVERIFY(fixture.model.setDeviceVolume(10, 0.6));
  QCOMPARE(fixture.transport.operations.size(), 2);
  const auto volume = fixture.transport.operations.constLast();
  QCOMPARE(volume.request.kind, OperationKind::SetVolume);
  QCOMPARE(volume.request.primary, (Handle{11, 10}));
  QCOMPARE(volume.request.volume, 0.6);
  fixture.transport.finish(volume, audioResult(OperationKind::SetVolume,
                                               OperationStatus::Succeeded, 11,
                                               3));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);

  QVERIFY(fixture.model.setDeviceMuted(20, true));
  QCOMPARE(fixture.transport.operations.constLast().request.kind,
           OperationKind::SetMute);
  QCOMPARE(fixture.transport.operations.constLast().request.muted, true);
  fixture.transport.finish(
      fixture.transport.operations.constLast(),
      audioResult(OperationKind::SetMute, OperationStatus::Succeeded, 11, 3));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);

  QVERIFY(fixture.model.setStreamVolume(30, 0.8));
  const auto streamVolume = fixture.transport.operations.constLast();
  QCOMPARE(streamVolume.request.kind, OperationKind::SetVolume);
  QCOMPARE(streamVolume.request.primary, (Handle{11, 30}));
  QCOMPARE(streamVolume.request.volume, 0.8);
  fixture.transport.finish(streamVolume,
                           audioResult(OperationKind::SetVolume,
                                       OperationStatus::Succeeded, 11, 3));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);

  QVERIFY(fixture.model.setStreamMuted(40, false));
  QCOMPARE(fixture.transport.operations.constLast().request.primary,
           (Handle{11, 40}));
  QCOMPARE(fixture.transport.operations.constLast().request.muted, false);
  fixture.transport.finish(
      fixture.transport.operations.constLast(),
      audioResult(OperationKind::SetMute, OperationStatus::Succeeded, 11, 3));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);

  // Hostile inputs are refused locally and never dispatched.
  const qsizetype dispatched = fixture.transport.operations.size();
  QVERIFY(!fixture.model.setDeviceVolume(10, 1.5));
  QVERIFY(!fixture.model.setDeviceVolume(10, -0.1));
  QVERIFY(!fixture.model.setDeviceVolume(10,
                                         std::numeric_limits<double>::quiet_NaN()));
  QVERIFY(!fixture.model.setDefaultDevice(999));
  QVERIFY(!fixture.model.setStreamVolume(10, 0.5));
  QVERIFY(!fixture.model.setDeviceMuted(30, true));
  QCOMPARE(fixture.transport.operations.size(), dispatched);
}

void AudioSettingsModelTest::availabilityEqualsClientAdmission() {
  Fixture fixture;

  // While one operation is pending, every row action is displayed
  // unavailable and every dispatch is refused with nothing sent.
  QVERIFY(fixture.model.setDeviceVolume(10, 0.5));
  for (const QVariant &row : fixture.model.outputDevices()) {
    QVERIFY(!row.toMap().value(QStringLiteral("setDefaultAvailable")).toBool());
    QVERIFY(!row.toMap().value(QStringLiteral("volumeAvailable")).toBool());
    QVERIFY(!row.toMap().value(QStringLiteral("muteAvailable")).toBool());
  }
  for (const QVariant &row : fixture.model.inputDevices()) {
    QVERIFY(!row.toMap().value(QStringLiteral("volumeAvailable")).toBool());
  }
  for (const QVariant &row : fixture.model.streams()) {
    QVERIFY(!row.toMap().value(QStringLiteral("volumeAvailable")).toBool());
    QVERIFY(!row.toMap().value(QStringLiteral("muteAvailable")).toBool());
  }
  const qsizetype dispatched = fixture.transport.operations.size();
  QVERIFY(!fixture.model.setDefaultDevice(12));
  QVERIFY(!fixture.model.setDeviceVolume(12, 0.5));
  QVERIFY(!fixture.model.setStreamVolume(30, 0.5));
  QVERIFY(!fixture.model.reload());
  QCOMPARE(fixture.transport.operations.size(), dispatched);

  fixture.transport.finish(
      fixture.transport.operations.constLast(),
      audioResult(OperationKind::SetVolume, OperationStatus::Succeeded, 11, 2));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QVERIFY(rowFlag(fixture.model.outputDevices(), 1, "setDefaultAvailable"));

  // An unavailable snapshot (service Starting) disables every action even
  // though inventory may be retained.
  Snapshot starting = readyAudioSnapshot(11, 3);
  starting.availability = Availability::Starting;
  fixture.transport.invalidate(QStringLiteral(":1.7"), 11, 3);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.transport.fetches.size(), 2, 1'000);
  fixture.transport.reply(fixture.transport.fetches.constLast(), starting);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.loading(), 1'000);
  QVERIFY(!rowFlag(fixture.model.outputDevices(), 1, "setDefaultAvailable"));
  QVERIFY(!rowFlag(fixture.model.streams(), 0, "volumeAvailable"));
  QVERIFY(!fixture.model.setDefaultDevice(12));
  QCOMPARE(fixture.transport.operations.size(), dispatched);
}

void AudioSettingsModelTest::reportsFailuresWithoutCredentialOrRadioSurface() {
  Fixture fixture;
  QVERIFY(fixture.model.setDefaultDevice(12));
  fixture.transport.finish(
      fixture.transport.operations.constLast(),
      audioResult(OperationKind::SetDefault, OperationStatus::Failed, 11, 2,
                  QStringLiteral("stale-handle")));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(
      fixture.model.errorText().contains(QStringLiteral("refreshed")),
      1'000);
  QVERIFY(fixture.model.operationStatusText().isEmpty());

  QVERIFY(fixture.model.setDeviceVolume(10, 0.5));
  fixture.transport.fail(fixture.transport.operations.constLast(),
                         QStringLiteral("transport-failed"));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.errorText().contains(
                               QStringLiteral("could not be confirmed")),
                           1'000);
}

void AudioSettingsModelTest::disablesActionsWhenCapabilitiesDisappear() {
  Fixture fixture;
  Snapshot restricted = readyAudioSnapshot(11, 5);
  // The protocol invalidates can-set flags their capability no longer backs.
  restricted.capabilities = Capability::SetVolume;
  for (Device &device : restricted.outputs) {
    device.canSetMute = false;
  }
  for (Device &device : restricted.inputs) {
    device.canSetMute = false;
  }
  for (Stream &stream : restricted.streams) {
    stream.canSetMute = false;
    stream.canMove = false;
  }
  fixture.transport.invalidate(QStringLiteral(":1.7"), 11, 5);
  QTRY_VERIFY(fixture.transport.fetches.size() >= 2);
  fixture.transport.reply(fixture.transport.fetches.constLast(), restricted);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(5),
                            1'000);

  QVERIFY(rowFlag(fixture.model.outputDevices(), 0, "volumeAvailable"));
  QVERIFY(!rowFlag(fixture.model.outputDevices(), 0, "setDefaultAvailable"));
  QVERIFY(!rowFlag(fixture.model.outputDevices(), 0, "muteAvailable"));
  QVERIFY(!rowFlag(fixture.model.streams(), 0, "muteAvailable"));
  const qsizetype dispatched = fixture.transport.operations.size();
  QVERIFY(!fixture.model.setDefaultDevice(12));
  QVERIFY(!fixture.model.setDeviceMuted(20, true));
  QVERIFY(!fixture.model.setStreamMuted(40, true));
  QCOMPARE(fixture.transport.operations.size(), dispatched);
  QVERIFY(fixture.model.setDeviceVolume(10, 0.5));
  QCOMPARE(fixture.transport.operations.size(), dispatched + 1);
}

void AudioSettingsModelTest::keepsDegradedTruthAdmittedAndLabeled() {
  Fixture fixture;
  Snapshot degraded = readyAudioSnapshot(11, 4);
  degraded.availability = Availability::Degraded;
  degraded.reasonCode = QStringLiteral("truncated-graph");
  fixture.transport.invalidate(QStringLiteral(":1.7"), 11, 4);
  QTRY_VERIFY(fixture.transport.fetches.size() >= 2);
  fixture.transport.reply(fixture.transport.fetches.constLast(), degraded);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.degraded(), 1'000);
  QVERIFY(fixture.model.statusText().contains(QStringLiteral("limited")));

  // A degraded snapshot is still dispatch-admitted by the public client, so
  // displayed availability must stay admitted with it.
  QVERIFY(rowFlag(fixture.model.outputDevices(), 1, "setDefaultAvailable"));
  QVERIFY(rowFlag(fixture.model.streams(), 0, "volumeAvailable"));
  QVERIFY(fixture.model.setDefaultDevice(12));
  QCOMPARE(fixture.transport.operations.size(), 1);
}

void AudioSettingsModelTest::uncertainOutcomesAreVisibleAndNeverReplayed() {
  Fixture fixture;
  fixture.client.setRequestTimeout(20);
  QVERIFY(fixture.model.setDeviceVolume(10, 0.5));
  QVERIFY(fixture.model.busy());
  QTest::qWait(150);
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.errorText().contains(
                               QStringLiteral("could not be confirmed")),
                           1'000);
  QVERIFY(fixture.model.errorText().contains(
      QStringLiteral("before you try again")));
  // The timed-out mutation is never replayed by the route.
  QCOMPARE(fixture.transport.operations.size(), 1);
  QVERIFY(fixture.transport.fetches.size() >= 2);
}

QTEST_MAIN(AudioSettingsModelTest)
#include "tst_audio_settings_model.moc"
