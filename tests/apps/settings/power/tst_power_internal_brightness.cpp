// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_settings_test_support.h"

#include <qindaqt/apps/settings_power/power_settings_model.h>
#include <qindaqt/services/brightness_model/brightness_math.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Apps::SettingsPower::TestSupport;

namespace {

const QString kOwner = QStringLiteral(":1.80");
const QString kPanelRow = QStringLiteral("internal-41-1");

QVariantMap internalRow(const PowerSettingsModel &model, const qsizetype index = 0) {
  return model.internalBrightnessRows().at(index).toMap();
}

Power::InternalBacklight panel(const QString &id, const Power::BacklightKind kind) {
  Power::InternalBacklight device = readySnapshot().internalBacklights.constFirst();
  device.handle.opaqueId = id;
  device.deviceName = id;
  device.kind = kind;
  return device;
}

Power::OperationResult result(const FakePowerTransport::Submission &submission,
                              const Power::OperationStatus status,
                              const quint64 initiatingRevision,
                              const quint64 observedRevision) {
  Power::OperationResult value = success(submission, observedRevision);
  value.status = status;
  value.initiatingRevision = initiatingRevision;
  return value;
}

} // namespace

class PowerInternalBrightnessTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void admittedPanelCoalescesToOneExactRawRequest();
  void convergesOnlyOnObservedReadback();
  void refusedPanelStatesStayDisabledAndSendNothing_data();
  void refusedPanelStatesStayDisabledAndSendNothing();
  void uncertainAndOwnerLossAreNeverReplayed();
  void changedLineageBeforeDispatchSendsNothing();
};

