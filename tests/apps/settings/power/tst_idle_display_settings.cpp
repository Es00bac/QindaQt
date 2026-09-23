// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_power/idle_display_settings.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;

namespace {
const QString key = QStringLiteral("power.idleDisplayOffMinutes");
const QString ownerA = QStringLiteral(":1.7");
const QString epochA = QStringLiteral("idle-epoch-a");

QVariantMap snapshotWire(QString epoch, quint64 revision, QVariant value) {
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), epoch},
          {QLatin1StringView(WireContract::FieldRevision), revision},
          {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
          {QLatin1StringView(WireContract::FieldSourceLayers),
           QVariantMap{{key, QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
QVariantMap commitWire(QString epoch, SettingsWireStatus status,
                       quint64 before, quint64 after, QVariant value,
                       QString message = {}) {
  const QStringList changed = status == SettingsWireStatus::Applied && after > before
      ? QStringList{key} : QStringList{};
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), epoch},
          {QLatin1StringView(WireContract::FieldRevisionBefore), before},
          {QLatin1StringView(WireContract::FieldRevisionAfter), after},
          {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
          {QLatin1StringView(WireContract::FieldSourceLayers),
           QVariantMap{{key, QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WireContract::FieldChangedKeys), changed},
          {QLatin1StringView(WireContract::FieldMessage), message}};
}

class FakeTransport final : public SettingsTransport {
public:
  bool start(QString *error) override { if (error) error->clear(); return true; }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &) override {
    snapshots.append({token, owner});
  }
  void commit(quint64 token, const QString &owner, const QString &epoch,
              quint64 revision, const QVariantList &operations) override {
    commits.append({token, owner, epoch, revision, operations});
  }
  void requestActivation() override {}
  struct SnapshotRequest { quint64 token; QString owner; };
  struct CommitRequest {
    quint64 token; QString owner; QString epoch; quint64 revision;
    QVariantList operations;
  };
  QList<SnapshotRequest> snapshots;
  QList<CommitRequest> commits;
};

struct Fixture {
  FakeTransport transport;
  SettingsClient client{transport, Settings1IdlePreferences::scopedKey(),
                        ClientTiming{150, 0, {10}}};
  Settings1IdlePreferences preferences{client};
  IdleDisplaySettingsModel model{preferences, client};

  Fixture() {
    const bool started = client.start();
    Q_ASSERT(started);
    Q_EMIT transport.ownerChanged(ownerA);
  }
  bool answer(quint64 revision, qint64 value,
              QString owner = ownerA, QString epoch = epochA) {
    if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 2000))
      return false;
    const auto request = transport.snapshots.takeFirst();
    if (request.owner != owner) return false;
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
        snapshotWire(epoch, revision, QVariant::fromValue(value)));
    return true;
  }
  bool ready(qint64 value = 20) {
    return answer(1, value) && model.available() && model.canEdit();
  }
  FakeTransport::CommitRequest lastCommit() const {
    Q_ASSERT(!transport.commits.isEmpty());
    return transport.commits.constLast();
  }
  void reply(SettingsWireStatus status, quint64 before, quint64 after,
             qint64 value, QString message = {}) {
    const auto request = lastCommit();
    Q_EMIT transport.commitReceived(request.token, request.owner,
        commitWire(request.epoch, status, before, after,
                   QVariant::fromValue(value), message));
  }
};
} // namespace

class IdleDisplaySettingsModelTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void absenceAndAdmission();
  void malformedSnapshotDoesNotClaimPolicy();
  void appliedNeedsFreshReadback();
  void mismatchAndRefusalStayVisible();
  void lostReplyAndOwnerReplacementNeverReplay();
  void newOwnerDisabledValueDoesNotRetainOldTimeout();
  void boundsAndDisabledDefault();
};

void IdleDisplaySettingsModelTest::absenceAndAdmission() {
  Fixture f;
  QVERIFY(!f.model.hasConfirmed());
  QVERIFY(!f.model.available());
  QVERIFY(!f.model.canEdit());
  QVERIFY(!f.model.enabled());
  QCOMPARE(f.model.minutes(), 0);
  QVERIFY(f.model.statusText().contains(QStringLiteral("not confirmed")));
  QVERIFY(!f.model.setEnabled(false));
  QVERIFY(f.transport.commits.isEmpty());
  QVERIFY(f.ready());
  QVERIFY(f.model.hasConfirmed());
  QCOMPARE(f.model.minutes(), 20);
  QVERIFY(f.model.enabled());
  f.client.refresh();
  QTRY_COMPARE(f.transport.snapshots.size(), 1);
  QVERIFY(!f.model.canEdit());
  QVERIFY(!f.model.setMinutes(15));
  QVERIFY(f.transport.commits.isEmpty());
  const QString admissionError = f.model.errorText();
  QVERIFY(!admissionError.isEmpty());
  QVERIFY(f.answer(1, 20));
  QVERIFY(f.model.canEdit());
  QCOMPARE(f.model.errorText(), admissionError);
  f.client.refresh();
  QVERIFY(f.answer(2, 30));
  QCOMPARE(f.model.minutes(), 30);
  QCOMPARE(f.model.errorText(), admissionError);
  QVERIFY(f.transport.commits.isEmpty());
}

