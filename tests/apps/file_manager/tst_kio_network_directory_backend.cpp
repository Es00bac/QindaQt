// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/kio_network_directory_backend.h"

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

} // namespace

class TestKioNetworkDirectoryBackend final : public QObject {
  Q_OBJECT

private slots:
  void refusesAnUnsupportedSchemeBeforeCreatingAJob();
  void refusesALocalPathBeforeCreatingAJob();
  void reachesJobCreationForACanonicalSmbUrl();
  void aNullJobBecomesATypedUnavailableResult();
  void cancelOnAnUnknownGenerationIsANoOp();
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

QTEST_GUILESS_MAIN(TestKioNetworkDirectoryBackend)
#include "tst_kio_network_directory_backend.moc"
