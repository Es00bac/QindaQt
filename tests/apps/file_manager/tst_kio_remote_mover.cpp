// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_remote_mover.h"

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production mover's own scheme/authority boundary
// (which URLs reach KIO at all) without ever starting a real KIO job: no
// DNS, socket, SMB/SFTP server, or filesystem is reachable from this file.
class RecordingMover final : public KioRemoteMover {
public:
  [[nodiscard]] KIO::CopyJob *createMoveJob(const QUrl &source,
                                            const QUrl &destination) const override {
    m_requestedSources.append(source);
    m_requestedDestinations.append(destination);
    return nullptr;
  }

  mutable QVector<QUrl> m_requestedSources;
  mutable QVector<QUrl> m_requestedDestinations;
};

// Test double returning a real KIO::CopyJob (KIO::move() is a CopyJob, like
// KIO::copy()) so a test can inspect the UI delegate seams the production
// move() left on a supported job. No network is touched: the event loop is
// never processed, so the started job never connects a slave, and the
// mover's destruction kills it quietly.
class DelegateProbingMover final : public KioRemoteMover {
public:
  [[nodiscard]] KIO::CopyJob *createMoveJob(const QUrl &source,
                                            const QUrl &destination) const override {
    m_job = KIO::move(source, destination, KIO::HideProgressInfo);
    return m_job;
  }

  mutable KIO::CopyJob *m_job = nullptr;
};

[[nodiscard]] QUrl smb(const QString &path) { return QUrl(QStringLiteral("smb://server/share") + path); }

} // namespace

// ADR-0156: the production KioRemoteMover keeps the same policy boundary as
// KioRemoteCopier (supported scheme, one authority, no userinfo) and the
// same retained KIO UI delegate (ordinary authentication prompts), on KIO's
// supported move() facility instead of copy().
class TestKioRemoteMover final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesACrossAuthorityMoveBeforeCreatingAJob();
  void aSupportedJobKeepsTheStandardKioUiDelegate();
  void destructionWithAPendingMoveDoesNotCrash();
};

void TestKioRemoteMover::refusesAnUnsupportedSchemeBeforeCreatingAJob() {
  RecordingMover mover;
  QSignalSpy finished(&mover, &RemoteMover::moveFinished);

  mover.move(1, QUrl(QStringLiteral("http://example.com/a")),
             QUrl(QStringLiteral("http://example.com/b")));

  QVERIFY(mover.m_requestedSources.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteMover::refusesACrossAuthorityMoveBeforeCreatingAJob() {
  RecordingMover mover;
  QSignalSpy finished(&mover, &RemoteMover::moveFinished);

  // Same-looking path, different authority: never reaches KIO.
  mover.move(2, smb(QStringLiteral("/notes.txt")),
             QUrl(QStringLiteral("sftp://server/share/notes.txt")));

  QVERIFY(mover.m_requestedSources.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteMover::aSupportedJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingMover mover;
  const QUrl source = smb(QStringLiteral("/notes.txt"));
  const QUrl destination = QUrl(QStringLiteral("smb://server/backup/notes.txt"));

  mover.move(3, source, destination);

  QVERIFY(mover.m_job != nullptr);
  QVERIFY2(mover.m_job->uiDelegate() != nullptr,
           "supported move job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(mover.m_job->uiDelegateExtension() != nullptr,
           "supported move job lost its KIO UI delegate extension");
  mover.cancel(3);
}

void TestKioRemoteMover::destructionWithAPendingMoveDoesNotCrash() {
  {
    DelegateProbingMover mover;
    mover.move(4, smb(QStringLiteral("/notes.txt")),
               QUrl(QStringLiteral("smb://server/backup/notes.txt")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestKioRemoteMover)
#include "tst_kio_remote_mover.moc"
