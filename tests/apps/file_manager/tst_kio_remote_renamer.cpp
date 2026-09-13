// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_remote_renamer.h"

#include <KIO/Job>
#include <KIO/SimpleJob>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production renamer's own scheme/parent boundary
// (which URLs reach KIO at all) without ever starting a real KIO job: no
// DNS, socket, SMB/SFTP server, or filesystem is reachable from this file.
class RecordingRenamer final : public KioRemoteRenamer {
public:
  [[nodiscard]] KIO::SimpleJob *createRenameJob(const QUrl &source,
                                                const QUrl &destination) const override {
    m_requestedSources.append(source);
    m_requestedDestinations.append(destination);
    return nullptr;
  }

  mutable QVector<QUrl> m_requestedSources;
  mutable QVector<QUrl> m_requestedDestinations;
};

// Test double returning a real KIO::SimpleJob so a test can inspect the UI
// delegate seams the production rename() left on a supported same-folder
// job. No network is touched: the event loop is never processed, so the
// started job never connects a slave, and the renamer's destruction kills
// it quietly.
class DelegateProbingRenamer final : public KioRemoteRenamer {
public:
  [[nodiscard]] KIO::SimpleJob *createRenameJob(const QUrl &source,
                                                const QUrl &destination) const override {
    m_job = KIO::rename(source, destination, KIO::HideProgressInfo);
    return m_job;
  }

  mutable KIO::SimpleJob *m_job = nullptr;
};

[[nodiscard]] QUrl smb(const QString &path) { return QUrl(QStringLiteral("smb://server/share") + path); }

} // namespace

class TestKioRemoteRenamer final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesACrossFolderRenameBeforeCreatingAJob();
  void aSupportedSameFolderJobKeepsTheStandardKioUiDelegate();
  void destructionWithAPendingRenameDoesNotCrash();
};

void TestKioRemoteRenamer::refusesAnUnsupportedSchemeBeforeCreatingAJob() {
  RecordingRenamer renamer;
  QSignalSpy finished(&renamer, &RemoteRenamer::renameFinished);

  renamer.rename(1, QUrl(QStringLiteral("http://example.com/a")),
                 QUrl(QStringLiteral("http://example.com/b")));

  QVERIFY(renamer.m_requestedSources.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteRenamer::refusesACrossFolderRenameBeforeCreatingAJob() {
  RecordingRenamer renamer;
  QSignalSpy finished(&renamer, &RemoteRenamer::renameFinished);

  // Same authority, different parent directory: never reaches KIO.
  renamer.rename(1, smb(QStringLiteral("/notes.txt")),
                 QUrl(QStringLiteral("smb://server/other/notes.txt")));

  QVERIFY(renamer.m_requestedSources.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(1).toString().isEmpty());
}

void TestKioRemoteRenamer::aSupportedSameFolderJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingRenamer renamer;
  const QUrl source = smb(QStringLiteral("/notes.txt"));
  const QUrl destination = smb(QStringLiteral("/report.txt"));

  renamer.rename(2, source, destination);

  QVERIFY(renamer.m_job != nullptr);
  QVERIFY2(renamer.m_job->uiDelegate() != nullptr,
           "supported rename job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(renamer.m_job->uiDelegateExtension() != nullptr,
           "supported rename job lost its KIO UI delegate extension");
  renamer.cancel(2);
}

void TestKioRemoteRenamer::destructionWithAPendingRenameDoesNotCrash() {
  {
    DelegateProbingRenamer renamer;
    renamer.rename(3, smb(QStringLiteral("/notes.txt")), smb(QStringLiteral("/report.txt")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestKioRemoteRenamer)
#include "tst_kio_remote_renamer.moc"
