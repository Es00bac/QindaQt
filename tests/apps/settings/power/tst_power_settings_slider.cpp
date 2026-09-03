// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_settings_test_support.h"

#include <qindaqt/apps/settings_power/power_settings_model.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Apps::SettingsPower::TestSupport;

class PowerSettingsSliderTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void burstCoalescesToOneExactRawRequest();
  void invalidAndAuthorityChangedDebouncesSendNothing();
  void unchangedNormalizedAndRawEquivalentDispatchNothing_data();
  void unchangedNormalizedAndRawEquivalentDispatchNothing();
};

void PowerSettingsSliderTest::burstCoalescesToOneExactRawRequest() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);
  const QString id = model.keyboardBrightnessRows().first().toMap()
                         .value(QStringLiteral("id")).toString();

  QVERIFY(model.requestKeyboardBrightness(id, 2'000));
  QVERIFY(model.requestKeyboardBrightness(id, 5'000));
  QVERIFY(model.requestKeyboardBrightness(id, 9'000));
  QCOMPARE(transport.submissions.size(), 0);
  QVERIFY(model.keyboardBrightnessRows().first().toMap()
              .value(QStringLiteral("available")).toBool());
  QTRY_COMPARE_WITH_TIMEOUT(transport.submissions.size(), 1, 500);
  QCOMPARE(transport.submissions.first().request.kind,
           Power::OperationKind::SetKeyboardBrightness);
  QCOMPARE(transport.submissions.first().request.value, quint32(9));
  QVERIFY(model.busy());

  transport.finishOperation(transport.submissions.first(),
                            success(transport.submissions.first(), 8));
  Power::Snapshot converged = readySnapshot(41, 8);
  converged.keyboardBacklights.first().value = 9;
  converged.keyboardBacklights.first().normalized = 9'000;
  transport.finishSnapshot(QStringLiteral(":1.80"),
                           transport.fetches.constLast().second, converged);
  QTRY_VERIFY(!model.busy());
}

void PowerSettingsSliderTest::invalidAndAuthorityChangedDebouncesSendNothing() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);
  const QString id = model.keyboardBrightnessRows().first().toMap()
                         .value(QStringLiteral("id")).toString();

  QVERIFY(!model.requestKeyboardBrightness(id, 10'001));
  QVERIFY(!model.requestKeyboardBrightness(QStringLiteral("keyboard-stale"),
                                           4'000));
  QVERIFY(model.requestKeyboardBrightness(id, 4'000));
  transport.announceOwner(QStringLiteral(":1.99"));
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 0);
  QVERIFY(model.errorText().contains(QStringLiteral("no change was sent")));
}

void PowerSettingsSliderTest::unchangedNormalizedAndRawEquivalentDispatchNothing_data() {
  QTest::addColumn<int>("normalized");
  QTest::newRow("exact-normalized") << 5'000;
  QTest::newRow("same-raw-after-conversion") << 5'001;
}

void PowerSettingsSliderTest::unchangedNormalizedAndRawEquivalentDispatchNothing() {
  // AGENT-NOTE: P2-2 regression — both exact normalized equality and a
  // distinct normalized value mapping to the current raw value are no-ops.
  QFETCH(int, normalized);
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);
  publish(client, transport);
  const QString id = model.keyboardBrightnessRows().first().toMap()
                         .value(QStringLiteral("id")).toString();

  QVERIFY(model.requestKeyboardBrightness(id, 4'000));
  QVERIFY(model.busy());
  QVERIFY(model.requestKeyboardBrightness(id, normalized));
  QTest::qWait(180);
  QCOMPARE(transport.submissions.size(), 0);
  QVERIFY(!model.busy());
  QVERIFY(model.operationStatusText().isEmpty());
  QVERIFY(model.errorText().isEmpty());
}

QTEST_GUILESS_MAIN(PowerSettingsSliderTest)
#include "tst_power_settings_slider.moc"
