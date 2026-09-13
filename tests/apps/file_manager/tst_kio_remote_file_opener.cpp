// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_remote_file_opener.h"
#include "runtime/file_manager_application.h"

#include <KIO/Job>
#include <KIO/OpenUrlJob>
#include <KJob>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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

// Review-P1 probe equivalent: a real KIO::OpenUrlJob pointed at a local
// hermetic file whose explicit MIME type has no associated application, so
// the job reaches KIO's standard Open With handler without any worker,
// network, or desktop handler. Mirrors the reviewer's hermetic probe.
class UnassociatedLocalFileOpener final : public KioRemoteFileOpener {
public:
  [[nodiscard]] bool prepare() {
    if (!m_fixture.isValid()) {
      return false;
    }
    QFile target(m_fixture.filePath(QStringLiteral("sample.unknown")));
    if (!target.open(QIODevice::WriteOnly)) {
      return false;
    }
    const QByteArray payload("fixture");
    return target.write(payload) == payload.size();
  }

  [[nodiscard]] KIO::OpenUrlJob *createOpenUrlJob(const QUrl &) const override {
    // The explicit MIME type skips KIO's worker-based type detection, the
    // same terminal path a remote file takes once KIO knows its type.
    m_job = new KIO::OpenUrlJob(
        QUrl::fromLocalFile(m_fixture.filePath(QStringLiteral("sample.unknown"))),
        QStringLiteral("application/x-qindaqt-opener-unknown"),
        const_cast<UnassociatedLocalFileOpener *>(this));
    return m_job;
  }

  mutable KIO::OpenUrlJob *m_job = nullptr;

private:
  QTemporaryDir m_fixture;
};

} // namespace

class TestKioRemoteFileOpener final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesALocalPathBeforeCreatingAJob();
  void aSupportedSmbJobKeepsTheStandardKioUiDelegate();
  void anUnassociatedFileTypeReachesTheStandardOpenWithPrompt();
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

// Review P1 on 2b37f9c1: with no associated application, KIO's standard
// widgets delegate constructs its Open With dialog (a QWidget). Under the
// pre-repair QGuiApplication composition this aborted the whole process
// (SIGABRT, "QWidget: Cannot create a QWidget without QApplication"). This
// row drives the real open() through that branch under the same application
// class production composes (the shared factory), dismisses the dialog
// hermetically, and asserts exactly one openFinished. It makes no network or
// desktop-handler contact.
void TestKioRemoteFileOpener::anUnassociatedFileTypeReachesTheStandardOpenWithPrompt() {
  UnassociatedLocalFileOpener opener;
  QVERIFY(opener.prepare());
  QSignalSpy finished(&opener, &RemoteFileOpener::openFinished);

  // The dialog is shown non-modally; poll and close it so the job reports
  // cancellation instead of outliving the test.
  QTimer dismisser;
  QObject::connect(&dismisser, &QTimer::timeout, qApp, [] {
    const QWidgetList topLevel = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevel) {
      if (auto *dialog = qobject_cast<QDialog *>(widget); dialog && dialog->isVisible()) {
        dialog->close();
        return;
      }
    }
  });
  dismisser.start(100);

  opener.open(QUrl(QStringLiteral("smb://server/share/sample.unknown")));

  QVERIFY2(finished.wait(20000),
           "open with no associated application never produced openFinished "
           "(KIO's standard Open With prompt aborted or hung the process)");
  dismisser.stop();
  QCOMPARE(finished.size(), 1);
  QVERIFY(opener.m_job != nullptr);
  QVERIFY2(qobject_cast<QApplication *>(QCoreApplication::instance()) != nullptr,
           "the File Manager process must run a QWidget-capable application "
           "for KIO's standard prompts to survive");
}

void TestKioRemoteFileOpener::destructionWithAPendingOpenDoesNotCrash() {
  {
    DelegateProbingOpener opener;
    opener.open(QUrl(QStringLiteral("sftp://server/home/notes.txt")));
  }
  QVERIFY(true);
}

// AGENT-NOTE: a custom main (not QTEST_GUILESS_MAIN) so the test process
// composes its application through the same production factory as main.cpp;
// that is what makes the row above run under the production application
// class and keeps it discriminating.
int main(int argc, char *argv[]) {
  auto application = QindaQt::Apps::FileManager::createApplication(argc, argv);
  Q_UNUSED(application);
  TestKioRemoteFileOpener test;
  return QTest::qExec(&test, argc, argv);
}

#include "tst_kio_remote_file_opener.moc"
