// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "restore/restore_state_store.h"
#include "restore/text_editor_restore_policy.h"
#include "ui/editor_window.h"
#include "ui/editor_application.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QFile>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class PolicyTransport final : public SettingsTransport {
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

QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                         const QVariant &value) {
  const QString key(TextEditorKeys::RestoreDocuments);
  return {
      {QLatin1StringView(WireContract::FieldStatus),
       quint32(SettingsWireStatus::Applied)},
      {QLatin1StringView(WireContract::FieldWireSchemaVersion),
       WireContract::WireSchemaVersion},
      {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
      {QLatin1StringView(WireContract::FieldEpoch), epoch},
      {QLatin1StringView(WireContract::FieldRevision), revision},
      {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
      {QLatin1StringView(WireContract::FieldSourceLayers),
       QVariantMap{{key, QStringLiteral("user-overrides")}}},
      {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       const QString &epoch, bool value,
                       const QString &message = {}) {
  const QString key(TextEditorKeys::RestoreDocuments);
  return {
      {QLatin1StringView(WireContract::FieldStatus), quint32(status)},
      {QLatin1StringView(WireContract::FieldWireSchemaVersion),
       WireContract::WireSchemaVersion},
      {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
      {QLatin1StringView(WireContract::FieldEpoch), epoch},
      {QLatin1StringView(WireContract::FieldRevisionBefore), before},
      {QLatin1StringView(WireContract::FieldRevisionAfter), after},
      {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
      {QLatin1StringView(WireContract::FieldSourceLayers),
       QVariantMap{{key, QStringLiteral("user-overrides")}}},
      {QLatin1StringView(WireContract::FieldChangedKeys),
       status == SettingsWireStatus::Applied ? QStringList{key}
                                             : QStringList{}},
      {QLatin1StringView(WireContract::FieldMessage), message}};
}

bool establish(PolicyTransport &transport, const QVariant &value,
               quint64 revision = 1) {
  emit transport.ownerChanged(QStringLiteral(":1.60"));
  if (!QTest::qWaitFor(
          [&transport] { return !transport.snapshots.isEmpty(); })) {
    return false;
  }
  const auto request = transport.snapshots.takeFirst();
  emit transport.snapshotReceived(
      request.token, request.owner,
      snapshotWire(QStringLiteral("epoch-a"), revision, value));
  return true;
}

DocumentStoreFactory localFactory() {
  return [] { return std::make_unique<LocalDocumentStore>(); };
}

} // namespace

class RestorePolicyTest final : public QObject {
  Q_OBJECT

private slots:
  void baselineAndLossFailClosed();
  void appliedWriteNeedsFreshConfirmation();
  void conflictRequiresExplicitRetry();
  void uncertainWriteIsNeverReplayed();
  void startupDropsUnavailablePathsWithoutContentState();
  void explicitPathsSuppressAlreadyEnabledRestore();
  void delayedInitialPolicyPreservesInventory();
  void windowInventoryTracksCloseAndPreferredActivePath();
};

void RestorePolicyTest::baselineAndLossFailClosed() {
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(),
                        {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  QVERIFY(client.start());
  QVERIFY(establish(transport, true));
  QTRY_VERIFY(policy.baselineReceived());
  QVERIFY(policy.enabled());
  emit transport.ownerChanged(QString{});
  QTRY_VERIFY(!policy.baselineReceived());
  QVERIFY(!policy.enabled());
}

void RestorePolicyTest::appliedWriteNeedsFreshConfirmation() {
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(),
                        {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  RestorePolicyResult result = RestorePolicyResult::Failed;
  int finished = 0;
  connect(&policy, &TextEditorRestorePolicy::applyFinished, this,
          [&result, &finished](RestorePolicyResult next, const QString &) {
            result = next;
            ++finished;
          });
  QVERIFY(client.start());
  QVERIFY(establish(transport, false, 3));
  QTRY_VERIFY(policy.baselineReceived());
  QVERIFY(policy.requestEnabled(true));
  QCOMPARE(transport.commits.size(), 1);
  const auto commit = transport.commits.first();
  emit transport.commitReceived(
      commit.token, commit.owner,
      commitWire(SettingsWireStatus::Applied, 3, 4, commit.epoch, true));
  QCOMPARE(finished, 0);
  QTRY_VERIFY(!transport.snapshots.isEmpty());
  const auto refresh = transport.snapshots.takeFirst();
  emit transport.snapshotReceived(refresh.token, refresh.owner,
                                  snapshotWire(commit.epoch, 4, true));
  QTRY_COMPARE(finished, 1);
  QCOMPARE(result, RestorePolicyResult::Applied);
  QVERIFY(policy.enabled());
}

void RestorePolicyTest::conflictRequiresExplicitRetry() {
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(),
                        {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  RestorePolicyResult result = RestorePolicyResult::Applied;
  connect(
      &policy, &TextEditorRestorePolicy::applyFinished, this,
      [&result](RestorePolicyResult next, const QString &) { result = next; });
  QVERIFY(client.start());
  QVERIFY(establish(transport, false));
  QTRY_VERIFY(policy.baselineReceived());
  QVERIFY(policy.requestEnabled(true));
  const auto commit = transport.commits.first();
  emit transport.commitReceived(
      commit.token, commit.owner,
      commitWire(SettingsWireStatus::Conflict, 2, 2, commit.epoch, false,
                 QStringLiteral("changed elsewhere")));
  QTRY_COMPARE(result, RestorePolicyResult::Conflict);
  QCOMPARE(transport.commits.size(), 1);
  QVERIFY(!policy.writePending());
}

void RestorePolicyTest::uncertainWriteIsNeverReplayed() {
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(),
                        {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  RestorePolicyResult result = RestorePolicyResult::Applied;
  connect(
      &policy, &TextEditorRestorePolicy::applyFinished, this,
      [&result](RestorePolicyResult next, const QString &) { result = next; });
  QVERIFY(client.start());
  QVERIFY(establish(transport, false));
  QTRY_VERIFY(policy.baselineReceived());
  QVERIFY(policy.requestEnabled(true));
  const auto commit = transport.commits.first();
  emit transport.requestFailed(commit.token, commit.owner,
                               QStringLiteral("org.test.Disconnected"),
                               QStringLiteral("transport lost"));
  QTRY_COMPARE(result, RestorePolicyResult::Uncertain);
  emit transport.ownerChanged(QStringLiteral(":1.61"));
  QTRY_VERIFY(!transport.snapshots.isEmpty());
  const auto refresh = transport.snapshots.takeFirst();
  emit transport.snapshotReceived(
      refresh.token, refresh.owner,
      snapshotWire(QStringLiteral("epoch-b"), 1, true));
  QTest::qWait(20);
  QCOMPARE(transport.commits.size(), 1);
}

void RestorePolicyTest::startupDropsUnavailablePathsWithoutContentState() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString valid = directory.filePath(QStringLiteral("valid.txt"));
  QFile file(valid);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("disk truth"), qint64(10));
  file.close();
  const QString missing = directory.filePath(QStringLiteral("missing.txt"));
  RestoreStateStore store(directory.filePath(QStringLiteral("state")));
  QVERIFY(store.store({.paths = {valid, missing}, .activeIndex = 0}).ok());

  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(),
                        {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  QVERIFY(client.start());
  QVERIFY(establish(transport, true));
  QTRY_VERIFY(policy.enabled());

  EditorApplication application(localFactory(), &policy, &store, {}, false);
  QVERIFY(application.start());
  auto &window = *application.windows().first();
  QCOMPARE(application.openPaths(), QStringList{valid});
  QCOMPARE(window.controller()->state().text(), QStringLiteral("disk truth"));
  QVERIFY(!window.controller()->state().isDirty());
  QVERIFY(window.statusBar()->currentMessage().contains(
      QStringLiteral("Skipped 1")));
  const RestoreLoadResult rewritten = store.load();
  QVERIFY(rewritten.ok());
  QCOMPARE(rewritten.state->paths, QStringList{valid});
}


void RestorePolicyTest::delayedInitialPolicyPreservesInventory() {
  QTemporaryDir root;
  const auto path = root.filePath("saved.txt");
  QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("saved"), qint64(5)); file.close();
  RestoreStateStore store(root.filePath("state"));
  QVERIFY(store.store({.paths={path}, .activeIndex=0}).ok());
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(), {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  QVERIFY(client.start());
  EditorApplication app(localFactory(), &policy, &store, {}, false);
  QVERIFY(app.start());
  QCOMPARE(store.load().state->paths, QStringList{path});
  QVERIFY(app.openPaths().isEmpty());
  QVERIFY(establish(transport, true));
  QTRY_COMPARE(app.openPaths(), QStringList{path});
  QCOMPARE(app.windows().size(), 1);
  emit transport.ownerChanged(QString{});
  QTRY_VERIFY(!policy.baselineReceived());
  QCOMPARE(store.load().error, RestoreStateError::Absent);
  QCOMPARE(app.openPaths(), QStringList{path});
}

void RestorePolicyTest::explicitPathsSuppressAlreadyEnabledRestore() {
  QTemporaryDir root;
  const auto saved = root.filePath("saved.txt"), explicitPath = root.filePath("explicit.txt");
  for (const auto &path : {saved, explicitPath}) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("disk"), qint64(4));
  }
  RestoreStateStore store(root.filePath("state"));
  QVERIFY(store.store({.paths={saved}, .activeIndex=0}).ok());
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(), {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  QVERIFY(client.start()); QVERIFY(establish(transport, true));
  QTRY_VERIFY(policy.enabled());
  EditorApplication application(localFactory(), &policy, &store, {}, false);
  QVERIFY(application.start({explicitPath}));
  QCOMPARE(application.openPaths(), QStringList{explicitPath});
  QCOMPARE(store.load().state->paths, QStringList{explicitPath});
}

void RestorePolicyTest::windowInventoryTracksCloseAndPreferredActivePath() {
  QTemporaryDir root;
  const auto first = root.filePath("first.txt"), second = root.filePath("second.txt");
  for (const auto &path : {first, second}) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("disk"), qint64(4));
  }
  RestoreStateStore store(root.filePath("state"));
  QVERIFY(store.store({.paths={first, second}, .activeIndex=0}).ok());
  PolicyTransport transport;
  SettingsClient client(transport, TextEditorKeys::scopedKeys(), {100, 0, {10}});
  TextEditorRestorePolicy policy(client);
  QVERIFY(client.start()); QVERIFY(establish(transport, true));
  QTRY_VERIFY(policy.enabled());
  EditorApplication application(localFactory(), &policy, &store, {}, false);
  QVERIFY(application.start());
  QCOMPARE(application.windows().size(), 2);
  QCOMPARE(store.load().state->activeIndex, 0);
  QVERIFY(application.windowForPath(first)->close());
  QCOMPARE(application.openPaths(), QStringList{second});
  QCOMPARE(store.load().state->paths, QStringList{second});
  QVERIFY(application.windowForPath(second)->close());
  QVERIFY(application.windows().isEmpty());
  QCOMPARE(store.load().state->paths, QStringList{second});
}

QTEST_MAIN(RestorePolicyTest)
#include "tst_restore_policy.moc"
