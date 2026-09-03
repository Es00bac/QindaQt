// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_settings_test_support.h"

#include <qindaqt/apps/settings_power/power_settings_model.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Apps::SettingsPower::TestSupport;

class PowerSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsTruthAndPoisonsSessionActions();
  void sharesProfileAdmissionAndConvergenceFence();
  void ownerReplacementClearsActionableTruth();
  void staleSnapshotClosesPresentationAndAdmission();
  void successfulRetryClearsReconnectStatus();
};

void PowerSettingsModelTest::projectsTruthAndPoisonsSessionActions() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);

  QVERIFY(model.ready());
  QCOMPARE(model.serviceEpoch(), qulonglong(41));
  QCOMPARE(model.supplyRows().size(), 2);
  const QVariantMap battery = model.supplyRows().at(1).toMap();
  QCOMPARE(battery.value(QStringLiteral("stateText")).toString(),
           QStringLiteral("Discharging"));
  QCOMPARE(battery.value(QStringLiteral("warningText")).toString(),
           QStringLiteral("Low charge"));
  QVERIFY(battery.value(QStringLiteral("timeText")).toString()
              .contains(QStringLiteral("remaining")));
  QCOMPARE(model.profileRows().size(), 3);
  QCOMPARE(model.profileHoldRows().size(), 1);
  QCOMPARE(model.profileHoldRows().first().toMap()
               .value(QStringLiteral("reason")).toString(),
           QStringLiteral("Rendering preview"));
  QCOMPARE(model.internalBrightnessRows().first().toMap()
               .value(QStringLiteral("rawText")).toString(),
           QStringLiteral("Raw 421 of 937"));
  QVERIFY(!model.internalBrightnessRows().first().toMap()
               .value(QStringLiteral("available")).toBool());
  QVERIFY(!model.sessionActionsSupported());

  const QMetaObject *meta = model.metaObject();
  QCOMPARE(meta->indexOfMethod("suspend()"), -1);
  QCOMPARE(meta->indexOfMethod("shutdown()"), -1);
  QCOMPARE(meta->indexOfMethod("lock()"), -1);
}

void PowerSettingsModelTest::sharesProfileAdmissionAndConvergenceFence() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);

  const QVariantList rows = model.profileRows();
  QVERIFY(!rows.at(1).toMap().value(QStringLiteral("available")).toBool());
  QVERIFY(rows.at(0).toMap().value(QStringLiteral("available")).toBool());
  QVERIFY(model.requestProfile(QStringLiteral("power-saver")));
  QCOMPARE(transport.submissions.size(), 1);
  QCOMPARE(transport.submissions.first().request.kind,
           Power::OperationKind::SetProfile);
  QCOMPARE(transport.submissions.first().request.profileId,
           QStringLiteral("power-saver"));
  QVERIFY(model.busy());
  for (const QVariant &row : model.profileRows())
    QVERIFY(!row.toMap().value(QStringLiteral("available")).toBool());

  transport.finishOperation(transport.submissions.first(),
                            success(transport.submissions.first(), 8));
  QCoreApplication::processEvents();
  QVERIFY(model.busy());
  Power::Snapshot converged = readySnapshot(41, 8);
  converged.profiles.activeProfileId = QStringLiteral("power-saver");
  transport.finishSnapshot(QStringLiteral(":1.80"),
                           transport.fetches.constLast().second, converged);
  QTRY_VERIFY(!model.busy());
  QVERIFY(model.errorText().isEmpty());
}

void PowerSettingsModelTest::ownerReplacementClearsActionableTruth() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);
  QVERIFY(model.requestProfile(QStringLiteral("performance")));

  transport.announceOwner(QStringLiteral(":1.81"));
  QTRY_VERIFY(!model.busy());
  QVERIFY(model.loading());
  QVERIFY(model.supplyRows().isEmpty());
  QVERIFY(model.profileRows().isEmpty());
  QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
  QCOMPARE(transport.submissions.size(), 1);
}

void PowerSettingsModelTest::staleSnapshotClosesPresentationAndAdmission() {
  // AGENT-NOTE: P2-1 regression — a retained snapshot after a failed newer
  // revision fetch must not outlive the page's "controls unavailable" truth.
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);
  const QString keyboardId = model.keyboardBrightnessRows().first().toMap()
                                 .value(QStringLiteral("id")).toString();

  Q_EMIT transport.invalidated(QStringLiteral(":1.80"), 41, 8);
  const quint64 fetchId = transport.fetches.constLast().second;
  Q_EMIT transport.snapshotReply(QStringLiteral(":1.80"), fetchId, false,
                                 Power::Snapshot{}, QStringLiteral("boom"));

  QVERIFY(model.stale());
  QVERIFY(model.statusText().contains(QStringLiteral("Controls are unavailable")));
  for (const QVariant &row : model.profileRows())
    QVERIFY(!row.toMap().value(QStringLiteral("available")).toBool());
  for (const QVariant &row : model.internalBrightnessRows())
    QVERIFY(!row.toMap().value(QStringLiteral("available")).toBool());
  for (const QVariant &row : model.keyboardBrightnessRows())
    QVERIFY(!row.toMap().value(QStringLiteral("available")).toBool());

  QVERIFY(!model.requestProfile(QStringLiteral("power-saver")));
  QVERIFY(!model.requestKeyboardBrightness(keyboardId, 4'000));
  QCOMPARE(transport.submissions.size(), 0);
}

void PowerSettingsModelTest::successfulRetryClearsReconnectStatus() {
  // AGENT-NOTE: P3-1 regression — reconnect feedback is transient and must
  // clear when retry publishes a fresh authoritative snapshot.
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);

  QVERIFY(model.retry());
  QVERIFY(model.operationStatusText().contains(QStringLiteral("Reconnecting")));
  transport.announceOwner(QStringLiteral(":1.80"));
  transport.finishSnapshot(QStringLiteral(":1.80"),
                           transport.fetches.constLast().second,
                           readySnapshot());

  QVERIFY(model.ready());
  QVERIFY(model.operationStatusText().isEmpty());
  QVERIFY(model.errorText().isEmpty());
}

QTEST_GUILESS_MAIN(PowerSettingsModelTest)
#include "tst_power_settings_model.moc"
