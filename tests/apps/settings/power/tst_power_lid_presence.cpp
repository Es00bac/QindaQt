// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_settings_test_support.h"

#include <qindaqt/apps/settings_power/power_settings_model.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtTest/QtTest>

using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Apps::SettingsPower::TestSupport;
namespace Power = QindaQt::Power;

namespace {

Power::Snapshot snapshotWithLid(const bool present, const bool closed = false) {
  Power::Snapshot snapshot = readySnapshot();
  snapshot.source.lidPresent = present;
  snapshot.source.lidClosed = closed;
  return snapshot;
}

// The client only refetches after an invalidation, so a fresh authoritative
// snapshot is delivered against the newest fetch request id and must carry a
// revision at or beyond the invalidated one to be accepted.
void republish(FakePowerTransport &transport, const Power::Snapshot &snapshot) {
  Power::Snapshot advanced = snapshot;
  advanced.revision = 8;
  Q_EMIT transport.invalidated(QStringLiteral(":1.80"), 41, 8);
  transport.finishSnapshot(QStringLiteral(":1.80"),
                           transport.fetches.constLast().second, advanced);
}

} // namespace

class PowerLidPresenceTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void lidIsHiddenUntilAdmittedTruthSaysOtherwise();
  void lidFollowsValidatedSnapshotTruth();
  void contradictoryLidTruthIsRejectedAndStaysHidden();
  void staleAuthorityHidesLidAgain();
};

void PowerLidPresenceTest::
    lidIsHiddenUntilAdmittedTruthSaysOtherwise() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);

  // No snapshot, no owner: the flag must fail closed.
  QVERIFY(!model.lidPresent());

  publish(client, transport, snapshotWithLid(false));
  QVERIFY(model.ready());
  QVERIFY(!model.lidPresent());

  republish(transport, snapshotWithLid(true));
  QVERIFY(model.ready());
  QVERIFY(model.lidPresent());
}

void PowerLidPresenceTest::lidFollowsValidatedSnapshotTruth() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);

  publish(client, transport, snapshotWithLid(true));
  QVERIFY(model.lidPresent());

  // A later authoritative snapshot without a lid hides the rows again.
  republish(transport, snapshotWithLid(false));
  QVERIFY(model.ready());
  QVERIFY(!model.lidPresent());
}

void PowerLidPresenceTest::contradictoryLidTruthIsRejectedAndStaysHidden() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);

  // Hostile/malformed wire truth: a closed lid with no lid present fails
  // protocol validation, so the whole snapshot is unadmittable and the page
  // must not display a lid.
  Power::Snapshot hostile = snapshotWithLid(false, true);
  const auto verdict = Power::validateSnapshot(hostile);
  QVERIFY(!verdict.accepted);

  publish(client, transport, hostile);
  QVERIFY(!model.ready());
  QVERIFY(!model.lidPresent());
}

void PowerLidPresenceTest::staleAuthorityHidesLidAgain() {
  FakePowerTransport transport;
  Power::PowerClient client(&transport);
  PowerSettingsModel model(client);

  publish(client, transport, snapshotWithLid(true));
  QVERIFY(model.lidPresent());

  // A failed newer fetch retains stale truth; presentation closes until
  // authoritative data returns, including lid visibility.
  Q_EMIT transport.invalidated(QStringLiteral(":1.80"), 41, 8);
  const quint64 fetchId = transport.fetches.constLast().second;
  Q_EMIT transport.snapshotReply(QStringLiteral(":1.80"), fetchId, false,
                                 Power::Snapshot{}, QStringLiteral("boom"));
  QVERIFY(model.stale());
  QVERIFY(!model.lidPresent());
}

QTEST_GUILESS_MAIN(PowerLidPresenceTest)
#include "tst_power_lid_presence.moc"
