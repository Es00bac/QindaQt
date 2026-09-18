// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/transfer_queue_controller.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;

namespace {

struct StartedTransfer final {
  quint64 id = 0;
  QUrl source;
  QUrl destinationFolder;
  TransferOperation operation = TransferOperation::Copy;
};

// Records what the queue dispatched and answers only when the test says so,
// so sequencing, fencing, and pause/cancel are observable without KIO, a
// network, or a filesystem.
class RecordingWorker final : public TransferWorker {
  Q_OBJECT

public:
  void start(quint64 id, const QUrl &source, const QUrl &destinationFolder,
             TransferOperation operation) override {
    m_started.append({id, source, destinationFolder, operation});
  }
  void pause(quint64 id) override { m_paused.append(id); }
  void resume(quint64 id) override { m_resumed.append(id); }
  void cancel(quint64 id) override { m_cancelled.append(id); }

  void finish(quint64 id, const QString &diagnostic = {}) {
    Q_EMIT finished(id, diagnostic);
  }
  void report(quint64 id, int percent) { Q_EMIT progressChanged(id, percent); }

  [[nodiscard]] const QVector<StartedTransfer> &started() const { return m_started; }
  [[nodiscard]] const QVector<quint64> &paused() const { return m_paused; }
  [[nodiscard]] const QVector<quint64> &resumed() const { return m_resumed; }
  [[nodiscard]] const QVector<quint64> &cancelled() const { return m_cancelled; }

private:
  QVector<StartedTransfer> m_started;
  QVector<quint64> m_paused;
  QVector<quint64> m_resumed;
  QVector<quint64> m_cancelled;
};

struct Fixture final {
  RecordingWorker *worker = nullptr;
  std::unique_ptr<TransferQueueController> queue;

  Fixture() {
    auto owned = std::make_unique<RecordingWorker>();
    worker = owned.get();
    queue = std::make_unique<TransferQueueController>(std::move(owned));
  }
};

} // namespace

class TestTransferQueue final : public QObject {
  Q_OBJECT

private slots:
  void runsOneItemAtATimeInOrder();
  void namesTheRouteWithoutQueueingAnything();
  void refusesWhatTheRouterGaveToAnotherOwner();
  void publishesProgressAndCommitsOnlyOnSuccess();
  void aFailedItemRetiresAndTheQueueContinues();
  void pauseAndResumeTrackTheActiveItem();
  void cancelRetiresAndFencesALateResult();
  void cancelAllRetiresEveryLiveItem();
  void clearFinishedKeepsLiveItems();
};

void TestTransferQueue::runsOneItemAtATimeInOrder() {
  Fixture fixture;
  QVERIFY(fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt"),
                                  QStringLiteral("/home/cabewse/b.txt")},
                                 QStringLiteral("sftp://qinda/mnt/storage"),
                                 QStringLiteral("copy")));
  QCOMPARE(fixture.queue->itemValues().size(), 2);
  // Exactly one job is live: a saturated link makes concurrent transfers
  // slower, and one job keeps the platform's prompts sequential.
  QCOMPARE(fixture.worker->started().size(), 1);
  QCOMPARE(fixture.worker->started().constFirst().source.toString(),
           QStringLiteral("file:///home/cabewse/a.txt"));
  QCOMPARE(fixture.worker->started().constFirst().destinationFolder.toString(),
           QStringLiteral("sftp://qinda/mnt/storage"));
  QCOMPARE(fixture.worker->started().constFirst().operation, TransferOperation::Copy);
  QCOMPARE(fixture.queue->itemValues().at(0).state, TransferState::Running);
  QCOMPARE(fixture.queue->itemValues().at(1).state, TransferState::Queued);
  QCOMPARE(fixture.queue->queuedCount(), 1);
  QCOMPARE(fixture.queue->activeDescription(),
           QStringLiteral("Copying a.txt to sftp://qinda/mnt/storage"));

  fixture.worker->finish(fixture.worker->started().constFirst().id);
  QCOMPARE(fixture.worker->started().size(), 2);
  QCOMPARE(fixture.queue->itemValues().at(0).state, TransferState::Succeeded);
  QCOMPARE(fixture.queue->itemValues().at(1).state, TransferState::Running);

  fixture.worker->finish(fixture.worker->started().at(1).id);
  QVERIFY(!fixture.queue->busy());
  QCOMPARE(fixture.queue->activeDescription(), QString());
}

