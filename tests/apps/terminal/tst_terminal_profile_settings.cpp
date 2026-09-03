// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/terminal_profile_settings.h"
#include "session/process_liveness.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_window.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QLabel>
#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::Terminal;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class ProfileTransport final : public SettingsTransport {
  Q_OBJECT
public:
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &keys) override {
    snapshots.append({token, owner, keys});
  }
  void commit(quint64 token, const QString &owner, const QString &epoch,
              quint64 revision, const QVariantList &operations) override {
    commits.append({token, owner, epoch, revision, operations});
  }
  void requestActivation() override {}

  struct SnapshotRequest {
    quint64 token;
    QString owner;
    QStringList keys;
  };
  struct CommitRequest {
    quint64 token;
    QString owner;
    QString epoch;
    quint64 revision;
    QVariantList operations;
  };
  QList<SnapshotRequest> snapshots;
  QList<CommitRequest> commits;
};

TerminalProfile userProfile() {
  TerminalProfile profile = builtinDefaultProfile();
  profile.id = QStringLiteral("work");
  profile.name = QStringLiteral("Work");
  profile.shellProgram = QStringLiteral("/bin/true");
  profile.fontSize = 13;
  return profile;
}

QVariantMap settingsValues(const QList<TerminalProfile> &profiles = {},
                           const QString &defaultId = builtinDefaultProfileId(),
                           bool restore = false) {
  bool ok = false;
  const QString encoded = encodeTerminalProfiles(profiles, &ok);
  Q_ASSERT(ok);
  return {{QString(TerminalKeys::Profiles), encoded},
          {QString(TerminalKeys::DefaultProfile), defaultId},
          {QString(TerminalKeys::RestoreTabs), restore}};
}

QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                         const QVariantMap &values) {
  QVariantMap sources;
  for (const QString &key : TerminalKeys::scopedKeys()) {
    sources.insert(key, QStringLiteral("user-overrides"));
  }
  return {
      {QLatin1StringView(WireContract::FieldStatus),
       quint32(SettingsWireStatus::Applied)},
      {QLatin1StringView(WireContract::FieldWireSchemaVersion),
       WireContract::WireSchemaVersion},
      {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
      {QLatin1StringView(WireContract::FieldEpoch), epoch},
      {QLatin1StringView(WireContract::FieldRevision), revision},
      {QLatin1StringView(WireContract::FieldValues), values},
      {QLatin1StringView(WireContract::FieldSourceLayers), sources},
      {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       const QString &epoch, const QString &key,
                       const QVariant &value, const QString &message = {}) {
  const QVariantMap values{{key, value}};
  const QVariantMap sources{{key, QStringLiteral("user-overrides")}};
  const QStringList changed =
      status == SettingsWireStatus::Applied ? QStringList{key} : QStringList{};
  return {
      {QLatin1StringView(WireContract::FieldStatus), quint32(status)},
      {QLatin1StringView(WireContract::FieldWireSchemaVersion),
       WireContract::WireSchemaVersion},
      {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
      {QLatin1StringView(WireContract::FieldEpoch), epoch},
      {QLatin1StringView(WireContract::FieldRevisionBefore), before},
      {QLatin1StringView(WireContract::FieldRevisionAfter), after},
      {QLatin1StringView(WireContract::FieldValues), values},
      {QLatin1StringView(WireContract::FieldSourceLayers), sources},
      {QLatin1StringView(WireContract::FieldChangedKeys), changed},
      {QLatin1StringView(WireContract::FieldMessage), message}};
}

bool establishBaseline(ProfileTransport &transport, const QString &owner,
                       const QString &epoch, quint64 revision,
                       const QVariantMap &values) {
  emit transport.ownerChanged(owner);
  if (!QTest::qWaitFor(
          [&transport] { return !transport.snapshots.isEmpty(); })) {
    return false;
  }
  const auto request = transport.snapshots.takeFirst();
  emit transport.snapshotReceived(request.token, request.owner,
                                  snapshotWire(epoch, revision, values));
  return true;
}

bool answerRefresh(ProfileTransport &transport, const QString &epoch,
                   quint64 revision, const QVariantMap &values) {
  if (!QTest::qWaitFor(
          [&transport] { return !transport.snapshots.isEmpty(); })) {
    return false;
  }
  const auto request = transport.snapshots.takeFirst();
  emit transport.snapshotReceived(request.token, request.owner,
                                  snapshotWire(epoch, revision, values));
  return true;
}

QString operationKey(const ProfileTransport::CommitRequest &request) {
  return request.operations.first()
      .toMap()
      .value(QLatin1StringView(WireContract::FieldKey))
      .toString();
}

QVariant operationValue(const ProfileTransport::CommitRequest &request) {
  return request.operations.first().toMap().value(
      QLatin1StringView(WireContract::FieldValue));
}

class NoProcessMonitor final : public ProcessMonitor {
public:
  ProcessExitInfo reap(ProcessId) override { return {}; }
  ProcessGroupState processGroupState(ProcessId) override {
    return ProcessGroupState::Unknown;
  }
  bool signalProcessGroup(ProcessId, int) override { return false; }
};

std::unique_ptr<TerminalWindow>
makePresentationWindow(TerminalProfileSettings &settings,
                       NoProcessMonitor &monitor) {
  TerminalSessionContext context{{}, QStringLiteral("/bin/true"), {}, {}};
  TerminalSession::BackendFactory noBackend = [](const TerminalProfile &) {
    return std::unique_ptr<TerminalSessionBackend>{};
  };
  auto sessions = std::make_unique<TerminalSessionCollection>(
      context, std::move(noBackend), &monitor, TeardownBounds{});
  TerminalViewAppearance appearance;
  appearance.statusWarningForeground = QColor(Qt::darkYellow);
  appearance.statusDangerForeground = QColor(Qt::red);
  return std::make_unique<TerminalWindow>(
      std::move(sessions), appearance,
      QStringList{QStringLiteral("qinda-dark")}, &settings);
}

} // namespace

class TerminalProfileSettingsTest final : public QObject {
  Q_OBJECT

private slots:
  void baselineRoundTripAndLossFailClosed();
  void applyCommitsEveryKeyAgainstFreshAuthority();
  void conflictAbortsWithoutReplayingLaterKeys();
  void uncertainCommitIsNeverReplayed();
  void productionWindowPresentsAsynchronousOutcome_data();
  void productionWindowPresentsAsynchronousOutcome();
};

void TerminalProfileSettingsTest::baselineRoundTripAndLossFailClosed() {
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {100, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVERIFY(establishBaseline(
      transport, QStringLiteral(":1.70"), QStringLiteral("epoch-a"), 4,
      settingsValues({userProfile()}, QStringLiteral("work"), true)));
  QTRY_VERIFY(settings.baselineReceived());
  QCOMPARE(settings.userProfiles(), QList<TerminalProfile>{userProfile()});
  QCOMPARE(settings.defaultProfile(), userProfile());
  QVERIFY(settings.restoreTabsPolicy());

  emit transport.ownerChanged(QString{});
  QTRY_VERIFY(!settings.baselineReceived());
  QVERIFY(settings.userProfiles().isEmpty());
  QCOMPARE(settings.defaultProfile(), builtinDefaultProfile());
  QVERIFY(!settings.restoreTabsPolicy());

  emit transport.ownerChanged(QStringLiteral(":1.701"));
  QTRY_VERIFY(!transport.snapshots.isEmpty());
  const auto request = transport.snapshots.takeFirst();
  QVariantMap malformed =
      settingsValues({userProfile()}, QStringLiteral("work"), true);
  malformed.insert(QString(TerminalKeys::Profiles), true);
  emit transport.snapshotReceived(
      request.token, request.owner,
      snapshotWire(QStringLiteral("epoch-b"), 1, malformed));
  QTest::qWait(20);
  QVERIFY(!settings.baselineReceived());
  QVERIFY(settings.userProfiles().isEmpty());
  QCOMPARE(settings.defaultProfile(), builtinDefaultProfile());
  QVERIFY(!settings.restoreTabsPolicy());
}

void TerminalProfileSettingsTest::applyCommitsEveryKeyAgainstFreshAuthority() {
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {100, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVariantMap authority = settingsValues();
  QVERIFY(establishBaseline(transport, QStringLiteral(":1.71"),
                            QStringLiteral("epoch-a"), 10, authority));
  QSignalSpy finished(&settings, &TerminalProfileSettings::applyFinished);
  QVERIFY(
      settings.applyProfiles({userProfile()}, QStringLiteral("work"), true));

  const QStringList expectedKeys = TerminalKeys::scopedKeys();
  for (int index = 0; index < expectedKeys.size(); ++index) {
    const quint64 before = 10U + static_cast<quint64>(index);
    const quint64 after = before + 1U;
    QTRY_COMPARE(transport.commits.size(), index + 1);
    const auto commit = transport.commits.at(index);
    QCOMPARE(operationKey(commit), expectedKeys.at(index));
    const QVariant intended = operationValue(commit);
    authority.insert(expectedKeys.at(index), intended);
    emit transport.commitReceived(commit.token, commit.owner,
                                  commitWire(SettingsWireStatus::Applied,
                                             before, after,
                                             QStringLiteral("epoch-a"),
                                             expectedKeys.at(index), intended));
    QVERIFY(
        answerRefresh(transport, QStringLiteral("epoch-a"), after, authority));
  }

  QTRY_COMPARE(finished.count(), 1);
  QCOMPARE(settings.userProfiles(), QList<TerminalProfile>{userProfile()});
  QCOMPARE(settings.defaultProfileId(), QStringLiteral("work"));
  QVERIFY(settings.restoreTabsPolicy());
  const QVariantList ledger = finished.first().first().toList();
  QCOMPARE(ledger.size(), 3);
  for (const QVariant &entry : ledger) {
    QCOMPARE(entry.toMap().value(QStringLiteral("result")).toString(),
             QStringLiteral("applied"));
  }
}

void TerminalProfileSettingsTest::conflictAbortsWithoutReplayingLaterKeys() {
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {100, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVERIFY(establishBaseline(transport, QStringLiteral(":1.72"),
                            QStringLiteral("epoch-a"), 5, settingsValues()));
  QSignalSpy finished(&settings, &TerminalProfileSettings::applyFinished);
  QVERIFY(
      settings.applyProfiles({userProfile()}, QStringLiteral("work"), true));
  QCOMPARE(transport.commits.size(), 1);
  const auto commit = transport.commits.first();
  emit transport.commitReceived(
      commit.token, commit.owner,
      commitWire(SettingsWireStatus::Conflict, 6, 6, QStringLiteral("epoch-a"),
                 operationKey(commit), QStringLiteral("[]"),
                 QStringLiteral("changed elsewhere")));
  QTRY_COMPARE(finished.count(), 1);
  QCOMPARE(transport.commits.size(), 1);
  const QVariantList ledger = finished.first().first().toList();
  QCOMPARE(ledger.at(0).toMap().value(QStringLiteral("result")).toString(),
           QStringLiteral("conflict"));
  QCOMPARE(ledger.at(1).toMap().value(QStringLiteral("result")).toString(),
           QStringLiteral("not-attempted"));
}

void TerminalProfileSettingsTest::uncertainCommitIsNeverReplayed() {
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {100, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVERIFY(establishBaseline(transport, QStringLiteral(":1.73"),
                            QStringLiteral("epoch-a"), 8, settingsValues()));
  QSignalSpy finished(&settings, &TerminalProfileSettings::applyFinished);
  QVERIFY(
      settings.applyProfiles({userProfile()}, QStringLiteral("work"), true));
  QCOMPARE(transport.commits.size(), 1);
  const auto commit = transport.commits.first();
  emit transport.requestFailed(commit.token, commit.owner,
                               QStringLiteral("org.test.Disconnected"),
                               QStringLiteral("transport lost"));
  QTRY_COMPARE(finished.count(), 1);
  QCOMPARE(finished.first()
               .first()
               .toList()
               .first()
               .toMap()
               .value(QStringLiteral("result"))
               .toString(),
           QStringLiteral("uncertain"));

  emit transport.ownerChanged(QStringLiteral(":1.74"));
  QVERIFY(answerRefresh(
      transport, QStringLiteral("epoch-b"), 1,
      settingsValues({userProfile()}, QStringLiteral("work"), true)));
  QTest::qWait(20);
  QCOMPARE(transport.commits.size(), 1);
}

void TerminalProfileSettingsTest::
    productionWindowPresentsAsynchronousOutcome_data() {
  QTest::addColumn<int>("outcome");
  QTest::addColumn<QString>("expectedSummary");
  QTest::newRow("conflict") << 0 << QStringLiteral("changed elsewhere");
  QTest::newRow("confirmed-rejection")
      << 1 << QStringLiteral("could not be saved");
  QTest::newRow("transport-failure")
      << 2 << QStringLiteral("outcome is uncertain");
  QTest::newRow("owner-loss") << 3 << QStringLiteral("outcome is uncertain");
}

void TerminalProfileSettingsTest::
    productionWindowPresentsAsynchronousOutcome() {
  QFETCH(int, outcome);
  QFETCH(QString, expectedSummary);
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {100, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVERIFY(establishBaseline(transport, QStringLiteral(":1.80"),
                            QStringLiteral("epoch-a"), 3, settingsValues()));
  NoProcessMonitor monitor;
  auto window = makePresentationWindow(settings, monitor);
  auto *status =
      window->findChild<QLabel *>(QStringLiteral("qindaqtTerminalStatus"));
  QVERIFY(status != nullptr);
  QVERIFY(
      settings.applyProfiles({userProfile()}, QStringLiteral("work"), true));
  QCOMPARE(transport.commits.size(), 1);
  const auto commit = transport.commits.first();

  if (outcome == 0) {
    emit transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus::Conflict, 4, 4,
                   QStringLiteral("epoch-a"), operationKey(commit),
                   QStringLiteral("[]"), QStringLiteral("changed elsewhere")));
  } else if (outcome == 1) {
    emit transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus::PersistenceFailed, 3, 3,
                   QStringLiteral("epoch-a"), operationKey(commit),
                   operationValue(commit), QStringLiteral("disk denied")));
  } else if (outcome == 2) {
    emit transport.requestFailed(commit.token, commit.owner,
                                 QStringLiteral("org.test.Disconnected"),
                                 QStringLiteral("transport lost"));
  } else {
    emit transport.ownerChanged(QString{});
  }

  QTRY_VERIFY(status->text().contains(expectedSummary, Qt::CaseInsensitive));
  QVERIFY(status->text().contains(QLatin1String("Profiles:")));
  QVERIFY(status->text().contains(QLatin1String("Default profile:")));
  QVERIFY(status->text().contains(QLatin1String("Restore tabs:")));
  QCOMPARE(status->accessibleName(),
           QStringLiteral("Session status: %1").arg(status->text()));

  if (outcome >= 2) {
    QVERIFY(status->text().contains(QLatin1String("not replayed"),
                                    Qt::CaseInsensitive));
    transport.snapshots.clear();
    emit transport.ownerChanged(QStringLiteral(":1.81"));
    QVERIFY(answerRefresh(transport, QStringLiteral("epoch-b"), 1,
                          settingsValues()));
    QTest::qWait(20);
    QCOMPARE(transport.commits.size(), 1);
  }
}

QTEST_MAIN(TerminalProfileSettingsTest)
#include "tst_terminal_profile_settings.moc"
