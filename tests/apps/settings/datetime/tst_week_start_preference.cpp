// SPDX-License-Identifier: GPL-3.0-or-later
#include "week_start_preference.h"

#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::SettingsDateTime;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;

namespace {
constexpr auto key = "services.calendarWeekStart";

class FakeTransport final : public SettingsTransport {
public:
  bool start(QString *error) override {
    if (error) error->clear();
    return true;
  }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &keys) override {
    snapshots.append({token, owner, keys});
  }
  void commit(quint64 token, const QString &owner, const QString &epoch,
              quint64 revision, const QVariantList &operations) override {
    commits.append({token, owner, epoch, revision, operations});
  }
  void requestActivation() override { ++activations; }

  struct SnapshotRequest { quint64 token; QString owner; QStringList keys; };
  struct CommitRequest {
    quint64 token; QString owner; QString epoch; quint64 revision;
    QVariantList operations;
  };
  QList<SnapshotRequest> snapshots;
  QList<CommitRequest> commits;
  int activations = 0;
};

QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                         const QString &value) {
  return {{QLatin1StringView(WireContract::FieldStatus),
           quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion),
           WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), epoch},
          {QLatin1StringView(WireContract::FieldRevision), revision},
          {QLatin1StringView(WireContract::FieldValues),
           QVariantMap{{QLatin1StringView(key), value}}},
          {QLatin1StringView(WireContract::FieldSourceLayers),
           QVariantMap{{QLatin1StringView(key), QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before,
                       quint64 after, const QString &value,
                       const QString &message = {}) {
  const QStringList changed = status == SettingsWireStatus::Applied &&
                                      after == before + 1
                                  ? QStringList{QLatin1StringView(key)} : QStringList{};
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion),
           WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
          {QLatin1StringView(WireContract::FieldRevisionBefore), before},
          {QLatin1StringView(WireContract::FieldRevisionAfter), after},
          {QLatin1StringView(WireContract::FieldValues),
           QVariantMap{{QLatin1StringView(key), value}}},
          {QLatin1StringView(WireContract::FieldSourceLayers),
           QVariantMap{{QLatin1StringView(key), QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WireContract::FieldChangedKeys), changed},
          {QLatin1StringView(WireContract::FieldMessage), message}};
}

struct Fixture {
  FakeTransport *transport;
  std::unique_ptr<SettingsWeekStartPreference> preference;
  explicit Fixture(int timeout = 70) {
    auto owned = std::make_unique<FakeTransport>();
    transport = owned.get();
    preference = std::make_unique<SettingsWeekStartPreference>(
        std::move(owned), ClientTiming{timeout, 0, {10}});
  }
  void ready(const QString &owner = QStringLiteral(":1.1"),
             quint64 revision = 1,
             const QString &value = QStringLiteral("locale")) {
    Q_EMIT transport->ownerChanged(owner);
    QTRY_COMPARE(transport->snapshots.size(), 1);
    const auto request = transport->snapshots.takeFirst();
    QCOMPARE(request.keys, QStringList{QLatin1StringView(key)});
    Q_EMIT transport->snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch"),
                                                   revision, value));
  }
  FakeTransport::CommitRequest takeCommit() {
    Q_ASSERT(transport->commits.size() == 1);
    return transport->commits.takeFirst();
  }
  void answer(const FakeTransport::CommitRequest &request,
              SettingsWireStatus status, quint64 before, quint64 after,
              const QString &value, const QString &message = {}) {
    Q_EMIT transport->commitReceived(request.token, request.owner,
                                     commitWire(status, before, after, value,
                                                message));
  }
  void readback(quint64 revision, const QString &value) {
    QTRY_COMPARE(transport->snapshots.size(), 1);
    const auto request = transport->snapshots.takeFirst();
    Q_EMIT transport->snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch"),
                                                   revision, value));
  }
};
} // namespace

class WeekStartPreferenceTest final : public QObject {
  Q_OBJECT
private slots:
  void initialAbsenceAndRequestAdmission();
  void waitsForFreshConfirmedValue();
  void refusalConflictAndExternalChange();
  void lostReplyAndOwnerReplacementNeverReplay();
  void ownerReplacementDuringWriteIsUncertain();
  void ownerReplacementBeforeAppliedReadbackIsUncertain();
  void staleReadbackTimesOutWithoutSuccess();
};

void WeekStartPreferenceTest::initialAbsenceAndRequestAdmission() {
  Fixture f;
  QVERIFY(!f.preference->editable());
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  QVERIFY(!f.preference->availabilityText().isEmpty());
  f.ready();
  QVERIFY(f.preference->editable());
  QCOMPARE(f.preference->availabilityText(), QString{});
  Q_EMIT f.transport->settingsChanged(QStringLiteral(":1.1"),
                                      QStringLiteral("epoch"), 2,
                                      QStringList{QLatin1StringView(key)});
  QTRY_COMPARE(f.transport->snapshots.size(), 1);
  QVERIFY(!f.preference->editable());
  QVERIFY(!f.preference->setWeekStart(QStringLiteral("monday")));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Refused);
  QVERIFY(!f.preference->diagnostic().isEmpty());
  f.readback(2, QStringLiteral("locale"));
  QVERIFY(f.preference->editable());
  QCOMPARE(f.transport->commits.size(), 0);
}

