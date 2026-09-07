// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/mutation_controller.h"

#include <QMutex>
#include <QMutexLocker>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QVariantList>
#include <QVector>

using namespace QindaQt::Apps::FileManager;

namespace {

// Batch-capable fake: records every dispatched request in call order and can
// fail one chosen 1-based call or wait for cancellation (optionally returning
// success once the flag lands, so the controller's between-item cancellation
// check can be observed deterministically).
class RecordingBackend final : public MutationBackend {
public:
  explicit RecordingBackend(QThread *guiThread, bool waitForCancellation = false)
      : m_guiThread(guiThread), m_waitForCancellation(waitForCancellation) {}

  [[nodiscard]] MutationResult
  execute(const MutationRequest &request,
          const MutationCancellation &cancellation,
          const MutationProgressCallback &progress) override {
    called.store(true);
    offGuiThread.store(QThread::currentThread() != m_guiThread);
    lastKind.store(static_cast<int>(request.kind));
    const int callIndex = m_callCount.fetch_add(1) + 1;
    {
      QMutexLocker lock(&m_requestMutex);
      m_requests.append(request);
    }
    if (callIndex == failOnCall.load()) {
      MutationResult failure;
      failure.error = failError;
      failure.diagnostic = failDiagnostic;
      return failure;
    }
    if (progress) {
      progress({1, 2, QStringLiteral("Processed one item")});
    }
    while (m_waitForCancellation && !cancellation->load()) {
      QThread::msleep(1);
    }
    if (cancellation->load() && !succeedWhenCancelled) {
      MutationResult cancelled;
      cancelled.error = MutationError::Cancelled;
      cancelled.diagnostic = QStringLiteral("cancelled by test");
      return cancelled;
    }
    MutationResult result;
    result.outputPath = request.destinationPath;
    if (request.kind == MutationKind::Trash) {
      result.trashToken = QStringLiteral("test-token");
      result.originalPath = request.sourcePath;
      result.outputIdentity = FileIdentity{1, 2, 3, 4, 5};
      return result;
    }
    if (request.kind == MutationKind::Restore) {
      result.originalPath = request.destinationPath;
      return result;
    }
    if (request.kind == MutationKind::CreateFolder ||
        request.kind == MutationKind::Rename || request.kind == MutationKind::Move) {
      auto undo = std::make_shared<MutationRequest>();
      undo->kind = MutationKind::Trash;
      result.undoRequest = std::move(undo);
    }
    return result;
  }

  [[nodiscard]] int requestCount() const {
    QMutexLocker lock(&m_requestMutex);
    return static_cast<int>(m_requests.size());
  }

  [[nodiscard]] MutationRequest recordedRequest(int index) const {
    QMutexLocker lock(&m_requestMutex);
    return m_requests.at(index);
  }

  std::atomic_bool called = false;
  std::atomic_bool offGuiThread = false;
  std::atomic_int lastKind = -1;
  std::atomic_int failOnCall = 0;
  MutationError failError = MutationError::IoError;
  QString failDiagnostic;
  bool succeedWhenCancelled = false;

private:
  QThread *m_guiThread;
  bool m_waitForCancellation;
  std::atomic_int m_callCount = 0;
  mutable QMutex m_requestMutex;
  QVector<MutationRequest> m_requests;
};

// AGENT-CONTRACT: Batch items are the QML entry snapshot — a "path" string
// plus decimal-string identity fields. identityFromMap() rejects every other
// shape, and FileIdentity::valid() additionally requires nonzero device/inode.
[[nodiscard]] QVariantMap entrySnapshot(const QString &path, quint64 inode) {
  return {{QStringLiteral("path"), path},
          {QStringLiteral("device"), QStringLiteral("1")},
          {QStringLiteral("inode"), QString::number(inode)},
          {QStringLiteral("identitySize"), QStringLiteral("3")},
          {QStringLiteral("modifiedNanoseconds"), QStringLiteral("4")},
          {QStringLiteral("mode"), QStringLiteral("5")}};
}

} // namespace

