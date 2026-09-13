// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_network_directory_backend.h"

#include <KIO/Job>
#include <KIO/ListJob>
#include <KJob>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

// Test double proving the production adapter's own scheme/policy boundary
// (which locations reach job creation at all) without ever starting a real
// KIO::ListJob: no DNS, socket, or SMB/SFTP server is reachable from this
// file. Returning nullptr here also exercises the "job could not be
// created" typed-failure path the same way a genuinely unreachable KIO
// slave would, still through the real requestListing()/finish() logic.
class RecordingKioBackend final : public KioNetworkDirectoryBackend {
public:
  [[nodiscard]] KIO::ListJob *createListJob(const QUrl &url) const override {
    m_requestedJobUrls.append(url);
    return nullptr;
  }

  mutable QVector<QUrl> m_requestedJobUrls;
};

// Test double returning a real KIO::ListJob so a test can inspect the UI
// delegate seams requestListing() left on a supported smb/sftp job. No
// network is touched: the job object is created but the event loop is never
// processed, so the KIO scheduler never connects a slave, and the backend
// destructor kills the pending job quietly.
class DelegateProbingBackend final : public KioNetworkDirectoryBackend {
public:
  [[nodiscard]] KIO::ListJob *createListJob(const QUrl &url) const override {
    m_job = KIO::listDir(url, KIO::HideProgressInfo);
    return m_job;
  }

  mutable KIO::ListJob *m_job = nullptr;
};

} // namespace

class TestKioNetworkDirectoryBackend final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesALocalPathBeforeCreatingAJob();
  void reachesJobCreationForACanonicalSmbUrl();
  void aNullJobBecomesATypedUnavailableResult();
  void cancelOnAnUnknownGenerationIsANoOp();
  void aSupportedSmbJobKeepsTheStandardKioUiDelegate();
  void aSupportedSftpJobKeepsTheStandardKioUiDelegate();
};

void TestKioNetworkDirectoryBackend::refusesAnUnsupportedSchemeBeforeCreatingAJob() {
  RecordingKioBackend backend;
  QSignalSpy ready(&backend, &NetworkDirectoryBackend::listingReady);

  backend.requestListing(1, QUrl(QStringLiteral("http://example.com/share")));

  QVERIFY(backend.m_requestedJobUrls.isEmpty());
  QCOMPARE(ready.size(), 1);
  const auto result = ready.constFirst().at(2).value<NetworkListingResult>();
  QCOMPARE(result.error, NetworkListingError::Unavailable);
}

void TestKioNetworkDirectoryBackend::refusesALocalPathBeforeCreatingAJob() {
  RecordingKioBackend backend;
  QSignalSpy ready(&backend, &NetworkDirectoryBackend::listingReady);

  backend.requestListing(1, QUrl(QStringLiteral("/home/jarrod")));

  QVERIFY(backend.m_requestedJobUrls.isEmpty());
  QCOMPARE(ready.size(), 1);
  const auto result = ready.constFirst().at(2).value<NetworkListingResult>();
  QCOMPARE(result.error, NetworkListingError::Unavailable);
}

void TestKioNetworkDirectoryBackend::reachesJobCreationForACanonicalSmbUrl() {
  RecordingKioBackend backend;
  const QUrl url(QStringLiteral("smb://server/share"));

  backend.requestListing(1, url);

  QCOMPARE(backend.m_requestedJobUrls.size(), 1);
  QCOMPARE(backend.m_requestedJobUrls.constFirst(), url);
}

void TestKioNetworkDirectoryBackend::aNullJobBecomesATypedUnavailableResult() {
  RecordingKioBackend backend;
  QSignalSpy ready(&backend, &NetworkDirectoryBackend::listingReady);
  const QUrl url(QStringLiteral("sftp://server/home"));

  backend.requestListing(7, url);

  QCOMPARE(ready.size(), 1);
  QCOMPARE(ready.constFirst().at(0).toULongLong(), 7ull);
  const auto result = ready.constFirst().at(2).value<NetworkListingResult>();
  QCOMPARE(result.error, NetworkListingError::Unavailable);
  QVERIFY(!result.ok());
}

void TestKioNetworkDirectoryBackend::cancelOnAnUnknownGenerationIsANoOp() {
  RecordingKioBackend backend;
  // No requestListing() ever ran for generation 99; cancel() must not crash
  // or touch anything.
  backend.cancel(99);
  QVERIFY(backend.m_requestedJobUrls.isEmpty());
}

// ADR-0151: a supported network job must keep the platform's standard KIO UI
// delegate and delegate extension, or a slave that needs credentials can
// never prompt and the folder is unreachable. This fails at the ADR-0137
// suppression (both seams cleared to nullptr) even with KIOWidgets loaded.
void TestKioNetworkDirectoryBackend::aSupportedSmbJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingBackend backend;
  const QUrl url(QStringLiteral("smb://server/share"));

  backend.requestListing(3, url);

  QVERIFY(backend.m_job != nullptr);
  QVERIFY2(backend.m_job->uiDelegate() != nullptr,
           "supported smb job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(backend.m_job->uiDelegateExtension() != nullptr,
           "supported smb job lost its KIO UI delegate extension");
}

void TestKioNetworkDirectoryBackend::aSupportedSftpJobKeepsTheStandardKioUiDelegate() {
  DelegateProbingBackend backend;
  const QUrl url(QStringLiteral("sftp://server/home"));

  backend.requestListing(4, url);

  QVERIFY(backend.m_job != nullptr);
  QVERIFY2(backend.m_job->uiDelegate() != nullptr,
           "supported sftp job lost its KIO UI delegate (credential prompt path)");
  QVERIFY2(backend.m_job->uiDelegateExtension() != nullptr,
           "supported sftp job lost its KIO UI delegate extension");
}

QTEST_GUILESS_MAIN(TestKioNetworkDirectoryBackend)
#include "tst_kio_network_directory_backend.moc"
