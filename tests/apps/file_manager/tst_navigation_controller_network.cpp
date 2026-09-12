// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"

#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Apps::FileManager::Test::FakeDirectoryLister;
using QindaQt::Apps::FileManager::Test::FakeFileLauncher;
using QindaQt::Apps::FileManager::Test::FakeNetworkDirectoryBackend;

namespace {

[[nodiscard]] DirectoryEntry makeEntry(const QString &name, const QString &absolutePath,
                                       bool isDirectory) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = absolutePath;
  entry.isDirectory = isDirectory;
  return entry;
}

[[nodiscard]] NetworkListingResult successResult(const QUrl &url,
                                                 QVector<DirectoryEntry> entries,
                                                 bool truncated = false) {
  NetworkListingResult result;
  result.url = url;
  result.entries = std::move(entries);
  result.truncated = truncated;
  return result;
}

[[nodiscard]] NetworkListingResult errorResult(const QUrl &url, NetworkListingError error) {
  NetworkListingResult result;
  result.url = url;
  result.error = error;
  result.diagnostic = QStringLiteral("synthetic failure");
  return result;
}

} // namespace

// Failure-before, fake-injected coverage for NavigationController's S5
// network routing: every scenario uses FakeNetworkDirectoryBackend and
// synthetic smb/sftp URLs. No DNS, socket, SMB/SFTP server, or credential
// prompt is ever reachable from this file.
class TestNavigationControllerNetwork final : public QObject {
  Q_OBJECT

private slots:
  void localNavigationIsUnaffectedByAMissingNetworkBackend();
  void navigatingToASmbUrlDispatchesThroughTheBackend();
  void navigatingToAMalformedNetworkUrlDoesNothing();
  void aMissingBackendReportsUnavailableInstead();
  void asyncSuccessPublishesEntriesAndClearsLoading();
  void typedAsyncErrorsMapToDistinctStatuses();
  void aStaleGenerationCallbackIsDiscarded();
  void aUrlMismatchCallbackIsDiscarded();
  void truncatedRemoteListingsAreReported();
  void navigatingBackToALocalPathClearsRemoteActiveAndCancels();
  void activatingARemoteDirectoryNavigatesToItsChildUrl();
  void activatingARemoteFileIsTruthfullyDisabled();
  void destructionWithAPendingRequestDoesNotCrash();
};

void TestNavigationControllerNetwork::localNavigationIsUnaffectedByAMissingNetworkBackend() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QStringLiteral("/home/jarrod");
  lister->setResult(QStringLiteral("/home/jarrod"), ready);
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());

  controller.navigateTo(QStringLiteral("/home/jarrod"));
  QCOMPARE(controller.statusKey(), QStringLiteral("empty"));
  QCOMPARE(controller.remoteActive(), false);
}

void TestNavigationControllerNetwork::navigatingToASmbUrlDispatchesThroughTheBackend() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  controller.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(rawBackend->requests().size(), 1);
  QCOMPARE(rawBackend->requests().first().url.toString(), QStringLiteral("smb://server/share"));
  QCOMPARE(controller.currentPath(), QStringLiteral("smb://server/share"));
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
  QCOMPARE(controller.remoteActive(), true);
  QCOMPARE(controller.entryCount(), 0);
}

void TestNavigationControllerNetwork::navigatingToAMalformedNetworkUrlDoesNothing() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  controller.navigateTo(QStringLiteral("smb:///share")); // no host
  QVERIFY(rawBackend->requests().isEmpty());
  QVERIFY(controller.currentPath().isEmpty());
  QCOMPARE(controller.remoteActive(), false);
}

void TestNavigationControllerNetwork::aMissingBackendReportsUnavailableInstead() {
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>());

  controller.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(controller.statusKey(), QStringLiteral("unavailable"));
  QCOMPARE(controller.remoteActive(), true);
  QVERIFY(!controller.statusMessage().isEmpty());
}

void TestNavigationControllerNetwork::asyncSuccessPublishesEntriesAndClearsLoading() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  const QUrl url(QStringLiteral("smb://server/share"));
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;

  rawBackend->emitReady(generation, url,
                        successResult(url, {makeEntry(QStringLiteral("Reports"),
                                                      QStringLiteral("smb://server/share/Reports"),
                                                      true),
                                            makeEntry(QStringLiteral("notes.txt"),
                                                      QStringLiteral("smb://server/share/notes.txt"),
                                                      false)}));

  QCOMPARE(controller.statusKey(), QStringLiteral("ready"));
  QCOMPARE(controller.entryCount(), 2);
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("Reports"));
}