void TestTransferQueue::namesTheRouteWithoutQueueingAnything() {
  Fixture fixture;
  QCOMPARE(fixture.queue->routeName({QStringLiteral("/home/cabewse/a.txt")},
                                    QStringLiteral("/home/cabewse/Downloads")),
           QStringLiteral("local"));
  QCOMPARE(fixture.queue->routeName({QStringLiteral("sftp://qinda/mnt/a.txt")},
                                    QStringLiteral("sftp://qinda/mnt/backup")),
           QStringLiteral("remote-child"));
  QCOMPARE(fixture.queue->routeName({QStringLiteral("/home/cabewse/a.txt")},
                                    QStringLiteral("sftp://qinda/mnt")),
           QStringLiteral("queue"));
  QCOMPARE(fixture.queue->routeName({QStringLiteral("/home/cabewse/a.txt")},
                                    QStringLiteral("nonsense")),
           QStringLiteral("refuse"));
  QVERIFY(fixture.queue->itemValues().isEmpty());
  QVERIFY(fixture.worker->started().isEmpty());
  QVERIFY(fixture.queue->refusal().isEmpty());
}

void TestTransferQueue::refusesWhatTheRouterGaveToAnotherOwner() {
  Fixture fixture;
  // AGENT-GUARD: exactly one owner per request. A local or one-child remote
  // request must not be quietly re-homed into the queue.
  QVERIFY(!fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt")},
                                  QStringLiteral("/home/cabewse/Downloads"),
                                  QStringLiteral("copy")));
  QVERIFY(!fixture.queue->refusal().isEmpty());
  QVERIFY(!fixture.queue->enqueue({QStringLiteral("sftp://qinda/mnt/a.txt")},
                                  QStringLiteral("sftp://qinda/mnt/backup"),
                                  QStringLiteral("move")));
  QVERIFY(!fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt")},
                                  QStringLiteral("sftp://qinda/mnt"),
                                  QStringLiteral("trash")));
  QVERIFY(fixture.worker->started().isEmpty());
  QVERIFY(fixture.queue->itemValues().isEmpty());

  // A router refusal is reported verbatim so the banner states the real cause.
  QVERIFY(!fixture.queue->enqueue({QStringLiteral("sftp://qinda/mnt/storage")},
                                  QStringLiteral("sftp://qinda/mnt/storage/inner"),
                                  QStringLiteral("copy")));
  QVERIFY(fixture.queue->refusal().contains(QStringLiteral("into itself")));
  fixture.queue->clearRefusal();
  QVERIFY(fixture.queue->refusal().isEmpty());
}

void TestTransferQueue::publishesProgressAndCommitsOnlyOnSuccess() {
  Fixture fixture;
  QSignalSpy committed(fixture.queue.get(),
                       &TransferQueueController::transferCommitted);
  QVERIFY(fixture.queue->enqueue({QStringLiteral("sftp://qinda/mnt/a.txt")},
                                 QStringLiteral("/home/cabewse/Downloads"),
                                 QStringLiteral("move")));
  const quint64 id = fixture.worker->started().constFirst().id;
  QCOMPARE(fixture.worker->started().constFirst().operation, TransferOperation::Move);
  QCOMPARE(fixture.queue->activeDescription(),
           QStringLiteral("Moving a.txt to /home/cabewse/Downloads"));

  fixture.worker->report(id, 42);
  QCOMPARE(fixture.queue->activePercent(), 42);
  // Out-of-range progress is bounded rather than trusted.
  fixture.worker->report(id, 900);
  QCOMPARE(fixture.queue->activePercent(), 100);
  fixture.worker->report(id, -5);
  QCOMPARE(fixture.queue->activePercent(), 0);

  fixture.worker->finish(id);
  QCOMPARE(committed.count(), 1);
  QCOMPARE(committed.constFirst().constFirst().toUrl().toString(),
           QStringLiteral("file:///home/cabewse/Downloads"));
  QCOMPARE(fixture.queue->itemValues().constFirst().percent, 100);
}

void TestTransferQueue::aFailedItemRetiresAndTheQueueContinues() {
  Fixture fixture;
  QSignalSpy committed(fixture.queue.get(),
                       &TransferQueueController::transferCommitted);
  QVERIFY(fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt"),
                                  QStringLiteral("/home/cabewse/b.txt")},
                                 QStringLiteral("sftp://qinda/mnt"),
                                 QStringLiteral("copy")));
  fixture.worker->finish(fixture.worker->started().constFirst().id,
                         QStringLiteral("Connection refused"));
  QCOMPARE(fixture.queue->itemValues().at(0).state, TransferState::Failed);
  QCOMPARE(fixture.queue->itemValues().at(0).diagnostic,
           QStringLiteral("Connection refused"));
  QCOMPARE(fixture.queue->failedCount(), 1);
  QCOMPARE(committed.count(), 0);
  // One failure retires only its own item; the rest of the selection runs.
  QCOMPARE(fixture.worker->started().size(), 2);
  QCOMPARE(fixture.queue->itemValues().at(1).state, TransferState::Running);
}