class TestMutationController final : public QObject {
  Q_OBJECT

private slots:
  void executesOffGuiThreadAndPublishesProgressAndUndo();
  void cancellationCompletesWithTypedFailure();
  void restoreAndEmptyTrashClearRecoveryState();
  void hostileAndOverlongNamesFailBeforeBackendDispatch();
  void unchangedRenameIsNoOp();
  void batchTrashRunsSequentiallyAndClearsPendingUndo();
  void batchSuccessKeepsTheSingleItemRestoreToken();
  void batchCopyAndMoveComputePerItemDestinations();
  void batchStopsOnTheFirstTypedFailure();
  void staleSelectionsAreRejectedBeforeDispatch();
  void emptySelectionAndRelativeDestinationAreRejected();
  void busyControllerRejectsANewBatch();
  void cancellationBetweenItemsStopsTheBatch();
};

void TestMutationController::executesOffGuiThreadAndPublishesProgressAndUndo() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));
  QVERIFY(controller.createFolder(fixture.path(), QStringLiteral("created")));
  QVERIFY(controller.busy());
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QVERIFY(recording->called.load());
  QVERIFY(recording->offGuiThread.load());
  QCOMPARE(recording->lastKind.load(), static_cast<int>(MutationKind::CreateFolder));
  QCOMPARE(controller.progressValue(), 100);
  QVERIFY(controller.canUndo());
  QCOMPARE(controller.resultText(), QStringLiteral("File operation completed"));
  QCOMPARE(controller.failureCode(), QStringLiteral("none"));
}

void TestMutationController::cancellationCompletesWithTypedFailure() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread(), true);
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));
  QVERIFY(controller.createFolder(fixture.path(), QStringLiteral("created")));
  QTRY_VERIFY_WITH_TIMEOUT(recording->called.load(), 1000);
  controller.cancel();
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(controller.failureCode(), QStringLiteral("cancelled"));
  QVERIFY(controller.failureMessage().contains(QStringLiteral("cancelled")));
  QVERIFY(!controller.canUndo());
}

void TestMutationController::restoreAndEmptyTrashClearRecoveryState() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));
  const QVariantMap identity = {{QStringLiteral("device"), QStringLiteral("1")},
                                {QStringLiteral("inode"), QStringLiteral("2")},
                                {QStringLiteral("identitySize"), QStringLiteral("3")},
                                {QStringLiteral("modifiedNanoseconds"),
                                 QStringLiteral("4")},
                                {QStringLiteral("mode"), QStringLiteral("5")}};
  QVERIFY(controller.trashItem(QStringLiteral("/fixture/item"), identity));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QVERIFY(controller.canRestore());
  QVERIFY(controller.restoreLast());
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(recording->lastKind.load(), static_cast<int>(MutationKind::Restore));
  QVERIFY(!controller.canRestore());

  QVERIFY(controller.trashItem(QStringLiteral("/fixture/item"), identity));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QVERIFY(controller.canRestore());
  QVERIFY(controller.emptyTrash());
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(recording->lastKind.load(), static_cast<int>(MutationKind::EmptyTrash));
  QVERIFY(!controller.canRestore());
}

void TestMutationController::hostileAndOverlongNamesFailBeforeBackendDispatch() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));
  QVERIFY(!controller.createFolder(fixture.path(), QStringLiteral("../escape")));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
  QVERIFY(!controller.createFolder(fixture.path(), QString(256, QLatin1Char('x'))));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
  QVERIFY(!controller.createFolder(fixture.path(), QString(100, QChar(0x6587))));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
  QVERIFY(!controller.copyItem(fixture.filePath(QStringLiteral("source")),
                               QStringLiteral("relative-destination"), {}));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
  QVERIFY(!recording->called.load());
}

void TestMutationController::unchangedRenameIsNoOp() {
  // AGENT-NOTE: Regression for review P3-1. The rename dialog is initialized
  // to the current name, and accepting it must not dispatch a guaranteed
  // destination collision or publish a misleading failure card.
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  QVERIFY(controller.renameItem(QStringLiteral("/fixture/same"),
                                QStringLiteral("same"), {}));
  QVERIFY(!controller.busy());
  QVERIFY(!recording->called.load());
  QCOMPARE(controller.failureCode(), QStringLiteral("none"));
}

