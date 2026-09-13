// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_fuse_remote_file_opener.h"
#include "network/network_location.h"
#include "runtime/file_manager_application.h"

#include <KIO/Job>
#include <KIO/OpenUrlJob>
#include <KJob>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production write-in-place opener's own policy
// boundary and resolve/fallback flow without a session bus, KIOFuse daemon,
// network, or desktop handler: canResolve()/createMountCall() are faked with
// canned QDBusPendingCall replies, and createOpenUrlJob() is either a pure
// recorder (nullptr return, so no KIO job is ever started) or returns a real
// KIO::OpenUrlJob the test inspects and destroys without further event-loop
// pumping for remote URLs.
class FakeFuseOpener : public KioFuseRemoteFileOpener {
public:
  [[nodiscard]] bool canResolve() const override { return m_canResolve; }

  [[nodiscard]] QDBusPendingCall createMountCall(const QUrl &url) const override {
    m_mountUrls.append(url);
    return m_replies.isEmpty()
               ? QDBusPendingCall::fromError(
                     QDBusError(QDBusError::InternalError, QStringLiteral("no canned reply")))
               : m_replies.takeFirst();
  }

  [[nodiscard]] KIO::OpenUrlJob *createOpenUrlJob(const QUrl &url) const override {
    m_jobUrls.append(url);
    if (!m_returnRealJob) {
      return nullptr;
    }
    m_job = new KIO::OpenUrlJob(url, const_cast<FakeFuseOpener *>(this));
    return m_job;
  }

  static QDBusPendingCall mountReply(const QString &localPath) {
    const QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KIOFuse"), QStringLiteral("/org/kde/KIOFuse"),
        QStringLiteral("org.kde.KIOFuse.VFS"), QStringLiteral("mountUrl"));
    return QDBusPendingCall::fromCompletedCall(request.createReply(localPath));
  }

  static QDBusPendingCall mountError() {
    return QDBusPendingCall::fromError(
        QDBusError(QDBusError::InternalError, QStringLiteral("mount refused")));
  }

  bool m_canResolve = true;
  mutable QVector<QDBusPendingCall> m_replies;
  bool m_returnRealJob = false;
  mutable QVector<QUrl> m_mountUrls;
  mutable QVector<QUrl> m_jobUrls;
  mutable KIO::OpenUrlJob *m_job = nullptr;
};

// A nonexistent absolute path: the resolved-open job is a plain file:// job
// whose file slave fails locally ("does not exist"), so the row can pump the
// event loop hermetically -- no slave with a network protocol is ever
// started, mirroring the ADR-0152 Open With row's local-only trick.
const auto kResolvedPath = QStringLiteral("/nonexistent-kiofuse-root/smb/server/share/notes.txt");

} // namespace

class TestKioFuseRemoteFileOpener final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeAnyContact();
  void refusesUserInfoBeforeAnyContact();
  void resolvedLocalPathIsOpenedThroughTheStandardOpenUrlJob();
  void mountErrorFallsBackToTheDirectRemoteOpen();
  void malformedMountRepliesFallBackToTheDirectRemoteOpen_data();
  void malformedMountRepliesFallBackToTheDirectRemoteOpen();
  void missingFacilityFallsBackWithoutAnyMountContact();
  void concurrentOpensResolveIndependently();
  void destructionWithAPendingResolutionIsQuiet();
};

void TestKioFuseRemoteFileOpener::refusesAnUnsupportedSchemeBeforeAnyContact() {
  FakeFuseOpener opener;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);

  opener.open(QUrl(QStringLiteral("http://example.com/file.txt")));

  QVERIFY(opener.m_mountUrls.isEmpty());
  QVERIFY(opener.m_jobUrls.isEmpty());
  QCOMPARE(finished.size(), 1);
  QVERIFY(!finished.constFirst().at(0).toString().isEmpty());
}

// ADR-0157: a URL carrying userinfo is refused BEFORE any D-Bus contact, so
// no credential material can leave the process through the mount call.
void TestKioFuseRemoteFileOpener::refusesUserInfoBeforeAnyContact() {
  FakeFuseOpener opener;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);

  opener.open(QUrl(QStringLiteral("smb://user:secret@server/share/notes.txt")));

  QVERIFY(opener.m_mountUrls.isEmpty());
  QVERIFY(opener.m_jobUrls.isEmpty());
  QCOMPARE(finished.size(), 1);
  const QString diagnostic = finished.constFirst().at(0).toString();
  QVERIFY(!diagnostic.isEmpty());
  QVERIFY(!diagnostic.contains(QStringLiteral("secret")));
  QVERIFY(diagnostic.size() <= NetworkLocation::maximumDiagnosticLength);
}

// The success path: the canonical remote URL is handed to the KIOFuse mount
// call verbatim, and the returned local path -- not the remote URL -- is
// what reaches KIO's standard open job with the standard UI delegate kept.
// The resolved file:// job fails locally (path does not exist) and reports
// one typed openFinished; no smb/sftp slave is ever started.
void TestKioFuseRemoteFileOpener::resolvedLocalPathIsOpenedThroughTheStandardOpenUrlJob() {
  FakeFuseOpener opener;
  opener.m_replies.append(FakeFuseOpener::mountReply(kResolvedPath));
  opener.m_returnRealJob = true;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);
  const QUrl remote(QStringLiteral("smb://server/share/notes.txt"));

  opener.open(remote);

  QTRY_VERIFY_WITH_TIMEOUT(!opener.m_jobUrls.isEmpty(), 10000);
  QCOMPARE(opener.m_mountUrls, QVector<QUrl>{remote});
  QCOMPARE(opener.m_jobUrls,
           QVector<QUrl>{QUrl::fromLocalFile(kResolvedPath)});
  QVERIFY(opener.m_job != nullptr);
  QVERIFY2(opener.m_job->uiDelegate() != nullptr,
           "resolved local open job lost its KIO UI delegate (credential prompt path)");
  QTRY_VERIFY_WITH_TIMEOUT(finished.size() == 1, 10000);
  QVERIFY(!finished.constFirst().at(0).toString().isEmpty());
}