void TestTransferQueue::pauseAndResumeTrackTheActiveItem() {
  Fixture fixture;
  QVERIFY(fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt")},
                                 QStringLiteral("sftp://qinda/mnt"),
                                 QStringLiteral("copy")));
  const quint64 id = fixture.worker->started().constFirst().id;
  QVERIFY(!fixture.queue->activePaused());

  fixture.queue->pauseActive();
  QVERIFY(fixture.queue->activePaused());
  QCOMPARE(fixture.worker->paused(), QVector<quint64>{id});
  QVERIFY(fixture.queue->busy());
  // A paused item is still the active one; pausing twice changes nothing.
  fixture.queue->pauseActive();
  QCOMPARE(fixture.worker->paused().size(), 1);

  fixture.queue->resumeActive();
  QVERIFY(!fixture.queue->activePaused());
  QCOMPARE(fixture.worker->resumed(), QVector<quint64>{id});
  fixture.queue->resumeActive();
  QCOMPARE(fixture.worker->resumed().size(), 1);
}

void TestTransferQueue::cancelRetiresAndFencesALateResult() {
  Fixture fixture;
  QSignalSpy committed(fixture.queue.get(),
                       &TransferQueueController::transferCommitted);
  QVERIFY(fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt"),
                                  QStringLiteral("/home/cabewse/b.txt")},
                                 QStringLiteral("sftp://qinda/mnt"),
                                 QStringLiteral("copy")));
  const quint64 first = fixture.worker->started().constFirst().id;
  fixture.queue->cancel(first);
  QCOMPARE(fixture.queue->itemValues().at(0).state, TransferState::Cancelled);
  QCOMPARE(fixture.worker->cancelled(), QVector<quint64>{first});
  // The next item starts immediately; a quiet kill delivers no result.
  QCOMPARE(fixture.worker->started().size(), 2);

  // AGENT-GUARD: the fence. A late success for a cancelled item must not
  // resurrect it, commit its destination, or disturb what is now running.
  fixture.worker->finish(first);
  QCOMPARE(fixture.queue->itemValues().at(0).state, TransferState::Cancelled);
  QCOMPARE(committed.count(), 0);
  QCOMPARE(fixture.queue->itemValues().at(1).state, TransferState::Running);
  QCOMPARE(fixture.worker->started().size(), 2);

  // Cancelling an unknown or already-retired id is a no-op.
  fixture.queue->cancel(first);
  fixture.queue->cancel(9999);
  QCOMPARE(fixture.worker->cancelled().size(), 1);
}

void TestTransferQueue::cancelAllRetiresEveryLiveItem() {
  Fixture fixture;
  QVERIFY(fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt"),
                                  QStringLiteral("/home/cabewse/b.txt"),
                                  QStringLiteral("/home/cabewse/c.txt")},
                                 QStringLiteral("sftp://qinda/mnt"),
                                 QStringLiteral("copy")));
  fixture.queue->cancelAll();
  QVERIFY(!fixture.queue->busy());
  for (const TransferItem &item : fixture.queue->itemValues()) {
    QCOMPARE(item.state, TransferState::Cancelled);
  }
  // Only the item that had actually been dispatched reaches the worker.
  QCOMPARE(fixture.worker->started().size(), 1);
  QCOMPARE(fixture.worker->cancelled().size(), 3);
}

void TestTransferQueue::clearFinishedKeepsLiveItems() {
  Fixture fixture;
  QVERIFY(fixture.queue->enqueue({QStringLiteral("/home/cabewse/a.txt"),
                                  QStringLiteral("/home/cabewse/b.txt")},
                                 QStringLiteral("sftp://qinda/mnt"),
                                 QStringLiteral("copy")));
  fixture.worker->finish(fixture.worker->started().constFirst().id);
  fixture.queue->clearFinished();
  QCOMPARE(fixture.queue->itemValues().size(), 1);
  QCOMPARE(fixture.queue->itemValues().constFirst().state, TransferState::Running);
  QVERIFY(fixture.queue->busy());
}

QTEST_MAIN(TestTransferQueue)
#include "tst_transfer_queue.moc"
