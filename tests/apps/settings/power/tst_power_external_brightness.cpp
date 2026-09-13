// SPDX-License-Identifier: GPL-3.0-or-later

#include "external_brightness_test_support.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Apps::SettingsPower::TestSupport::External;
using Display::ErrorCode;
using Display::OperationStatus;

class PowerExternalBrightnessTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsOneTruthfulRowPerExternalOutput();
  void queuedChangesCoalesceToOneExactRequest();
  void delayedReplyConvergesOnObservedRepublication();
  void noOpAndStaleTargetsSendNothing();
  void refusedOutputsStayVisibleAndCannotDispatch_data();
  void refusedOutputsStayVisibleAndCannotDispatch();
  void typedOutcomesAndOwnerLossAreNeverReplayed();
};

void PowerExternalBrightnessTest::projectsOneTruthfulRowPerExternalOutput() {
  Route route;
  QVERIFY(route.model.rows().isEmpty());
  route.publish(topology(), brightness(topology()));

  // The internal panel stays with Power1 and the disabled output is hidden.
  QCOMPARE(route.model.rows().size(), 3);
  const QVariantMap studio = route.row(0);
  QCOMPARE(studio.value(QStringLiteral("id")).toString(), QStringLiteral("external-1"));
  QCOMPARE(studio.value(QStringLiteral("name")).toString(), QStringLiteral("Studio monitor"));
  QVERIFY(studio.value(QStringLiteral("known")).toBool());
  QCOMPARE(studio.value(QStringLiteral("normalized")).toUInt(), 6'000U);
  QVERIFY(studio.value(QStringLiteral("settable")).toBool());
  QVERIFY(studio.value(QStringLiteral("available")).toBool());
  QVERIFY(studio.value(QStringLiteral("reason")).toString().isEmpty());
  const QVariantMap projector = route.row(1);
  QCOMPARE(projector.value(QStringLiteral("id")).toString(), QStringLiteral("external-2"));
  QCOMPARE(projector.value(QStringLiteral("normalized")).toUInt(), 2'500U);
  QVERIFY(projector.value(QStringLiteral("available")).toBool());

  const QVariantMap television = route.row(2);
  QCOMPARE(television.value(QStringLiteral("name")).toString(), QStringLiteral("Office TV"));
  QVERIFY(!television.value(QStringLiteral("known")).toBool());
  QVERIFY(!television.value(QStringLiteral("settable")).toBool());
  QVERIFY(!television.value(QStringLiteral("available")).toBool());
  QVERIFY(television.value(QStringLiteral("reason")).toString()
              .contains(QStringLiteral("does not offer brightness control")));
  QVERIFY(television.value(QStringLiteral("accessibleDescription")).toString()
              .contains(QStringLiteral("read-only")));

  QVERIFY(!route.model.requestBrightness(QStringLiteral("external-3"), 5'000));
  QVERIFY(!route.model.requestBrightness(QStringLiteral("external-9"), 5'000));
  QVERIFY(!route.model.requestBrightness(QStringLiteral("external-1"), 10'001));
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 0);
}

void PowerExternalBrightnessTest::queuedChangesCoalesceToOneExactRequest() {
  Route route;
  route.publish(topology(), brightness(topology()));

  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 2'000));
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 5'000));
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 8'000));
  QVERIFY(route.model.busy());
  QVERIFY(route.row(0).value(QStringLiteral("available")).toBool());
  // One serialized gesture: the second monitor is fenced meanwhile.
  QVERIFY(!route.row(1).value(QStringLiteral("available")).toBool());
  QVERIFY(!route.model.requestBrightness(QStringLiteral("external-2"), 1'000));
  QCOMPARE(route.transport.brightnessSubmissions.size(), 0);

  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 1, 500);
  const auto &submission = route.transport.brightnessSubmissions.constFirst();
  QCOMPARE(submission.owner, kOwner);
  QVERIFY(submission.request == (Display::BrightnessRequest{
                                    .baseEpoch = kEpoch,
                                    .baseRevision = 9,
                                    .stableId = QStringLiteral("conn:DP-1"),
                                    .value = 8'000}));
  QVERIFY(!route.row(0).value(QStringLiteral("available")).toBool());
  QVERIFY(route.model.operationStatusText().contains(QStringLiteral("Applying")));
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 1);
}

void PowerExternalBrightnessTest::delayedReplyConvergesOnObservedRepublication() {
  Route route;
  route.publish(topology(), brightness(topology()));
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 8'000));
  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 1, 500);

  // Display1 replies only after it observes the value, and its Changed hint
  // and observed republication arrive before that reply.
  QTest::qWait(50);
  QVERIFY(route.model.busy());
  const Display::BrightnessSnapshot observed =
      withValue(brightness(topology()), QStringLiteral("DP-1"), 8'000, 10);
  route.republish(topology(), observed);
  QVERIFY(route.model.busy());
  QCOMPARE(route.row(0).value(QStringLiteral("normalized")).toUInt(), 8'000U);
  route.finish(0, immediate(OperationStatus::Succeeded, 9, 10));
  QTRY_VERIFY_WITH_TIMEOUT(!route.model.busy(), 1'000);
  QVERIFY(route.model.errorText().isEmpty());
  QVERIFY(route.row(0).value(QStringLiteral("available")).toBool());

  // An authoritative republication at a different value ends the wait at once.
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 3'000));
  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 2, 500);
  QCOMPARE(route.transport.brightnessSubmissions.constLast().request.baseRevision,
           quint64{10});
  route.finish(1, immediate(OperationStatus::Succeeded, 10, 11));
  QTest::qWait(50);
  QVERIFY(route.model.busy());
  route.republish(topology(), withValue(observed, QStringLiteral("DP-1"), 3'400, 11));
  QTRY_VERIFY_WITH_TIMEOUT(!route.model.busy(), 1'000);
  QVERIFY(route.model.errorText().contains(QStringLiteral("different brightness")));
  QCOMPARE(route.row(0).value(QStringLiteral("normalized")).toUInt(), 3'400U);
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 2);
}

void PowerExternalBrightnessTest::noOpAndStaleTargetsSendNothing() {
  Route route;
  route.publish(topology(), brightness(topology()));

  // Returning a gesture to observed truth cancels its queued predecessor.
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 7'000));
  QVERIFY(route.model.busy());
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 6'000));
  QVERIFY(!route.model.busy());
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 0);

  // A brightness republication during the debounce refuses the stale gesture.
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-2"), 9'000));
  route.republish(topology(),
                  withValue(brightness(topology()), QStringLiteral("DP-1"), 5'000, 10));
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 0);
  QVERIFY(route.model.errorText().contains(QStringLiteral("before the request could be sent")));

  // A topology replacement behind the same row ID cancels the queued gesture.
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-2"), 9'000));
  Display::Snapshot unplugged = topology(5);
  unplugged.outputs.removeFirst();
  unplugged.outputs.first().primary = true;
  route.republish(unplugged, brightness(unplugged, 11));
  QVERIFY(route.model.errorText().contains(QStringLiteral("no change was sent")));
  QCOMPARE(route.row(1).value(QStringLiteral("name")).toString(), QStringLiteral("Office TV"));
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 0);
}

void PowerExternalBrightnessTest::refusedOutputsStayVisibleAndCannotDispatch_data() {
  QTest::addColumn<Display::Snapshot>("topologyValue");
  QTest::addColumn<Display::BrightnessSnapshot>("brightnessValue");
  QTest::addColumn<bool>("degrade");
  QTest::addColumn<QString>("reasonFragment");

  Display::Snapshot replica = topology();
  replica.outputs[2].replicationSourceStableId = QStringLiteral("conn:DP-1");
  QTest::newRow("replica") << replica << brightness(replica) << false
                           << QStringLiteral("mirror");

  Display::Snapshot ambiguous = topology();
  ambiguous.outputs[2].ambiguousIdentity = true;
  QTest::newRow("ambiguous") << ambiguous << brightness(ambiguous) << false
                             << QStringLiteral("identified");

  Display::BrightnessSnapshot unobserved = brightness(topology());
  unobserved.outputs[2].observed = false;
  unobserved.outputs[2].value = 0;
  QTest::newRow("unobserved") << topology() << unobserved << false
                              << QStringLiteral("not reported");

  Display::Snapshot arranging = topology();
  arranging.transactions = {{.transactionId = QStringLiteral("tx-1"),
                             .state = Display::TransactionState::Applying,
                             .reason = Display::TransactionReason::None,
                             .initiatingEpoch = kEpoch,
                             .baseRevision = 4,
                             .observedRevision = 4,
                             .deadlineMonotonicMilliseconds = 0,
                             .revertAttempt = 0}};
  QTest::newRow("arrangement-in-progress") << arranging << brightness(arranging)
                                           << false << QString();

  QTest::newRow("degraded-client") << topology() << brightness(topology()) << true
                                   << QString();
}

void PowerExternalBrightnessTest::refusedOutputsStayVisibleAndCannotDispatch() {
  QFETCH(Display::Snapshot, topologyValue);
  QFETCH(Display::BrightnessSnapshot, brightnessValue);
  QFETCH(bool, degrade);
  QFETCH(QString, reasonFragment);
  Route route;
  route.publish(topologyValue, brightnessValue);
  if (degrade) {
    route.transport.publishInvalidation(kOwner, kEpoch, topologyValue.revision);
    route.transport.replySnapshot(route.transport.fetches.constLast(), {}, false,
                                  QStringLiteral("transport-error"));
    QCOMPARE(route.client.state(), DisplayClient::ClientState::Degraded);
  }

  QCOMPARE(route.model.rows().size(), 3);
  const QVariantMap refused = route.row(1);
  QCOMPARE(refused.value(QStringLiteral("name")).toString(), QStringLiteral("Projector"));
  QVERIFY(!refused.value(QStringLiteral("available")).toBool());
  if (!reasonFragment.isEmpty()) {
    QVERIFY(!refused.value(QStringLiteral("settable")).toBool());
    QVERIFY2(refused.value(QStringLiteral("reason")).toString()
                 .contains(reasonFragment, Qt::CaseInsensitive),
             qPrintable(refused.value(QStringLiteral("reason")).toString()));
    QVERIFY(refused.value(QStringLiteral("accessibleDescription")).toString()
                .contains(QStringLiteral("read-only")));
  }
  QVERIFY(!route.model.requestBrightness(QStringLiteral("external-2"), 1'000));
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 0);
}

void PowerExternalBrightnessTest::typedOutcomesAndOwnerLossAreNeverReplayed() {
  Route route;
  route.publish(topology(), brightness(topology()));

  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 8'000));
  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 1, 500);
  route.finish(0, immediate(OperationStatus::Busy, 9, 9, QStringLiteral("compositor-busy"),
                            ErrorCode::TransactionActive));
  QTRY_VERIFY_WITH_TIMEOUT(!route.model.busy(), 1'000);
  QVERIFY(route.model.errorText().contains(QStringLiteral("busy")));

  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 7'000));
  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 2, 500);
  route.finish(1, immediate(OperationStatus::Rejected, 9, 9,
                            QStringLiteral("brightness-unsupported"),
                            ErrorCode::CompositorRejected));
  QTRY_VERIFY_WITH_TIMEOUT(!route.model.busy(), 1'000);
  QVERIFY(route.model.errorText().contains(QStringLiteral("does not accept brightness")));

  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 6'500));
  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 3, 500);
  route.finish(2, immediate(OperationStatus::Uncertain, 9, 9,
                            QStringLiteral("brightness-observation-timeout"),
                            ErrorCode::Timeout));
  QTRY_VERIFY_WITH_TIMEOUT(!route.model.busy(), 1'000);
  QVERIFY(route.model.errorText().contains(QStringLiteral("not replayed")));

  // Owner loss during the delayed reply: no replay to the replacement owner.
  QVERIFY(route.model.requestBrightness(QStringLiteral("external-1"), 4'000));
  QTRY_COMPARE_WITH_TIMEOUT(route.transport.brightnessSubmissions.size(), 4, 500);
  route.transport.publishOwner(QStringLiteral(":1.91"));
  QTRY_VERIFY_WITH_TIMEOUT(!route.model.busy(), 1'000);
  QVERIFY(route.model.errorText().contains(QStringLiteral("not replayed")));
  QVERIFY(route.model.rows().isEmpty());
  route.transport.replySnapshot(route.transport.fetches.constLast(), topology());
  route.transport.replyBrightness(route.transport.brightnessFetches.constLast(),
                                  brightness(topology()));
  route.finish(3, immediate(OperationStatus::Succeeded, 9, 10));
  QTest::qWait(180);
  QCOMPARE(route.transport.brightnessSubmissions.size(), 4);
  QVERIFY(route.row(0).value(QStringLiteral("available")).toBool());
  QVERIFY(!route.model.busy());
}

QTEST_GUILESS_MAIN(PowerExternalBrightnessTest)
#include "tst_power_external_brightness.moc"
