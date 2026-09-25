// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "download_allowlist.h"

using namespace QindaQt::QindaLutris;

// The ADR-0275 section 6 allowlist: exact https hosts only, every trick that
// makes a URL *look* like an allowed host refused.
class tst_download_allowlist : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void allowedUrls_data() {
    QTest::addColumn<QString>("url");
    QTest::newRow("battle.net") << QStringLiteral(
        "https://downloader.battle.net/download/getInstaller?os=win&installer=Battle.net-Setup.exe");
    QTest::newRow("ea") << QStringLiteral(
        "https://origin-a.akamaihd.net/EA-Desktop-Client-Download/installer-releases/EAappInstaller.exe");
    QTest::newRow("ubisoft") << QStringLiteral(
        "https://static3.cdn.ubi.com/orbit/launcher_installer/UbisoftConnectInstaller.exe");
    QTest::newRow("epic api") << QStringLiteral(
        "https://launcher-public-service-prod06.ol.epicgames.com/launcher/api/installer/download/"
        "EpicGamesLauncherInstaller.msi");
    QTest::newRow("epic cdn") << QStringLiteral(
        "https://epicgames-download1.akamaized.net/Builds/UnrealEngineLauncher/x.msi");
    QTest::newRow("gog") << QStringLiteral(
        "https://webinstallers.gog-statics.com/download/GOG_Galaxy_2.0.exe");
    QTest::newRow("amazon") << QStringLiteral("https://download.amazongames.com/AmazonGamesSetup.exe");
    QTest::newRow("github release") << QStringLiteral(
        "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/GE-Proton11-6/"
        "GE-Proton11-6-x86_64.tar.gz");
    QTest::newRow("github objects") << QStringLiteral("https://objects.githubusercontent.com/a/b");
    QTest::newRow("github release assets")
        << QStringLiteral("https://release-assets.githubusercontent.com/a/b");
    QTest::newRow("github api") << QStringLiteral(
        "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases?per_page=5");
    QTest::newRow("explicit 443") << QStringLiteral("https://github.com:443/x");
    QTest::newRow("upper-case scheme and host") << QStringLiteral("HTTPS://DOWNLOADER.BATTLE.NET/x");
    QTest::newRow("compat db placeholder")
        << QStringLiteral("https://%1/compat-db-v1.json").arg(QString::fromLatin1(kCompatDbRefreshHost));
  }
  void allowedUrls() {
    QFETCH(QString, url);
    QVERIFY2(isAllowedDownloadUrl(url), qPrintable(url));
    QVERIFY(isAllowedDownloadUrl(QUrl(url)));
  }

  void refusedUrls_data() {
    QTest::addColumn<QString>("url");
    QTest::newRow("plain http") << QStringLiteral("http://downloader.battle.net/x.exe");
    QTest::newRow("ftp") << QStringLiteral("ftp://downloader.battle.net/x.exe");
    QTest::newRow("file") << QStringLiteral("file:///etc/passwd");
    QTest::newRow("wrong host") << QStringLiteral("https://example.com/x.exe");
    QTest::newRow("lookalike suffix") << QStringLiteral("https://downloader.battle.net.evil.com/x.exe");
    QTest::newRow("lookalike prefix") << QStringLiteral("https://evildownloader.battle.net/x.exe");
    QTest::newRow("subdomain of allowed") << QStringLiteral("https://evil.github.com/x");
    QTest::newRow("parent of allowed") << QStringLiteral("https://battle.net/x.exe");
    QTest::newRow("userinfo naming allowed host")
        << QStringLiteral("https://downloader.battle.net@evil.com/x.exe");
    QTest::newRow("userinfo before allowed host")
        << QStringLiteral("https://evil.com@downloader.battle.net/x.exe");
    QTest::newRow("user and password") << QStringLiteral("https://u:p@github.com/x");
    QTest::newRow("backslash authority trick")
        << QStringLiteral("https://evil.com\\@downloader.battle.net/x.exe");
    QTest::newRow("trailing dot host") << QStringLiteral("https://github.com./x");
    QTest::newRow("other port") << QStringLiteral("https://github.com:8443/x");
    QTest::newRow("ip literal") << QStringLiteral("https://140.82.112.3/x");
    QTest::newRow("relative") << QStringLiteral("/download/x.exe");
    QTest::newRow("scheme only") << QStringLiteral("https:downloader.battle.net/x.exe");
    QTest::newRow("embedded space") << QStringLiteral("https://github.com /x");
    QTest::newRow("empty") << QString();
  }
  void refusedUrls() {
    QFETCH(QString, url);
    QVERIFY2(!isAllowedDownloadUrl(url), qPrintable(url));
  }

  void tolerantQUrlWithUserInfoIsRefused() {
    // A QUrl built in tolerant mode (as a redirect target might be) must be
    // judged by its parsed authority, not by how the text looked.
    const QUrl url(QStringLiteral("https://downloader.battle.net%40evil.com@github.com/x"));
    QVERIFY(!isAllowedDownloadUrl(url));
  }

  void tableIsExactAndLowerCase() {
    const QStringList hosts = allowedDownloadHosts();
    QCOMPARE(hosts.size(), 12);
    for (const QString &host : hosts) {
      QCOMPARE(host, host.toLower());
      QVERIFY(!host.contains(QLatin1Char('*')));
    }
    QVERIFY(hosts.contains(QString::fromLatin1(kCompatDbRefreshHost)));
  }
};

QTEST_GUILESS_MAIN(tst_download_allowlist)
#include "tst_download_allowlist.moc"