void PowerInternalBrightnessTest::admittedPanelCoalescesToOneExactRawRequest() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);
  QCOMPARE(internalRow(model).value(QStringLiteral("id")).toString(), kPanelRow);
  QVERIFY(internalRow(model).value(QStringLiteral("available")).toBool());
  const QString keyboardRow = model.keyboardBrightnessRows().first().toMap()
                                  .value(QStringLiteral("id")).toString();

  QVERIFY(model.requestInternalBrightness(kPanelRow, 2'000));
  QVERIFY(model.requestInternalBrightness(kPanelRow, 6'000));
  QVERIFY(model.requestInternalBrightness(kPanelRow, 8'000));
  QVERIFY(internalRow(model).value(QStringLiteral("available")).toBool());
  // One serialized brightness gesture: the other target is fenced meanwhile.
  QVERIFY(!model.requestKeyboardBrightness(keyboardRow, 1'000));
  QCOMPARE(transport.submissions.size(), 0);

  QTRY_COMPARE_WITH_TIMEOUT(transport.submissions.size(), 1, 500);
  const Power::PowerClientRequest request = transport.submissions.first().request;
  const auto raw = Brightness::denormalizeRaw(0, 937, 8'000);
  QVERIFY(raw.succeeded());
  QCOMPARE(request.kind, Power::OperationKind::SetInternalBrightness);
  QCOMPARE(request.handle, (Power::Handle{.epoch = 41,
                                          .opaqueId = QStringLiteral("internal-panel")}));
  QCOMPARE(request.value, raw.value);
  QVERIFY(model.busy());
  QVERIFY(!internalRow(model).value(QStringLiteral("available")).toBool());
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 1);
}

void PowerInternalBrightnessTest::convergesOnlyOnObservedReadback() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);

  QVERIFY(model.requestInternalBrightness(kPanelRow, 8'000));
  QTRY_COMPARE_WITH_TIMEOUT(transport.submissions.size(), 1, 500);
  const quint32 requested = transport.submissions.first().request.value;
  transport.finishOperation(transport.submissions.first(),
                            success(transport.submissions.first(), 8));
  QVERIFY(model.busy());
  Power::Snapshot converged = readySnapshot(41, 8);
  converged.internalBacklights.first().observed = requested;
  transport.finishSnapshot(kOwner, transport.fetches.constLast().second, converged);
  QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1'000);
  QVERIFY(model.errorText().isEmpty());
  QCOMPARE(internalRow(model).value(QStringLiteral("rawValue")).toUInt(), requested);

  // The kernel settles elsewhere: the observed value wins immediately instead
  // of holding the fence until the convergence timeout.
  QVERIFY(model.requestInternalBrightness(kPanelRow, 2'000));
  QTRY_COMPARE_WITH_TIMEOUT(transport.submissions.size(), 2, 500);
  transport.finishOperation(transport.submissions.last(),
                            result(transport.submissions.last(),
                                   Power::OperationStatus::Succeeded, 8, 9));
  Power::Snapshot settled = readySnapshot(41, 9);
  settled.internalBacklights.first().observed = 300;
  transport.finishSnapshot(kOwner, transport.fetches.constLast().second, settled);
  QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1'000);
  QVERIFY(model.errorText().contains(QStringLiteral("different brightness")));
  QCOMPARE(internalRow(model).value(QStringLiteral("rawValue")).toUInt(), 300U);
  QCOMPARE(transport.submissions.size(), 2);
}

void PowerInternalBrightnessTest::refusedPanelStatesStayDisabledAndSendNothing_data() {
  QTest::addColumn<Power::Snapshot>("snapshot");
  QTest::addColumn<int>("refusedIndex");
  QTest::addColumn<int>("availableCount");
  QTest::addColumn<QString>("reasonFragment");

  Power::Snapshot readOnly = readySnapshot();
  readOnly.internalBacklights.first().status = Power::BacklightStatus::Unavailable;
  readOnly.internalBacklights.first().reason = Power::BacklightReason::LogindError;
  readOnly.internalBacklights.first().diagnostic = QStringLiteral("backlight-read-only");
  QTest::newRow("read-only") << readOnly << 0 << 0 << QStringLiteral("read-only");

  Power::Snapshot ambiguous = readySnapshot();
  ambiguous.internalBacklights.append(
      panel(QStringLiteral("internal-second"), Power::BacklightKind::Firmware));
  QTest::newRow("ambiguous") << ambiguous << 0 << 0 << QStringLiteral("ambiguous");

  Power::Snapshot notSelected = readySnapshot();
  notSelected.internalBacklights.append(
      panel(QStringLiteral("internal-raw"), Power::BacklightKind::Raw));
  QTest::newRow("lower-preference") << notSelected << 1 << 1
                                    << QStringLiteral("Another backlight");

  Power::Snapshot degraded = readySnapshot();
  degraded.internalBacklights.first().status = Power::BacklightStatus::Degraded;
  degraded.internalBacklights.first().reason = Power::BacklightReason::DeviceDisappeared;
  degraded.internalBacklights.first().observedKnown = false;
  degraded.internalBacklights.first().observed = 0;
  QTest::newRow("degraded") << degraded << 0 << 0 << QStringLiteral("disappeared");

  Power::Snapshot noCapability = readySnapshot();
  noCapability.capabilities &= ~Power::Capabilities(Power::Capability::InternalBacklight);
  QTest::newRow("capability-absent") << noCapability << 0 << 0
                                     << QStringLiteral("unavailable");

  Power::Snapshot serviceUnavailable = readySnapshot();
  serviceUnavailable.availability = Power::Availability::Unavailable;
  serviceUnavailable.reasonCode = QStringLiteral("upower-unavailable");
  QTest::newRow("service-unavailable") << serviceUnavailable << 0 << 0 << QString();
}

void PowerInternalBrightnessTest::refusedPanelStatesStayDisabledAndSendNothing() {
  QFETCH(Power::Snapshot, snapshot);
  QFETCH(int, refusedIndex);
  QFETCH(int, availableCount);
  QFETCH(QString, reasonFragment);
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport, snapshot);

  int available = 0;
  for (const QVariant &row : model.internalBrightnessRows())
    available += row.toMap().value(QStringLiteral("available")).toBool() ? 1 : 0;
  QCOMPARE(available, availableCount);
  const QVariantMap refused = internalRow(model, refusedIndex);
  QVERIFY(!refused.value(QStringLiteral("available")).toBool());
  if (!reasonFragment.isEmpty()) {
    QVERIFY2(refused.value(QStringLiteral("reason")).toString()
                 .contains(reasonFragment, Qt::CaseInsensitive),
             qPrintable(refused.value(QStringLiteral("reason")).toString()));
    QVERIFY(refused.value(QStringLiteral("accessibleDescription")).toString()
                .contains(QStringLiteral("read-only")));
  }
  QVERIFY(!model.requestInternalBrightness(
      refused.value(QStringLiteral("id")).toString(), 1'000));
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 0);
}

void PowerInternalBrightnessTest::uncertainAndOwnerLossAreNeverReplayed() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);

  QVERIFY(model.requestInternalBrightness(kPanelRow, 8'000));
  QTRY_COMPARE_WITH_TIMEOUT(transport.submissions.size(), 1, 500);
  transport.finishOperation(transport.submissions.first(),
                            result(transport.submissions.first(),
                                   Power::OperationStatus::Uncertain, 7, 7));
  QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1'000);
  QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
  transport.finishSnapshot(kOwner, transport.fetches.constLast().second,
                           readySnapshot(41, 8));
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 1);

  QVERIFY(model.requestInternalBrightness(kPanelRow, 3'000));
  QTRY_COMPARE_WITH_TIMEOUT(transport.submissions.size(), 2, 500);
  transport.announceOwner(QStringLiteral(":1.81"));
  QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1'000);
  QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
  transport.finishSnapshot(QStringLiteral(":1.81"), transport.fetches.constLast().second,
                           readySnapshot(41, 9));
  QVERIFY(internalRow(model).value(QStringLiteral("available")).toBool());
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 2);
}

void PowerInternalBrightnessTest::changedLineageBeforeDispatchSendsNothing() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);

  // A newer revision re-resolves admission at dispatch and refuses the stale
  // gesture instead of sending a value chosen against older truth.
  QVERIFY(model.requestInternalBrightness(kPanelRow, 8'000));
  Power::Snapshot newer = readySnapshot(41, 8);
  newer.source.acPresent = false;
  Q_EMIT transport.invalidated(kOwner, 41, 8);
  transport.finishSnapshot(kOwner, transport.fetches.constLast().second, newer);
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 0);
  QVERIFY(model.errorText().contains(QStringLiteral("before the request could be sent")));

  // An epoch replacement cancels the queued gesture immediately.
  QVERIFY(model.requestInternalBrightness(kPanelRow, 6'000));
  Power::Snapshot replaced = readySnapshot(42, 1);
  Q_EMIT transport.invalidated(kOwner, 42, 1);
  transport.finishSnapshot(kOwner, transport.fetches.constLast().second, replaced);
  QVERIFY(model.errorText().contains(QStringLiteral("no change was sent")));
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 0);
}

QTEST_GUILESS_MAIN(PowerInternalBrightnessTest)
#include "tst_power_internal_brightness.moc"
