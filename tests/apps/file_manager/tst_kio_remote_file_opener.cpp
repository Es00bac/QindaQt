// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_remote_file_opener.h"

#include <KIO/Job>
#include <KIO/OpenUrlJob>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production opener's own scheme/policy boundary
// (which URLs reach KIO at all) without ever starting a real KIO::OpenUrlJob:
// no DNS, socket, SMB/SFTP server, desktop handler, or credential prompt is
// reachable from this file.
class RecordingOpener final : public KioRemoteFileOpener {
public:
  [[nodiscard]] KIO::OpenUrlJob *createOpenUrlJob(const QUrl &url) const override {
    m_requestedJobUrls.append(url);
    return nullptr;
  }

  mutable QVector<QUrl> m_requestedJobUrls;
};

// Test double returning a real KIO::OpenUrlJob so a test can inspect the UI
// delegate seams the production open() left on a supported smb/sftp job. No
// network is touched: the event loop is never processed, so the started job
// never connects a slave, and the opener's destruction kills it quietly.
class DelegateProbingOpener final : public KioRemoteFileOpener {
public:
  [[nodiscard]] KIO::OpenUrlJob *createOpenUrlJob(const QUrl &url) const override {
    m_job = new KIO::OpenUrlJob(url, const_cast<DelegateProbingOpener *>(this));
    return m_job;
  }

  mutable KIO::OpenUrlJob *m_job = nullptr;
};

} // namespace

class TestKioRemoteFileOpener final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesALocalPathBeforeCreatingAJob();
  void aSupportedSmbJobKeepsTheStandardKioUiDelegate();
  void destructionWithAPendingOpenDoesNotCrash();
};

void TestKioRemoteFileOpener::refusesAnUnsupportedSchemeBeforeCreatingAJob() {
  RecordingOpener opener;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);

  opener.open(QUrl(QStringLiteral("http://example.com/file.txt")));

  QVERIFY(opener.m_requestedJobUrls.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(0).toString().isEmpty());
}

void TestKioRemoteFileOpener::refusesALocalPathBeforeCreatingAJob() {
  RecordingOpener opener;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);

  opener.open(QUrl(QStringLiteral("/home/jarrod/notes.txt")));

  QVERIFY(opener.m_requestedJobUrls.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(0).toString().isEmpty());
}

// ADR-0151/0152: a supported open job must keep the platform's standard KIO
// UI delegate, or a slave that needs credentials can never prompt. (Unlike
// KIO::ListJob, OpenUrlJob is a KCompositeJob: its delegate extension lives
// on the KIO::Job subjobs it starts, so only the delegate seam is visible
// here.) Mirrors the listing-backend proof.
void TestKioRemoteFileOpener::aSupportedSmbJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingOpener opener;
  const QUrl url(QStringLiteral("smb://server/share/notes.txt"));

  opener.open(url);

  QVERIFY(opener.m_job != nullptr);
  QVERIFY2(opener.m_job->uiDelegate() != nullptr,
           "supported smb open job lost its KIO UI delegate (credential prompt path)");
}

void TestKioRemoteFileOpener::destructionWithAPendingOpenDoesNotCrash() {
  {
    DelegateProbingOpener opener;
    opener.open(QUrl(QStringLiteral("sftp://server/home/notes.txt")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestKioRemoteFileOpener)
#include "tst_kio_remote_file_opener.moc"