void TestNavigationControllerNetwork::typedAsyncErrorsMapToDistinctStatuses() {
  const struct {
    NetworkListingError error;
    const char *statusKey;
  } cases[] = {
      {NetworkListingError::Unavailable, "unavailable"},
      {NetworkListingError::AuthenticationRequired, "authentication-required"},
      {NetworkListingError::PermissionDenied, "permission-denied"},
      {NetworkListingError::NotFound, "missing"},
      {NetworkListingError::Transport, "transport-error"},
      {NetworkListingError::Unknown, "error"},
  };
  for (const auto &testCase : cases) {
    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    auto *rawBackend = backend.get();
    NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                    std::make_unique<FakeFileLauncher>(), std::move(backend));
    const QUrl url(QStringLiteral("sftp://server/home"));
    controller.navigateTo(url.toString());
    const quint64 generation = rawBackend->requests().last().generation;
    rawBackend->emitReady(generation, url, errorResult(url, testCase.error));
    QCOMPARE(controller.statusKey(), QLatin1String(testCase.statusKey));
    QCOMPARE(controller.statusMessage(), QStringLiteral("synthetic failure"));
  }
}

void TestNavigationControllerNetwork::aStaleGenerationCallbackIsDiscarded() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  const QUrl url(QStringLiteral("smb://server/share"));
  controller.navigateTo(url.toString());
  const quint64 firstGeneration = rawBackend->requests().last().generation;
  controller.refresh(); // bumps the generation again without changing the URL
  const quint64 secondGeneration = rawBackend->requests().last().generation;
  QVERIFY(secondGeneration != firstGeneration);

  rawBackend->emitReady(firstGeneration, url,
                        successResult(url, {makeEntry(QStringLiteral("stale"),
                                                      QStringLiteral("smb://server/share/stale"),
                                                      false)}));
  // The stale generation's entries never apply; the current request is
  // still pending (Loading).
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
  QCOMPARE(controller.entryCount(), 0);

  rawBackend->emitReady(secondGeneration, url, successResult(url, {}));
  QCOMPARE(controller.statusKey(), QStringLiteral("empty"));
}

void TestNavigationControllerNetwork::aUrlMismatchCallbackIsDiscarded() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  const QUrl url(QStringLiteral("smb://server/share"));
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  const QUrl otherUrl(QStringLiteral("smb://server/other"));

  rawBackend->emitReady(generation, otherUrl, successResult(otherUrl, {}));
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
}

void TestNavigationControllerNetwork::truncatedRemoteListingsAreReported() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  const QUrl url(QStringLiteral("smb://server/share"));
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  rawBackend->emitReady(
      generation, url,
      successResult(url,
                    {makeEntry(QStringLiteral("a"), QStringLiteral("smb://server/share/a"), false)},
                    /*truncated=*/true));

  QCOMPARE(controller.statusKey(), QStringLiteral("ready"));
  QVERIFY(controller.statusMessage().contains(QStringLiteral("first")));
}

void TestNavigationControllerNetwork::
    navigatingBackToALocalPathClearsRemoteActiveAndCancels() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QStringLiteral("/home/jarrod");
  lister->setResult(QStringLiteral("/home/jarrod"), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend));

  controller.navigateTo(QStringLiteral("smb://server/share"));
  const quint64 generation = rawBackend->requests().last().generation;
  QVERIFY(controller.remoteActive());

  controller.navigateTo(QStringLiteral("/home/jarrod"));
  QCOMPARE(controller.remoteActive(), false);
  QCOMPARE(controller.statusKey(), QStringLiteral("empty"));
  QVERIFY(rawBackend->cancelled().contains(generation));
}

void TestNavigationControllerNetwork::activatingARemoteDirectoryNavigatesToItsChildUrl() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend));

  const QUrl url(QStringLiteral("smb://server/share"));
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  const QString childUrl = QStringLiteral("smb://server/share/Reports");
  rawBackend->emitReady(generation, url,
                        successResult(url, {makeEntry(QStringLiteral("Reports"), childUrl, true)}));

  controller.activate(0);
  QCOMPARE(rawBackend->requests().size(), 2);
  QCOMPARE(rawBackend->requests().last().url.toString(), childUrl);
  QCOMPARE(controller.currentPath(), childUrl);
  QVERIFY(controller.remoteActive());
}

void TestNavigationControllerNetwork::activatingARemoteFileIsTruthfullyDisabled() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto launcher = std::make_unique<FakeFileLauncher>();
  auto *rawLauncher = launcher.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(), std::move(launcher),
                                  std::move(backend));

  const QUrl url(QStringLiteral("smb://server/share"));
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  rawBackend->emitReady(
      generation, url,
      successResult(url, {makeEntry(QStringLiteral("notes.txt"),
                                    QStringLiteral("smb://server/share/notes.txt"), false)}));

  QVERIFY(controller.launchError().isEmpty());
  controller.activate(0);
  QVERIFY(!controller.launchError().isEmpty());
  QVERIFY(rawLauncher->requestedPaths().isEmpty());
}

void TestNavigationControllerNetwork::destructionWithAPendingRequestDoesNotCrash() {
  {
    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                    std::make_unique<FakeFileLauncher>(), std::move(backend));
    controller.navigateTo(QStringLiteral("smb://server/share"));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestNavigationControllerNetwork)
#include "tst_navigation_controller_network.moc"
