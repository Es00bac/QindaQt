// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "network_downloader.h"

using namespace QindaQt::QindaLutris;

// The production downloader's decisions (DownloadGuard) and its refusal path.
// No row reaches the network: every NetworkDownloader case is refused before
// a request is made.
class tst_download_guard : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void startRefusesNonAllowlisted() {
    DownloadGuard guard;
    QVERIFY(guard.checkStart(QUrl(QStringLiteral("https://github.com/x"))) == std::nullopt);
    QVERIFY(guard.checkStart(QUrl(QStringLiteral("http://github.com/x"))).has_value());
    QVERIFY(guard.checkStart(QUrl(QStringLiteral("https://evil.example/x"))).has_value());
  }

  void redirectToAllowlistedHostIsFollowed() {
    DownloadGuard guard;
    QVERIFY(guard.checkRedirect(QUrl(QStringLiteral(
                "https://epicgames-download1.akamaized.net/Builds/x.msi"))) == std::nullopt);
  }

  void redirectOffTheAllowlistIsRefused() {
    DownloadGuard guard;
    const auto refused = guard.checkRedirect(QUrl(QStringLiteral("https://cdn.evil.example/x.exe")));
    QVERIFY(refused.has_value());
    QVERIFY(refused->contains(QStringLiteral("cdn.evil.example")));
  }

  void redirectDowngradeToHttpIsRefused() {
    DownloadGuard guard;
    QVERIFY(guard.checkRedirect(QUrl(QStringLiteral("http://downloader.battle.net/x"))).has_value());
  }

  void redirectLoopIsBounded() {
    DownloadLimits limits;
    limits.maxRedirects = 2;
    DownloadGuard guard(limits);
    const QUrl hop(QStringLiteral("https://downloader.battle.net/x"));
    QVERIFY(guard.checkRedirect(hop) == std::nullopt);
    QVERIFY(guard.checkRedirect(hop) == std::nullopt);
    QVERIFY(guard.checkRedirect(hop).has_value());
    guard.reset();
    QVERIFY(guard.checkRedirect(hop) == std::nullopt);
  }

  void httpErrorIsRefused() {
    DownloadGuard guard;
    const auto refused = guard.checkHeaders(404, -1);
    QVERIFY(refused.has_value());
    QVERIFY(refused->contains(QStringLiteral("404")));
    QVERIFY(guard.checkHeaders(500, 10).has_value());
    QVERIFY(guard.checkHeaders(200, 10) == std::nullopt);
  }

  void announcedOversizeIsRefusedUpFront() {
    DownloadLimits limits;
    limits.maxBytes = 100;
    DownloadGuard guard(limits);
    QVERIFY(guard.checkHeaders(200, 101).has_value());
  }

  void growthPastTheCapIsRefused() {
    DownloadLimits limits;
    limits.maxBytes = 100;
    DownloadGuard guard(limits);
    QVERIFY(guard.checkHeaders(200, -1) == std::nullopt);
    QVERIFY(guard.checkProgress(100) == std::nullopt);
    QVERIFY(guard.checkProgress(101).has_value());
  }

  void truncatedTransferIsRefused() {
    DownloadGuard guard;
    QVERIFY(guard.checkHeaders(200, 1000) == std::nullopt);
    const auto refused = guard.checkCompletion(400);
    QVERIFY(refused.has_value());
    QVERIFY(refused->contains(QStringLiteral("cut short")));
    QVERIFY(guard.checkCompletion(1000) == std::nullopt);
  }

  void overrunOfAnnouncedLengthIsRefused() {
    DownloadGuard guard;
    QVERIFY(guard.checkHeaders(200, 10) == std::nullopt);
    QVERIFY(guard.checkProgress(11).has_value());
  }

  void emptyBodyIsRefused() {
    DownloadGuard guard;
    QVERIFY(guard.checkHeaders(200, -1) == std::nullopt);
    QVERIFY(guard.checkCompletion(0).has_value());
    QVERIFY(guard.checkCompletion(5) == std::nullopt);
  }

  void networkDownloaderRefusesOffListUrlsAsynchronously() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/dl-XXXXXX"));
    QVERIFY(dir.isValid());
    const QString destination = dir.filePath(QStringLiteral("x.exe"));
    NetworkDownloader downloader;
    QSignalSpy finished(&downloader, &Downloader::finished);
    downloader.start(QUrl(QStringLiteral("http://downloader.battle.net/x.exe")), destination);
    QCOMPARE(finished.count(), 0); // never synchronous
    QVERIFY(finished.wait(2000));
    QCOMPARE(finished.first().at(0).toBool(), false);
    QVERIFY(!finished.first().at(1).toString().isEmpty());
    QVERIFY(!QFile::exists(destination));
    QVERIFY(!QFile::exists(destination + QStringLiteral(".part")));
  }

  void networkDownloaderCancelSuppressesTheQueuedResult() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/dl-XXXXXX"));
    NetworkDownloader downloader;
    QSignalSpy finished(&downloader, &Downloader::finished);
    downloader.start(QUrl(QStringLiteral("https://evil.example/x.exe")),
                     dir.filePath(QStringLiteral("x.exe")));
    downloader.cancel();
    QTest::qWait(50);
    QCOMPARE(finished.count(), 0);
  }
};

QTEST_GUILESS_MAIN(tst_download_guard)
#include "tst_download_guard.moc"