void TestMutationController::batchTrashRunsSequentiallyAndClearsPendingUndo() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  // Seed a pending one-level undo so the test proves a successful batch
  // replaces it rather than leaving a stale undo behind.
  QVERIFY(controller.createFolder(QStringLiteral("/fixture"), QStringLiteral("created")));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QVERIFY(controller.canUndo());

  QVERIFY(controller.trashItems(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 11),
       entrySnapshot(QStringLiteral("/fixture/b.txt"), 12)}));
  QVERIFY(controller.busy());
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);

  QCOMPARE(recording->requestCount(), 3);
  QCOMPARE(recording->recordedRequest(1).kind, MutationKind::Trash);
  QCOMPARE(recording->recordedRequest(1).sourcePath,
           QStringLiteral("/fixture/a.txt"));
  QCOMPARE(recording->recordedRequest(2).kind, MutationKind::Trash);
  QCOMPARE(recording->recordedRequest(2).sourcePath,
           QStringLiteral("/fixture/b.txt"));
  QCOMPARE(controller.failureCode(), QStringLiteral("none"));
  QCOMPARE(controller.resultText(), QStringLiteral("Finished 2 items"));
  // AGENT-CONTRACT: Batch results deliberately carry no undo request and no
  // Trash token, so undo stays one-level/single-item and restoreLast() keeps
  // referring to the most recent single-item trash only.
  QVERIFY(!controller.canUndo());
  QVERIFY(!controller.canRestore());
}

void TestMutationController::batchSuccessKeepsTheSingleItemRestoreToken() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  MutationController controller(std::move(backend));

  const QVariantMap identity = entrySnapshot(QStringLiteral("/fixture/solo.txt"), 21);
  QVERIFY(controller.trashItem(QStringLiteral("/fixture/solo.txt"), identity));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QVERIFY(controller.canRestore());

  QVERIFY(controller.trashItems(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 22),
       entrySnapshot(QStringLiteral("/fixture/b.txt"), 23)}));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(controller.resultText(), QStringLiteral("Finished 2 items"));
  // The batch's cleared trash token must not clobber the live restore token.
  QVERIFY(controller.canRestore());
}

void TestMutationController::batchCopyAndMoveComputePerItemDestinations() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  const QVariantList items = {entrySnapshot(QStringLiteral("/src/a.txt"), 31),
                              entrySnapshot(QStringLiteral("/src/b.txt"), 32)};
  QVERIFY(controller.copyItemsTo(items, QStringLiteral("/dest/dir")));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(recording->requestCount(), 2);
  for (int i = 0; i < 2; ++i) {
    QCOMPARE(recording->recordedRequest(i).kind, MutationKind::Copy);
    QCOMPARE(recording->recordedRequest(i).destinationPath,
             QStringLiteral("/dest/dir/%1").arg(i == 0 ? "a.txt" : "b.txt"));
    QVERIFY(recording->recordedRequest(i).declaredRoots.contains(
        QStringLiteral("/dest/dir")));
    QVERIFY(recording->recordedRequest(i).expectedSource.has_value());
  }
  QCOMPARE(controller.resultText(), QStringLiteral("Finished 2 items"));

  QVERIFY(controller.moveItemsTo(items, QStringLiteral("/dest/moved")));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(recording->requestCount(), 4);
  QCOMPARE(recording->recordedRequest(3).kind, MutationKind::Move);
  QCOMPARE(recording->recordedRequest(3).destinationPath,
           QStringLiteral("/dest/moved/b.txt"));
  QCOMPARE(controller.resultText(), QStringLiteral("Finished 2 items"));
  QVERIFY(!controller.canUndo());
}

void TestMutationController::batchStopsOnTheFirstTypedFailure() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  recording->failOnCall.store(2);
  recording->failError = MutationError::Vanished;
  recording->failDiagnostic = QStringLiteral("gone missing");
  MutationController controller(std::move(backend));

  QVERIFY(controller.trashItems(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 41),
       entrySnapshot(QStringLiteral("/fixture/b.txt"), 42),
       entrySnapshot(QStringLiteral("/fixture/c.txt"), 43)}));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);

  // The failing item's backend call happened; the third item was never sent.
  QCOMPARE(recording->requestCount(), 2);
  QCOMPARE(controller.failureCode(), QStringLiteral("vanished"));
  QCOMPARE(controller.failureMessage(),
           QStringLiteral("Completed 1 of 3 items; b.txt: gone missing"));
  QVERIFY(controller.resultText().isEmpty());
}

