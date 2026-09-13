// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_remote_folder_creator.h"

#include <KIO/Job>
#include <KIO/MkdirJob>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production creator's own scheme/userinfo boundary
// (which URLs reach KIO at all) without ever starting a real KIO job: no
// DNS, socket, SMB/SFTP server, or filesystem is reachable from this file.
class RecordingCreator final : public KioRemoteFolderCreator {
public:
  [[nodiscard]] KIO::MkdirJob *createMkdirJob(const QUrl &url) const override {
    m_requestedUrls.append(url);
    return nullptr;
  }

  mutable QVector<QUrl> m_requestedUrls;
};

// Test double returning a real KIO::MkdirJob so a test can inspect the UI
// delegate seams the production createFolder() left on a supported job. No
// network is touched: the event loop is never processed, so the started job
// never connects a slave, and the creator's destruction kills it quietly.
class DelegateProbingCreator final : public KioRemoteFolderCreator {
public:
  [[nodiscard]] KIO::MkdirJob *createMkdirJob(const QUrl &url) const override {
    m_job = KIO::mkdir(url);
    return m_job;
  }

  mutable KIO::MkdirJob *m_job = nullptr;
};

} // namespace

class TestKioRemoteFolderCreator final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesUserinfoBeforeCreatingAJob();
  void aSupportedJobKeepsTheStandardKioUiDelegate();
  void destructionWithAPendingCreateDoesNotCrash();
};

void TestKioRemoteFolderCreator::refusesAnUnsupportedSchemeBeforeCreatingAJob() {
  RecordingCreator creator;
  QSignalSpy finished(&creator, &RemoteFolderCreator::createFinished);

  creator.createFolder(1, QUrl(QStringLiteral("http://example.com/newdir")));

  QVERIFY(creator.m_requestedUrls.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteFolderCreator::refusesUserinfoBeforeCreatingAJob() {
  RecordingCreator creator;
  QSignalSpy finished(&creator, &RemoteFolderCreator::createFinished);

  creator.createFolder(2, QUrl(QStringLiteral("smb://user:pass@server/share/newdir")));

  QVERIFY(creator.m_requestedUrls.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteFolderCreator::aSupportedJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingCreator creator;
  const QUrl url(QStringLiteral("smb://server/share/New%20Folder"));

  creator.createFolder(3, url);

  QVERIFY(creator.m_job != nullptr);
  QVERIFY2(creator.m_job->uiDelegate() != nullptr,
           "supported mkdir job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(creator.m_job->uiDelegateExtension() != nullptr,
           "supported mkdir job lost its KIO UI delegate extension");
  creator.cancel(3);
}

void TestKioRemoteFolderCreator::destructionWithAPendingCreateDoesNotCrash() {
  {
    DelegateProbingCreator creator;
    creator.createFolder(4, QUrl(QStringLiteral("sftp://server/home/newdir")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestKioRemoteFolderCreator)
#include "tst_kio_remote_folder_creator.moc"