void IdleDisplaySettingsModelTest::malformedSnapshotDoesNotClaimPolicy() {
  Fixture f;
  QTRY_COMPARE(f.transport.snapshots.size(), 1);
  const auto request = f.transport.snapshots.takeFirst();
  Q_EMIT f.transport.snapshotReceived(request.token, request.owner,
      snapshotWire(epochA, 1, QStringLiteral("20")));
  QVERIFY(!f.model.hasConfirmed());
  QVERIFY(!f.model.available());
  QVERIFY(!f.model.canEdit());
  QVERIFY(!f.model.enabled());
  QVERIFY(!f.model.setMinutes(15));
  QVERIFY(f.transport.commits.isEmpty());
  f.client.refresh();
  QVERIFY(f.answer(2, 20));
  QVERIFY(f.model.available());
  QVERIFY(f.model.hasConfirmed());
  QCOMPARE(f.model.minutes(), 20);
}

void IdleDisplaySettingsModelTest::appliedNeedsFreshReadback() {
  Fixture f;
  QVERIFY(f.ready());
  QVERIFY(f.model.setMinutes(15));
  QVERIFY(f.model.busy());
  QVERIFY(!f.model.canEdit());
  QCOMPARE(f.model.minutes(), 20);
  QCOMPARE(f.lastCommit().operations.constFirst().toMap()
               .value(QLatin1StringView(WireContract::FieldValue)).toLongLong(), 15);
  f.reply(SettingsWireStatus::Applied, 1, 2, 15);
  QVERIFY(f.model.busy());
  QCOMPARE(f.model.minutes(), 20);
  QVERIFY(f.model.statusText().contains(QStringLiteral("Checking")));
  QVERIFY(f.answer(1, 20));
  QVERIFY(f.model.busy());
  QCOMPARE(f.model.minutes(), 20);
  f.client.refresh();
  QVERIFY(f.answer(2, 15));
  QVERIFY(!f.model.busy());
  QVERIFY(f.model.available());
  QCOMPARE(f.model.minutes(), 15);
  QVERIFY(f.model.errorText().isEmpty());
  QCOMPARE(f.transport.commits.size(), 1);
}

void IdleDisplaySettingsModelTest::mismatchAndRefusalStayVisible() {
  Fixture f;
  QVERIFY(f.ready());
  QVERIFY(f.model.setMinutes(15));
  f.reply(SettingsWireStatus::Applied, 1, 2, 15);
  QVERIFY(f.answer(2, 30));
  QVERIFY(!f.model.busy());
  QVERIFY(f.model.conflict());
  QCOMPARE(f.model.minutes(), 30);
  QVERIFY(f.model.errorText().contains(QStringLiteral("differs")));
  f.client.refresh();
  QVERIFY(f.answer(2, 30));
  QVERIFY(f.model.conflict());
  QVERIFY(!f.model.errorText().isEmpty());
  QVERIFY(f.model.setMinutes(18));
  f.reply(SettingsWireStatus::PersistenceFailed, 2, 2, 30,
          QStringLiteral("disk full"));
  QVERIFY(!f.model.busy());
  QVERIFY(!f.model.conflict());
  QCOMPARE(f.model.minutes(), 30);
  QVERIFY(f.model.errorText().contains(QStringLiteral("disk full")));
  QVERIFY(f.answer(2, 30));
  QVERIFY(f.model.errorText().contains(QStringLiteral("disk full")));
  QVERIFY(f.model.retry());
  QVERIFY(f.answer(2, 30));
  QVERIFY(f.model.errorText().contains(QStringLiteral("disk full")));
  QCOMPARE(f.transport.commits.size(), 2);
}

