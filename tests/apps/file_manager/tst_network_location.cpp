// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/network_location.h"

#include <QTest>

using namespace QindaQt::Apps::FileManager;

class TestNetworkLocation final : public QObject {
  Q_OBJECT

private slots:
  void classifiesSupportedSchemesCaseInsensitively();
  void classifiesEverythingElseAsLocal();
  void canonicalizesHostAndCollapsesDotSegments();
  void refusesAMissingHost();
  void refusesEmbeddedCredentials();
  void refusesADotDotEscape();
  void isSupportedSchemeMatchesCanonicalUrlsOnly();
  void parentOfStepsUpOneSegmentAndStopsAtTheRoot();
  void breadcrumbForListsRootFirstCumulativeSegments();
  void childUrlAppendsOnePlainSegment();
  void boundedDiagnosticTruncates();
};

void TestNetworkLocation::classifiesSupportedSchemesCaseInsensitively() {
  QCOMPARE(NetworkLocation::classify(QStringLiteral("smb://server/share")),
           LocationScheme::Smb);
  QCOMPARE(NetworkLocation::classify(QStringLiteral("SFTP://server/home")),
           LocationScheme::Sftp);
  QCOMPARE(NetworkLocation::classify(QStringLiteral("Smb://Server")),
           LocationScheme::Smb);
}

void TestNetworkLocation::classifiesEverythingElseAsLocal() {
  QCOMPARE(NetworkLocation::classify(QStringLiteral("/home/jarrod/Desktop")),
           LocationScheme::Local);
  QCOMPARE(NetworkLocation::classify(QStringLiteral("relative/path")),
           LocationScheme::Local);
  QCOMPARE(NetworkLocation::classify(QStringLiteral("file:///home/jarrod")),
           LocationScheme::Local);
  QCOMPARE(NetworkLocation::classify(QStringLiteral("http://example.com")),
           LocationScheme::Local);
  QCOMPARE(NetworkLocation::classify(QString()), LocationScheme::Local);
}

void TestNetworkLocation::canonicalizesHostAndCollapsesDotSegments() {
  const auto url = NetworkLocation::canonicalize(
      QStringLiteral("SMB://Server:445/share/./sub//leaf"));
  QVERIFY(url.has_value());
  QCOMPARE(url->scheme(), QStringLiteral("smb"));
  QCOMPARE(url->host(), QStringLiteral("server"));
  QCOMPARE(url->port(), 445);
  QCOMPARE(url->path(), QStringLiteral("/share/sub/leaf"));

  // Two spellings of the same folder canonicalize identically.
  const auto other = NetworkLocation::canonicalize(
      QStringLiteral("smb://server:445/share/sub/leaf/"));
  QVERIFY(other.has_value());
  QCOMPARE(url->toString(), other->toString());
}

void TestNetworkLocation::refusesAMissingHost() {
  QVERIFY(!NetworkLocation::canonicalize(QStringLiteral("smb:///share")).has_value());
}

void TestNetworkLocation::refusesEmbeddedCredentials() {
  QVERIFY(!NetworkLocation::canonicalize(
               QStringLiteral("smb://user:pass@server/share"))
               .has_value());
  QVERIFY(!NetworkLocation::canonicalize(QStringLiteral("sftp://user@server/home"))
               .has_value());
}

void TestNetworkLocation::refusesADotDotEscape() {
  QVERIFY(!NetworkLocation::canonicalize(QStringLiteral("smb://server/share/../etc"))
               .has_value());
  QVERIFY(!NetworkLocation::canonicalize(QStringLiteral("smb://server/.."))
               .has_value());
}

void TestNetworkLocation::isSupportedSchemeMatchesCanonicalUrlsOnly() {
  const auto url = NetworkLocation::canonicalize(QStringLiteral("sftp://server/home"));
  QVERIFY(url.has_value());
  QVERIFY(NetworkLocation::isSupportedScheme(*url));
  QVERIFY(!NetworkLocation::isSupportedScheme(QUrl(QStringLiteral("http://server/home"))));
  QVERIFY(!NetworkLocation::isSupportedScheme(QUrl(QStringLiteral("/home/jarrod"))));
}

void TestNetworkLocation::parentOfStepsUpOneSegmentAndStopsAtTheRoot() {
  const auto leaf = NetworkLocation::canonicalize(QStringLiteral("smb://server/share/sub"));
  QVERIFY(leaf.has_value());
  const auto share = NetworkLocation::parentOf(*leaf);
  QVERIFY(share.has_value());
  QCOMPARE(share->toString(), QStringLiteral("smb://server/share"));
  const auto root = NetworkLocation::parentOf(*share);
  QVERIFY(root.has_value());
  QCOMPARE(root->toString(), QStringLiteral("smb://server"));
  QVERIFY(!NetworkLocation::parentOf(*root).has_value());
}

void TestNetworkLocation::breadcrumbForListsRootFirstCumulativeSegments() {
  const auto url = NetworkLocation::canonicalize(QStringLiteral("smb://server/share/sub"));
  QVERIFY(url.has_value());
  const auto segments = NetworkLocation::breadcrumbFor(*url);
  QCOMPARE(segments.size(), 3);
  QCOMPARE(segments.at(0).name, QStringLiteral("smb://server"));
  QCOMPARE(segments.at(0).url.toString(), QStringLiteral("smb://server"));
  QCOMPARE(segments.at(1).name, QStringLiteral("share"));
  QCOMPARE(segments.at(1).url.toString(), QStringLiteral("smb://server/share"));
  QCOMPARE(segments.at(2).name, QStringLiteral("sub"));
  QCOMPARE(segments.at(2).url.toString(), QStringLiteral("smb://server/share/sub"));
}

void TestNetworkLocation::childUrlAppendsOnePlainSegment() {
  const auto parent = NetworkLocation::canonicalize(QStringLiteral("smb://server/share"));
  QVERIFY(parent.has_value());
  const QUrl child = NetworkLocation::childUrl(*parent, QStringLiteral("Report.docx"));
  QCOMPARE(child.toString(), QStringLiteral("smb://server/share/Report.docx"));
}

void TestNetworkLocation::boundedDiagnosticTruncates() {
  const QString long_diagnostic(NetworkLocation::maximumDiagnosticLength + 100, QLatin1Char('x'));
  QCOMPARE(NetworkLocation::boundedDiagnostic(long_diagnostic).size(),
           static_cast<int>(NetworkLocation::maximumDiagnosticLength));
  QCOMPARE(NetworkLocation::boundedDiagnostic(QStringLiteral("short")),
           QStringLiteral("short"));
}

QTEST_GUILESS_MAIN(TestNetworkLocation)
#include "tst_network_location.moc"