void TestMutationController::staleSelectionsAreRejectedBeforeDispatch() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  const auto expectStale = [&controller](const QVariantList &items) {
    QVERIFY(!controller.trashItems(items));
    QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
    QCOMPARE(controller.failureMessage(),
             QStringLiteral("The selection is stale; refresh and try again"));
  };
  // A non-map variant.
  expectStale({QVariant(QStringLiteral("not-a-map"))});
  // A map without identity fields.
  expectStale({QVariantMap{{QStringLiteral("path"), QStringLiteral("/fixture/a")}}});
  // An unparsable decimal-string identity field.
  QVariantMap badNumber = entrySnapshot(QStringLiteral("/fixture/a"), 51);
  badNumber.insert(QStringLiteral("inode"), QStringLiteral("not-a-number"));
  expectStale({badNumber});
  // A zero device/inode fails FileIdentity::valid().
  QVariantMap zeroIdentity = entrySnapshot(QStringLiteral("/fixture/a"), 0);
  expectStale({zeroIdentity});
  // An empty path.
  expectStale({entrySnapshot(QString(), 52)});

  QVERIFY(!recording->called.load());
}

void TestMutationController::emptySelectionAndRelativeDestinationAreRejected() {
  auto backend = std::make_unique<RecordingBackend>(QThread::currentThread());
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  QVERIFY(!controller.trashItems({}));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
  QCOMPARE(controller.failureMessage(), QStringLiteral("No items are selected"));

  const QVariantList items = {entrySnapshot(QStringLiteral("/fixture/a.txt"), 61)};
  QVERIFY(!controller.copyItemsTo(items, QStringLiteral("relative/dir")));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));
  QCOMPARE(controller.failureMessage(),
           QStringLiteral("Choose an absolute local destination path"));
  QVERIFY(!controller.moveItemsTo(items, QString()));
  QCOMPARE(controller.failureCode(), QStringLiteral("invalid-request"));

  QVERIFY(!recording->called.load());
}

void TestMutationController::busyControllerRejectsANewBatch() {
  auto backend =
      std::make_unique<RecordingBackend>(QThread::currentThread(), true);
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  QVERIFY(controller.trashItems(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 71),
       entrySnapshot(QStringLiteral("/fixture/b.txt"), 72)}));
  QTRY_VERIFY_WITH_TIMEOUT(recording->called.load(), 1000);
  QVERIFY(controller.busy());

  QVERIFY(!controller.trashItems(
      {entrySnapshot(QStringLiteral("/fixture/c.txt"), 73)}));
  QCOMPARE(controller.failureCode(), QStringLiteral("busy"));

  controller.cancel();
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
  QCOMPARE(controller.failureCode(), QStringLiteral("cancelled"));
  QCOMPARE(recording->requestCount(), 1);
}

void TestMutationController::cancellationBetweenItemsStopsTheBatch() {
  auto backend =
      std::make_unique<RecordingBackend>(QThread::currentThread(), true);
  backend->succeedWhenCancelled = true;
  RecordingBackend *recording = backend.get();
  MutationController controller(std::move(backend));

  QVERIFY(controller.trashItems(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 81),
       entrySnapshot(QStringLiteral("/fixture/b.txt"), 82),
       entrySnapshot(QStringLiteral("/fixture/c.txt"), 83)}));
  QTRY_VERIFY_WITH_TIMEOUT(recording->called.load(), 1000);
  controller.cancel();
  QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);

  // Item one completed; the between-item cancellation check stopped the rest.
  QCOMPARE(recording->requestCount(), 1);
  QCOMPARE(controller.failureCode(), QStringLiteral("cancelled"));
  QCOMPARE(controller.failureMessage(),
           QStringLiteral("Cancelled after 1 of 3 items"));
}

QTEST_GUILESS_MAIN(TestMutationController)
#include "tst_mutation_controller.moc"