void WeekStartPreferenceTest::waitsForFreshConfirmedValue() {
  Fixture f;
  f.ready();
  QVERIFY(f.preference->setWeekStart(QStringLiteral("monday")));
  QVERIFY(!f.preference->editable());
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Pending);
  const auto request = f.takeCommit();
  QCOMPARE(request.revision, quint64(1));
  QCOMPARE(request.operations.size(), 1);
  f.answer(request, SettingsWireStatus::Applied, 1, 2,
           QStringLiteral("monday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Pending);
  f.readback(1, QStringLiteral("locale"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Pending);
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  f.preference->refresh();
  f.readback(2, QStringLiteral("monday"));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("monday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Idle);
  QVERIFY(f.preference->diagnostic().isEmpty());
  QVERIFY(f.preference->editable());
}

void WeekStartPreferenceTest::refusalConflictAndExternalChange() {
  Fixture f;
  f.ready();
  QVERIFY(f.preference->setWeekStart(QStringLiteral("sunday")));
  auto request = f.takeCommit();
  f.answer(request, SettingsWireStatus::PersistenceFailed, 1, 1,
           QStringLiteral("locale"), QStringLiteral("disk full"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Refused);
  QVERIFY(f.preference->diagnostic().contains(QStringLiteral("disk full")));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  f.readback(1, QStringLiteral("locale"));
  QVERIFY(f.preference->diagnostic().contains(QStringLiteral("disk full")));
  QVERIFY(f.preference->setWeekStart(QStringLiteral("monday")));
  request = f.takeCommit();
  f.answer(request, SettingsWireStatus::Conflict, 2, 2,
           QStringLiteral("sunday"), QStringLiteral("revision conflict"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Conflict);
  QVERIFY(f.preference->diagnostic().contains(QStringLiteral("revision conflict")));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  f.readback(2, QStringLiteral("sunday"));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("sunday"));
  QVERIFY(f.preference->editable());
  QCOMPARE(f.transport->commits.size(), 0);
}

void WeekStartPreferenceTest::lostReplyAndOwnerReplacementNeverReplay() {
  Fixture f(40);
  f.ready();
  QVERIFY(f.preference->setWeekStart(QStringLiteral("sunday")));
  const auto request = f.takeCommit();
  QTRY_COMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QVERIFY(f.preference->diagnostic().contains(QStringLiteral("may have changed")));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  QCOMPARE(f.transport->commits.size(), 0);
  Q_EMIT f.transport->commitReceived(request.token, request.owner,
                                     commitWire(SettingsWireStatus::Applied, 1, 2,
                                                QStringLiteral("sunday")));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  f.transport->snapshots.clear(); // discard any old-owner recovery request
  Q_EMIT f.transport->ownerChanged(QStringLiteral(":1.2"));
  QVERIFY(!f.preference->editable());
  QTRY_COMPARE(f.transport->snapshots.size(), 1);
  const auto read = f.transport->snapshots.takeFirst();
  Q_EMIT f.transport->snapshotReceived(read.token, read.owner,
                                      snapshotWire(QStringLiteral("new-epoch"), 0,
                                                   QStringLiteral("sunday")));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("sunday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QCOMPARE(f.transport->commits.size(), 0);
}

void WeekStartPreferenceTest::ownerReplacementDuringWriteIsUncertain() {
  Fixture f;
  f.ready();
  QVERIFY(f.preference->setWeekStart(QStringLiteral("monday")));
  const auto request = f.takeCommit();
  Q_EMIT f.transport->ownerChanged(QStringLiteral(":1.2"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QVERIFY(!f.preference->editable());
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  Q_EMIT f.transport->commitReceived(request.token, request.owner,
                                     commitWire(SettingsWireStatus::Applied, 1, 2,
                                                QStringLiteral("monday")));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QCOMPARE(f.transport->commits.size(), 0);
  QTRY_COMPARE(f.transport->snapshots.size(), 1);
  const auto read = f.transport->snapshots.takeFirst();
  Q_EMIT f.transport->snapshotReceived(read.token, read.owner,
                                      snapshotWire(QStringLiteral("new-epoch"), 0,
                                                   QStringLiteral("monday")));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("monday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QVERIFY(f.preference->editable());
}

void WeekStartPreferenceTest::ownerReplacementBeforeAppliedReadbackIsUncertain() {
  Fixture f;
  f.ready();
  QVERIFY(f.preference->setWeekStart(QStringLiteral("monday")));
  const auto request = f.takeCommit();
  f.answer(request, SettingsWireStatus::Applied, 1, 2,
           QStringLiteral("monday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Pending);
  // SettingsClient is already Authenticating after Applied. Replacing the
  // exact owner keeps that same state, so stateChanged alone cannot notify us.
  Q_EMIT f.transport->ownerChanged(QStringLiteral(":1.2"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  QCOMPARE(f.transport->commits.size(), 0);
  QTRY_COMPARE(f.transport->snapshots.size(), 1);
  const auto read = f.transport->snapshots.takeFirst();
  Q_EMIT f.transport->snapshotReceived(read.token, read.owner,
                                      snapshotWire(QStringLiteral("new-epoch"), 0,
                                                   QStringLiteral("monday")));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("monday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QCOMPARE(f.transport->commits.size(), 0);
}

void WeekStartPreferenceTest::staleReadbackTimesOutWithoutSuccess() {
  Fixture f(40);
  f.ready();
  QVERIFY(f.preference->setWeekStart(QStringLiteral("monday")));
  const auto request = f.takeCommit();
  f.answer(request, SettingsWireStatus::Applied, 1, 2,
           QStringLiteral("monday"));
  f.readback(1, QStringLiteral("locale"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Pending);
  QTRY_COMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
  QCOMPARE(f.preference->weekStart(), QStringLiteral("locale"));
  QCOMPARE(f.transport->commits.size(), 0);
  f.preference->refresh();
  f.readback(2, QStringLiteral("monday"));
  QCOMPARE(f.preference->weekStart(), QStringLiteral("monday"));
  QCOMPARE(f.preference->writeState(), WeekStartWriteState::Uncertain);
}

QTEST_MAIN(WeekStartPreferenceTest)
#include "tst_week_start_preference.moc"
