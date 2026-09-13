// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_remote_copier.h"

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production copier's own scheme/authority boundary
// (which URLs reach KIO at all) without ever starting a real KIO job: no
// DNS, socket, SMB/SFTP server, or filesystem is reachable from this file.
class RecordingCopier final : public KioRemoteCopier {
public:
  [[nodiscard]] KIO::CopyJob *createCopyJob(const QUrl &source,
                                            const QUrl &destination) const override {
    m_requestedSources.append(source);
    m_requestedDestinations.append(destination);
    return nullptr;
  }

  mutable QVector<QUrl> m_requestedSources;
  mutable QVector<QUrl> m_requestedDestinations;
};

// Test double returning a real KIO::CopyJob so a test can inspect the UI
// delegate seams the production copy() left on a supported job. No network
// is touched: the event loop is never processed, so the started job never
// connects a slave, and the copier's destruction kills it quietly.
class DelegateProbingCopier final : public KioRemoteCopier {
public:
  [[nodiscard]] KIO::CopyJob *createCopyJob(const QUrl &source,
                                            const QUrl &destination) const override {
    m_job = KIO::copy(source, destination, KIO::HideProgressInfo);
    return m_job;
  }

  mutable KIO::CopyJob *m_job = nullptr;
};

[[nodiscard]] QUrl smb(const QString &path) { return QUrl(QStringLiteral("smb://server/share") + path); }

} // namespace

class TestKioRemoteCopier final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesACrossAuthorityCopyBeforeCreatingAJob();
  void aSupportedJobKeepsTheStandardKioUiDelegate();
  void destructionWithAPendingCopyDoesNotCrash();
};

void TestKioRemoteCopier::refusesAnUnsupportedSchemeBeforeCreatingAJob() {
  RecordingCopier copier;
  QSignalSpy finished(&copier, &RemoteCopier::copyFinished);

  copier.copy(1, QUrl(QStringLiteral("http://example.com/a")),
              QUrl(QStringLiteral("http://example.com/b")));

  QVERIFY(copier.m_requestedSources.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteCopier::refusesACrossAuthorityCopyBeforeCreatingAJob() {
  RecordingCopier copier;
  QSignalSpy finished(&copier, &RemoteCopier::copyFinished);

  // Same-looking path, different authority: never reaches KIO.
  copier.copy(2, smb(QStringLiteral("/notes.txt")),
              QUrl(QStringLiteral("sftp://server/share/notes.txt")));

  QVERIFY(copier.m_requestedSources.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteCopier::aSupportedJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingCopier copier;
  const QUrl source = smb(QStringLiteral("/notes.txt"));
  const QUrl destination = QUrl(QStringLiteral("smb://server/backup/notes.txt"));

  copier.copy(3, source, destination);

  QVERIFY(copier.m_job != nullptr);
  QVERIFY2(copier.m_job->uiDelegate() != nullptr,
           "supported copy job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(copier.m_job->uiDelegateExtension() != nullptr,
           "supported copy job lost its KIO UI delegate extension");
  copier.cancel(3);
}

void TestKioRemoteCopier::destructionWithAPendingCopyDoesNotCrash() {
  {
    DelegateProbingCopier copier;
    copier.copy(4, smb(QStringLiteral("/notes.txt")),
                QUrl(QStringLiteral("smb://server/backup/notes.txt")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestKioRemoteCopier)
#include "tst_kio_remote_copier.moc"
