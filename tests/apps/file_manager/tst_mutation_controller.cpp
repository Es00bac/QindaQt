// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/mutation_controller.h"

#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace QindaQt::Apps::FileManager;

namespace {

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
    if (progress) {
      progress({1, 2, QStringLiteral("Processed one item")});
    }
    while (m_waitForCancellation && !cancellation->load()) {
      QThread::msleep(1);
    }
    if (cancellation->load()) {
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

  std::atomic_bool called = false;
  std::atomic_bool offGuiThread = false;
  std::atomic_int lastKind = -1;

private:
  QThread *m_guiThread;
  bool m_waitForCancellation;
};

} // namespace

class TestMutationController final : public QObject {
  Q_OBJECT

private slots:
  void executesOffGuiThreadAndPublishesProgressAndUndo();
  void cancellationCompletesWithTypedFailure();
  void restoreAndEmptyTrashClearRecoveryState();
  void hostileAndOverlongNamesFailBeforeBackendDispatch();
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
  const QVariantMap identity = {{QStringLiteral("device"), 1},
                                {QStringLiteral("inode"), 2},
                                {QStringLiteral("identitySize"), 3},
                                {QStringLiteral("modifiedNanoseconds"), 4},
                                {QStringLiteral("mode"), 5}};
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

QTEST_GUILESS_MAIN(TestMutationController)
#include "tst_mutation_controller.moc"
