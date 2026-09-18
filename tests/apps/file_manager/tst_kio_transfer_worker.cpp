// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_transfer_worker.h"

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production worker's own realm boundary -- which
// URL pairs reach KIO at all -- without ever starting a real KIO job: no DNS,
// socket, SMB/SFTP server, or filesystem is reachable from this file.
class RecordingWorker final : public KioTransferWorker {
public:
  [[nodiscard]] KIO::CopyJob *createJob(const QUrl &source,
                                        const QUrl &destinationFolder,
                                        TransferOperation operation) const override {
    m_sources.append(source);
    m_destinations.append(destinationFolder);
    m_operations.append(operation);
    return nullptr;
  }

  mutable QVector<QUrl> m_sources;
  mutable QVector<QUrl> m_destinations;
  mutable QVector<TransferOperation> m_operations;
};

// Test double returning a real KIO::CopyJob so a test can inspect the UI
// delegate seams the production start() left on a supported job. The event
// loop is never processed, so the started job never connects a worker
// process, and destruction kills it quietly.
class DelegateProbingWorker final : public KioTransferWorker {
public:
  [[nodiscard]] KIO::CopyJob *createJob(const QUrl &source,
                                        const QUrl &destinationFolder,
                                        TransferOperation operation) const override {
    m_job = operation == TransferOperation::Move
                ? KIO::move(source, destinationFolder, KIO::HideProgressInfo)
                : KIO::copy(source, destinationFolder, KIO::HideProgressInfo);
    return m_job;
  }

  mutable KIO::CopyJob *m_job = nullptr;
};

[[nodiscard]] QUrl local(const QString &path) {
  return QUrl::fromLocalFile(path);
}

} // namespace

class TestKioTransferWorker final : public QObject {
  Q_OBJECT

private slots:
  void acceptsOnlyPairsWithANetworkEndpoint();
  void refusesUnusableEndpointsBeforeCreatingAJob();
  void carriesTheRequestedOperationAndUrlsToTheJobSeam();
  void aSupportedJobKeepsTheStandardKioUiDelegate();
  void destructionWithAPendingTransferDoesNotCrash();
};

void TestKioTransferWorker::acceptsOnlyPairsWithANetworkEndpoint() {
  const QUrl remote(QStringLiteral("sftp://qinda/mnt/storage"));
  const QUrl otherRemote(QStringLiteral("smb://nas/share"));

  QVERIFY(KioTransferWorker::isTransferableAcrossRealms(local(QStringLiteral("/home/cabewse/a.txt")), remote));
  QVERIFY(KioTransferWorker::isTransferableAcrossRealms(remote, local(QStringLiteral("/home/cabewse"))));
  QVERIFY(KioTransferWorker::isTransferableAcrossRealms(remote, otherRemote));

  // AGENT-GUARD: a purely local transfer must never reach KIO from here --
  // MutationController owns local copies and moves, with identity checks,
  // Trash, and undo this worker has not.
  QVERIFY(!KioTransferWorker::isTransferableAcrossRealms(
      local(QStringLiteral("/home/cabewse/a.txt")), local(QStringLiteral("/tmp"))));
}

void TestKioTransferWorker::refusesUnusableEndpointsBeforeCreatingAJob() {
  RecordingWorker worker;
  QSignalSpy finished(&worker, &TransferWorker::finished);

  const QUrl remote(QStringLiteral("sftp://qinda/mnt"));
  const struct {
    QUrl source;
    QUrl destination;
  } refused[] = {
      {QUrl(QStringLiteral("http://example.com/a")), remote},
      {remote, QUrl(QStringLiteral("http://example.com/b"))},
      // Userinfo on either side is refused independently of the router.
      {QUrl(QStringLiteral("sftp://cabewse@qinda/mnt/a.txt")), local(QStringLiteral("/tmp"))},
      {local(QStringLiteral("/home/cabewse/a.txt")),
       QUrl(QStringLiteral("sftp://cabewse:secret@qinda/mnt"))},
      // A relative local path is not an addressable endpoint.
      {QUrl(QStringLiteral("file:relative")), remote},
      {local(QStringLiteral("/home/cabewse/a.txt")), local(QStringLiteral("/tmp"))},
  };
  quint64 id = 1;
  for (const auto &pair : refused) {
    worker.start(id, pair.source, pair.destination, TransferOperation::Copy);
    ++id;
  }
  QVERIFY(worker.m_sources.isEmpty());
  QCOMPARE(finished.size(), static_cast<int>(std::size(refused)));
  for (const auto &emission : finished) {
    QVERIFY(!emission.at(1).toString().isEmpty());
  }
}

void TestKioTransferWorker::carriesTheRequestedOperationAndUrlsToTheJobSeam() {
  RecordingWorker worker;
  QSignalSpy finished(&worker, &TransferWorker::finished);
  const QUrl source = local(QStringLiteral("/home/cabewse/a.txt"));
  const QUrl destination(QStringLiteral("sftp://qinda/mnt/storage"));

  worker.start(7, source, destination, TransferOperation::Move);
  QCOMPARE(worker.m_sources, QVector<QUrl>{source});
  QCOMPARE(worker.m_destinations, QVector<QUrl>{destination});
  // AGENT-GUARD: a move must never be substituted by a copy or the reverse.
  QCOMPARE(worker.m_operations, QVector<TransferOperation>{TransferOperation::Move});

  // A null job from the seam still answers exactly once, so the queue can
  // never wait forever on a facility that refused to build a job.
  QCOMPARE(finished.size(), 1);
  QCOMPARE(finished.constFirst().at(0).toULongLong(), 7ULL);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioTransferWorker::aSupportedJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingWorker worker;
  worker.start(3, local(QStringLiteral("/home/cabewse/a.txt")),
               QUrl(QStringLiteral("sftp://qinda/mnt/storage")), TransferOperation::Copy);

  QVERIFY(worker.m_job != nullptr);
  QVERIFY2(worker.m_job->uiDelegate() != nullptr,
           "supported transfer job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(worker.m_job->uiDelegateExtension() != nullptr,
           "supported transfer job lost its KIO UI delegate extension");
  worker.cancel(3);
  // Cancelling twice, and cancelling an id that never ran, stay no-ops.
  worker.cancel(3);
  worker.cancel(999);
  worker.pause(999);
  worker.resume(999);
}

void TestKioTransferWorker::destructionWithAPendingTransferDoesNotCrash() {
  {
    DelegateProbingWorker worker;
    worker.start(4, local(QStringLiteral("/home/cabewse/a.txt")),
                 QUrl(QStringLiteral("sftp://qinda/mnt/storage")),
                 TransferOperation::Copy);
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestKioTransferWorker)
#include "tst_kio_transfer_worker.moc"