// The facility answered with an error: the opener must fall back to the
// ADR-0152 direct open of the SAME canonical remote URL. The recorder
// returns no job, so nothing is ever started on a network protocol.
void TestKioFuseRemoteFileOpener::mountErrorFallsBackToTheDirectRemoteOpen() {
  FakeFuseOpener opener;
  opener.m_replies.append(FakeFuseOpener::mountError());
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);
  const QUrl remote(QStringLiteral("sftp://server/home/notes.txt"));

  opener.open(remote);

  QTRY_VERIFY_WITH_TIMEOUT(!opener.m_jobUrls.isEmpty(), 10000);
  QCOMPARE(opener.m_mountUrls, QVector<QUrl>{remote});
  QCOMPARE(opener.m_jobUrls, QVector<QUrl>{remote});
  QTRY_VERIFY_WITH_TIMEOUT(finished.size() == 1, 10000);
  QVERIFY(!finished.constFirst().at(0).toString().isEmpty());
}

void TestKioFuseRemoteFileOpener::malformedMountRepliesFallBackToTheDirectRemoteOpen_data() {
  QTest::addColumn<QString>("reply");

  QTest::newRow("empty") << QString();
  QTest::newRow("relative") << QStringLiteral("kiofuse/smb/server");
}

void TestKioFuseRemoteFileOpener::malformedMountRepliesFallBackToTheDirectRemoteOpen() {
  QFETCH(QString, reply);
  FakeFuseOpener opener;
  opener.m_replies.append(FakeFuseOpener::mountReply(reply));
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);
  const QUrl remote(QStringLiteral("smb://server/share/notes.txt"));

  opener.open(remote);

  QTRY_VERIFY_WITH_TIMEOUT(!opener.m_jobUrls.isEmpty(), 10000);
  QCOMPARE(opener.m_jobUrls, QVector<QUrl>{remote});
  QTRY_VERIFY_WITH_TIMEOUT(finished.size() == 1, 10000);
}

// No session bus (or the KIOFuse service otherwise unreachable before any
// call): the opener must not touch the mount facility at all and open the
// remote URL directly -- byte-for-byte the pre-ADR-0157 behavior.
void TestKioFuseRemoteFileOpener::missingFacilityFallsBackWithoutAnyMountContact() {
  FakeFuseOpener opener;
  opener.m_canResolve = false;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);
  const QUrl remote(QStringLiteral("smb://server/share/notes.txt"));

  opener.open(remote);

  QVERIFY(opener.m_mountUrls.isEmpty());
  QCOMPARE(opener.m_jobUrls, QVector<QUrl>{remote});
  QCOMPARE(finished.size(), 1);
}

// Two quick opens are independent operations: each mount call carries its
// own canonical URL and each resolution opens its own local path -- a late
// reply for one can never retire or redirect the other.
void TestKioFuseRemoteFileOpener::concurrentOpensResolveIndependently() {
  const QString pathA = QStringLiteral("/nonexistent-kiofuse-root/smb/server/a/one.txt");
  const QString pathB = QStringLiteral("/nonexistent-kiofuse-root/sftp/server/two.txt");
  FakeFuseOpener opener;
  opener.m_replies.append(FakeFuseOpener::mountReply(pathA));
  opener.m_replies.append(FakeFuseOpener::mountReply(pathB));
  opener.m_returnRealJob = true;
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);
  const QUrl remoteA(QStringLiteral("smb://server/a/one.txt"));
  const QUrl remoteB(QStringLiteral("sftp://server/two.txt"));

  opener.open(remoteA);
  opener.open(remoteB);

  QTRY_VERIFY_WITH_TIMEOUT(finished.size() == 2, 10000);
  QCOMPARE(opener.m_mountUrls, (QVector<QUrl>{remoteA, remoteB}));
  QCOMPARE(opener.m_jobUrls,
           (QVector<QUrl>{QUrl::fromLocalFile(pathA), QUrl::fromLocalFile(pathB)}));
}

// Destruction is the cancellation channel for a fire-and-forget open (the
// ADR-0152 contract has no shared Cancel owner): the pending watcher dies
// with the opener, no job is created, and no completion is emitted late.
void TestKioFuseRemoteFileOpener::destructionWithAPendingResolutionIsQuiet() {
  const QUrl remote(QStringLiteral("smb://server/share/notes.txt"));
  auto *opener = new FakeFuseOpener;
  opener->m_replies.append(FakeFuseOpener::mountReply(kResolvedPath));
  QSignalSpy finished(opener, &RemoteFileOpener::openFinished);
  opener->open(remote);

  delete opener;

  QTest::qWait(100);
  QCOMPARE(finished.size(), 0);
}

// AGENT-NOTE: a custom main (not QTEST_GUILESS_MAIN) so the test process
// composes its application through the same production factory as main.cpp;
// that is what keeps the delegate assertions discriminating for the
// QWidget-capable composition KIO's standard prompts require.
int main(int argc, char *argv[]) {
  auto application = QindaQt::Apps::FileManager::createApplication(argc, argv);
  Q_UNUSED(application);
  TestKioFuseRemoteFileOpener test;
  return QTest::qExec(&test, argc, argv);
}

#include "tst_kio_fuse_remote_file_opener.moc"
