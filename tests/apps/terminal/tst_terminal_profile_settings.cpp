// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/terminal_profile_settings.h"
#include "session/process_liveness.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_profile_dialog.h"
#include "ui/terminal_window.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QAction>
#include <QApplication>
#include <QFontDatabase>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSignalSpy>
#include <QTimer>
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
          {QString(TerminalKeys::RestoreWindows), restore}};
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
  const TerminalViewAppearance appearance = TerminalAppearanceAdapter::derive(
      QPalette(), TerminalContentScheme::Dark, false,
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
  return std::make_unique<TerminalWindow>(std::move(sessions), appearance,
                                          &settings);
}

} // namespace

class TerminalProfileSettingsTest final : public QObject {
  Q_OBJECT

private slots:
  void baselineRoundTripAndLossFailClosed();
  void applyCommitsEveryKeyAgainstFreshAuthority();
  void conflictAbortsWithoutReplayingLaterKeys();
  void uncertainCommitIsNeverReplayed();
  void productionDialogPresentsAsynchronousOutcome_data();
  void productionDialogPresentsAsynchronousOutcome();
  void productionDialogClosesOnlyAfterAllApplied();
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
  QVERIFY(settings.restoreWindowsPolicy());

  emit transport.ownerChanged(QString{});
  QTRY_VERIFY(!settings.baselineReceived());
  QVERIFY(settings.userProfiles().isEmpty());
  QCOMPARE(settings.defaultProfile(), builtinDefaultProfile());
  QVERIFY(!settings.restoreWindowsPolicy());

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
  QVERIFY(!settings.restoreWindowsPolicy());
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
  QVERIFY(settings.restoreWindowsPolicy());
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
    productionDialogPresentsAsynchronousOutcome_data() {
  QTest::addColumn<int>("outcome");
  QTest::addColumn<QString>("expectedSummary");
  QTest::newRow("conflict") << 0 << QStringLiteral("changed elsewhere");
  QTest::newRow("confirmed-rejection")
      << 1 << QStringLiteral("could not be saved");
  QTest::newRow("transport-loss")
      << 2 << QStringLiteral("session bus disconnected");
  QTest::newRow("owner-loss") << 3 << QStringLiteral("outcome is uncertain");
  QTest::newRow("timeout-uncertain") << 4 << QStringLiteral("commit timed out");
}

void TerminalProfileSettingsTest::
    productionDialogPresentsAsynchronousOutcome() {
  QFETCH(int, outcome);
  QFETCH(QString, expectedSummary);
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {20, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVERIFY(establishBaseline(transport, QStringLiteral(":1.80"),
                            QStringLiteral("epoch-a"), 3, settingsValues()));
  NoProcessMonitor monitor;
  auto window = makePresentationWindow(settings, monitor);
  auto *action =
      window->findChild<QAction *>(QStringLiteral("profileManageAction"));
  QVERIFY(action != nullptr);
  QVERIFY(action->isEnabled());
  window->show();
  QTRY_VERIFY(window->isVisible());

  bool callbackRan = false;
  bool dialogWasLive = false;
  bool dialogRemainedLive = false;
  bool controlsReenabled = false;
  bool outcomePresentedAccessibly = false;
  QString outcomeText;
  QString accessibleText;
  QString callbackError;
  const auto inspectAndDismiss = [&](TerminalProfileDialog *dialog) {
    auto *status =
        dialog->findChild<QLabel *>(QStringLiteral("profileApplyStatus"));
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    dialogRemainedLive = dialog->isVisible() && dialog->isModal() &&
                         dialog->isEnabled() && dialog->result() == 0;
    controlsReenabled = buttons != nullptr && buttons->isEnabled();
    if (status != nullptr) {
      outcomeText = status->text();
      accessibleText = status->accessibleDescription();
      outcomePresentedAccessibly =
          status->isVisible() &&
          status->accessibleName() == QStringLiteral("Profile save status") &&
          accessibleText == outcomeText;
    }
    dialog->reject();
  };
  QTimer::singleShot(0, window.get(), [&] {
    callbackRan = true;
    auto *dialog = window->findChild<TerminalProfileDialog *>(
        QStringLiteral("terminalProfileDialog"));
    if (dialog == nullptr) {
      callbackError = QStringLiteral("production dialog was not created");
      if (QWidget *modal = QApplication::activeModalWidget()) {
        modal->close();
      }
      return;
    }
    dialogWasLive =
        dialog->isVisible() && dialog->isModal() && dialog->isEnabled();
    if (!QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection) ||
        transport.commits.size() != 1) {
      callbackError = QStringLiteral("dialog Apply did not start one commit");
      dialog->reject();
      return;
    }
    const auto commit = transport.commits.first();
    if (outcome == 0) {
      emit transport.commitReceived(
          commit.token, commit.owner,
          commitWire(SettingsWireStatus::Conflict, 4, 4,
                     QStringLiteral("epoch-a"), operationKey(commit),
                     QStringLiteral("authoritative-change"),
                     QStringLiteral("changed elsewhere")));
    } else if (outcome == 1) {
      emit transport.commitReceived(
          commit.token, commit.owner,
          commitWire(SettingsWireStatus::PersistenceFailed, 3, 3,
                     QStringLiteral("epoch-a"), operationKey(commit),
                     operationValue(commit), QStringLiteral("disk denied")));
    } else if (outcome == 2) {
      emit transport.busDisconnected();
    } else if (outcome == 3) {
      emit transport.ownerChanged(QString{});
    } else {
      QTimer::singleShot(40, dialog,
                         [&, dialog] { inspectAndDismiss(dialog); });
      return;
    }
    inspectAndDismiss(dialog);
  });
  action->trigger();

  QVERIFY2(callbackRan, qPrintable(callbackError));
  QVERIFY2(callbackError.isEmpty(), qPrintable(callbackError));
  QVERIFY(dialogWasLive);
  QVERIFY(dialogRemainedLive);
  QVERIFY(controlsReenabled);
  QVERIFY(outcomePresentedAccessibly);
  QVERIFY(outcomeText.contains(expectedSummary, Qt::CaseInsensitive));
  QVERIFY(outcomeText.contains(QLatin1String("Profiles:")));
  QVERIFY(outcomeText.contains(QLatin1String("Default profile:")));
  QVERIFY(outcomeText.contains(QLatin1String("Compatibility preference:")));
  QVERIFY(!outcomeText.contains(QLatin1String("Saving")));
  QCOMPARE(accessibleText, outcomeText);

  if (outcome >= 2) {
    QVERIFY(outcomeText.contains(QLatin1String("not replayed"),
                                 Qt::CaseInsensitive));
    QCOMPARE(transport.commits.size(), 1);
  }
}

void TerminalProfileSettingsTest::productionDialogClosesOnlyAfterAllApplied() {
  ProfileTransport transport;
  SettingsClient client(transport, TerminalKeys::scopedKeys(), {100, 0, {10}});
  TerminalProfileSettings settings(client);
  QVERIFY(client.start());
  QVariantMap authority = settingsValues();
  QVERIFY(establishBaseline(transport, QStringLiteral(":1.82"),
                            QStringLiteral("epoch-a"), 7, authority));
  NoProcessMonitor monitor;
  auto window = makePresentationWindow(settings, monitor);
  auto *action =
      window->findChild<QAction *>(QStringLiteral("profileManageAction"));
  QVERIFY(action != nullptr);

  bool callbackRan = false;
  bool closedAfterAllApplied = false;
  bool liveFailureAssertionWouldRejectControl = false;
  QString callbackError;
  QTimer::singleShot(0, window.get(), [&] {
    callbackRan = true;
    auto *dialog = window->findChild<TerminalProfileDialog *>(
        QStringLiteral("terminalProfileDialog"));
    if (dialog == nullptr || !dialog->isVisible() || !dialog->isModal() ||
        !QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection)) {
      callbackError = QStringLiteral("production dialog did not start Apply");
      if (dialog != nullptr) {
        dialog->reject();
      }
      return;
    }
    const QStringList keys = TerminalKeys::scopedKeys();
    for (int index = 0; index < keys.size(); ++index) {
      if (transport.commits.size() != index + 1) {
        callbackError = QStringLiteral("apply sequence did not advance");
        dialog->reject();
        return;
      }
      const auto commit = transport.commits.at(index);
      const QVariant intended = operationValue(commit);
      authority.insert(keys.at(index), intended);
      const quint64 before = 7U + static_cast<quint64>(index);
      emit transport.commitReceived(
          commit.token, commit.owner,
          commitWire(SettingsWireStatus::Applied, before, before + 1U,
                     QStringLiteral("epoch-a"), keys.at(index), intended));
      if (!answerRefresh(transport, QStringLiteral("epoch-a"), before + 1U,
                         authority)) {
        callbackError = QStringLiteral("post-commit refresh was not requested");
        dialog->reject();
        return;
      }
    }
    closedAfterAllApplied =
        !dialog->isVisible() && dialog->result() == QDialog::Accepted;
    liveFailureAssertionWouldRejectControl =
        !(dialog->isVisible() && dialog->isModal() && dialog->isEnabled() &&
          dialog->result() == 0);
  });
  action->trigger();

  QVERIFY2(callbackRan, qPrintable(callbackError));
  QVERIFY2(callbackError.isEmpty(), qPrintable(callbackError));
  QVERIFY(closedAfterAllApplied);
  QVERIFY(liveFailureAssertionWouldRejectControl);
  QVERIFY(window->findChild<TerminalProfileDialog *>(
              QStringLiteral("terminalProfileDialog")) == nullptr);
}

QTEST_MAIN(TerminalProfileSettingsTest)
#include "tst_terminal_profile_settings.moc"