void IdleDisplaySettingsModelTest::lostReplyAndOwnerReplacementNeverReplay() {
  Fixture lost;
  QVERIFY(lost.ready());
  QVERIFY(lost.model.setMinutes(15));
  auto request = lost.lastCommit();
  Q_EMIT lost.transport.requestFailed(request.token, request.owner,
      QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
      QStringLiteral("reply lost"));
  QVERIFY(lost.model.uncertain());
  QVERIFY(!lost.model.busy());
  QCOMPARE(lost.model.minutes(), 20);
  QVERIFY(lost.model.errorText().contains(QStringLiteral("may have happened")));
  QVERIFY(lost.answer(1, 20));
  QCOMPARE(lost.transport.commits.size(), 1);

  Fixture replaced;
  QVERIFY(replaced.ready());
  QVERIFY(replaced.model.setMinutes(15));
  Q_EMIT replaced.transport.ownerChanged(QStringLiteral(":1.8"));
  QVERIFY(replaced.model.uncertain());
  QVERIFY(!replaced.model.canEdit());
  QVERIFY(!replaced.model.available());
  QCOMPARE(replaced.model.minutes(), 20);
  Q_EMIT replaced.transport.commitReceived(replaced.lastCommit().token, ownerA,
      commitWire(epochA, SettingsWireStatus::Applied, 1, 2,
                 QVariant::fromValue<qint64>(15)));
  QVERIFY(replaced.model.uncertain());
  QVERIFY(replaced.answer(1, 10, QStringLiteral(":1.8"),
                          QStringLiteral("idle-epoch-b")));
  QVERIFY(replaced.model.canEdit());
  QCOMPARE(replaced.model.minutes(), 10);
  QCOMPARE(replaced.transport.commits.size(), 1);

  Fixture applied;
  QVERIFY(applied.ready());
  QVERIFY(applied.model.setMinutes(15));
  applied.reply(SettingsWireStatus::Applied, 1, 2, 15);
  QVERIFY(applied.model.busy());
  Q_EMIT applied.transport.ownerChanged(QStringLiteral(":1.8"));
  QVERIFY(applied.model.uncertain());
  QVERIFY(!applied.model.busy());
  // The old owner's post-Applied snapshot cannot confirm its retired write.
  Q_EMIT applied.transport.snapshotReceived(0, ownerA,
      snapshotWire(epochA, 2, QVariant::fromValue<qint64>(15)));
  QVERIFY(applied.answer(1, 25, QStringLiteral(":1.8"),
                         QStringLiteral("idle-epoch-b")));
  QCOMPARE(applied.model.minutes(), 25);
  QVERIFY(applied.model.uncertain());
  QCOMPARE(applied.transport.commits.size(), 1);
}

void IdleDisplaySettingsModelTest::newOwnerDisabledValueDoesNotRetainOldTimeout() {
  Fixture f;
  QVERIFY(f.ready(30));
  QCOMPARE(f.model.minutes(), 30);
  Q_EMIT f.transport.ownerChanged(QStringLiteral(":1.8"));
  QVERIFY(!f.model.available());
  // The retained old value stays visible only as last-confirmed truth.
  QCOMPARE(f.model.minutes(), 30);
  QVERIFY(f.answer(1, -1, QStringLiteral(":1.8"),
                   QStringLiteral("idle-epoch-b")));
  QVERIFY(f.model.available());
  QVERIFY(!f.model.enabled());
  QCOMPARE(f.model.minutes(), IdleDisplayPreferences::defaultTimeoutMinutes());
  QVERIFY(f.model.setEnabled(true));
  QCOMPARE(f.lastCommit().operations.constFirst().toMap()
               .value(QLatin1StringView(WireContract::FieldValue)).toLongLong(),
           IdleDisplayPreferences::defaultTimeoutMinutes());
}

void IdleDisplaySettingsModelTest::boundsAndDisabledDefault() {
  Fixture f;
  QVERIFY(f.ready(-1));
  QVERIFY(!f.model.enabled());
  QCOMPARE(f.model.minutes(), IdleDisplayPreferences::defaultTimeoutMinutes());
  QVERIFY(!f.model.setMinutes(0));
  QVERIFY(!f.model.setMinutes(241));
  QVERIFY(f.transport.commits.isEmpty());
  QVERIFY(f.model.setEnabled(true));
  QCOMPARE(f.lastCommit().operations.constFirst().toMap()
               .value(QLatin1StringView(WireContract::FieldValue)).toLongLong(),
           IdleDisplayPreferences::defaultTimeoutMinutes());
  f.reply(SettingsWireStatus::Applied, 1, 2,
          IdleDisplayPreferences::defaultTimeoutMinutes());
  QVERIFY(f.answer(2, IdleDisplayPreferences::defaultTimeoutMinutes()));
  QVERIFY(f.model.enabled());
  QVERIFY(f.model.setEnabled(false));
  QCOMPARE(f.lastCommit().operations.constFirst().toMap()
               .value(QLatin1StringView(WireContract::FieldValue)).toLongLong(), -1);
}

QTEST_MAIN(IdleDisplaySettingsModelTest)
#include "tst_idle_display_settings.moc"
