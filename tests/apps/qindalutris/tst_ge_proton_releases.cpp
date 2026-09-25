// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

#include "ge_proton_releases.h"

using namespace QindaQt::QindaLutris;

namespace {

const QString kBase =
    QStringLiteral("https://github.com/GloriousEggroll/proton-ge-custom/releases/download/");

QJsonObject asset(const QString &tag, const QString &name, qint64 size = 100,
                  const QString &base = kBase) {
  return QJsonObject{{QStringLiteral("name"), name},
                     {QStringLiteral("size"), double(size)},
                     {QStringLiteral("browser_download_url"), base + tag + QLatin1Char('/') + name}};
}

QJsonObject release(const QString &tag, const QJsonArray &assets, bool draft = false,
                    bool prerelease = false) {
  return QJsonObject{{QStringLiteral("tag_name"), tag},
                     {QStringLiteral("draft"), draft},
                     {QStringLiteral("prerelease"), prerelease},
                     {QStringLiteral("published_at"), QStringLiteral("2026-08-28T21:37:07Z")},
                     {QStringLiteral("assets"), assets}};
}

QJsonArray archPair(const QString &tag, const QString &arch) {
  const QString stem = tag + QLatin1Char('-') + arch;
  return {asset(tag, stem + QStringLiteral(".sha512sum"), 158),
          asset(tag, stem + QStringLiteral(".tar.gz"), 533700853)};
}

QByteArray document(const QJsonArray &releases) {
  return QJsonDocument(releases).toJson(QJsonDocument::Compact);
}

QString hex128(char c) { return QString(128, QLatin1Char(c)); }

} // namespace

class tst_ge_proton_releases : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void apiUrlIsTheGloriousEggrollReleaseList() {
    const QUrl url = geProtonReleasesApiUrl(5);
    QCOMPARE(url.host(), QStringLiteral("api.github.com"));
    QCOMPARE(url.path(), QStringLiteral("/repos/GloriousEggroll/proton-ge-custom/releases"));
    QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("per_page")), QStringLiteral("5"));
    QCOMPARE(QUrlQuery(geProtonReleasesApiUrl(5000)).queryItemValue(QStringLiteral("per_page")),
             QStringLiteral("100"));
  }

  void parsesArchitectureSpecificAssets() {
    QJsonArray assets = archPair(QStringLiteral("GE-Proton11-6"), QStringLiteral("aarch64"));
    for (const QJsonValue &value : archPair(QStringLiteral("GE-Proton11-6"), QStringLiteral("x86_64"))) {
      assets.append(value);
    }
    const auto parsed = parseGeProtonReleases(document({release(QStringLiteral("GE-Proton11-6"), assets)}));
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.releases.size(), 1);
    const GeProtonRelease &r = parsed.releases.first();
    QCOMPARE(r.tagName, QStringLiteral("GE-Proton11-6"));
    QCOMPARE(r.toolName, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(r.tarballName, QStringLiteral("GE-Proton11-6-x86_64.tar.gz"));
    QCOMPARE(r.checksumName, QStringLiteral("GE-Proton11-6-x86_64.sha512sum"));
    QCOMPARE(r.tarballBytes, qint64(533700853));
    QVERIFY(r.tarballUrl.toString().endsWith(QStringLiteral("/GE-Proton11-6/GE-Proton11-6-x86_64.tar.gz")));
    QVERIFY(r.publishedAt.isValid());
    QVERIFY(!r.prerelease);

    const auto arm = parseGeProtonReleases(
        document({release(QStringLiteral("GE-Proton11-6"), assets)}), QStringLiteral("aarch64"));
    QCOMPARE(arm.releases.first().toolName, QStringLiteral("GE-Proton11-6-aarch64"));
  }

  void acceptsPreArchitectureNamingForX86Only() {
    const QString tag = QStringLiteral("GE-Proton9-20");
    const QJsonArray assets{asset(tag, tag + QStringLiteral(".tar.gz")),
                            asset(tag, tag + QStringLiteral(".sha512sum"))};
    const auto parsed = parseGeProtonReleases(document({release(tag, assets)}));
    QCOMPARE(parsed.releases.size(), 1);
    QCOMPARE(parsed.releases.first().toolName, tag);
    QVERIFY(parseGeProtonReleases(document({release(tag, assets)}), QStringLiteral("aarch64"))
                .releases.isEmpty());
  }

  void skipsWhatCannotBeVerifiedOrTrusted() {
    const QString tag = QStringLiteral("GE-Proton11-5");
    const QJsonArray noChecksum{asset(tag, tag + QStringLiteral("-x86_64.tar.gz"))};
    const QJsonArray offList{
        asset(tag, tag + QStringLiteral("-x86_64.tar.gz"), 1, QStringLiteral("https://mirror.evil.example/")),
        asset(tag, tag + QStringLiteral("-x86_64.sha512sum"))};
    const QJsonArray releases{
        release(tag, noChecksum),
        release(QStringLiteral("GE-Proton11-4"), offList),
        release(QStringLiteral("GE-Proton11-8"), archPair(QStringLiteral("GE-Proton11-8"), QStringLiteral("x86_64")), true),
        release(QStringLiteral("Proton-Evil"), archPair(QStringLiteral("Proton-Evil"), QStringLiteral("x86_64"))),
        release(QStringLiteral("GE-Proton../x"), {}),
        release(QStringLiteral("GE-Proton11-7"), archPair(QStringLiteral("GE-Proton11-7"), QStringLiteral("x86_64")), false, true),
    };
    const auto parsed = parseGeProtonReleases(document(releases));
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.releases.size(), 1);
    QCOMPARE(parsed.releases.first().tagName, QStringLiteral("GE-Proton11-7"));
    QVERIFY(parsed.releases.first().prerelease);
  }

  void refusesMalformedDocumentsWhole() {
    QVERIFY(!parseGeProtonReleases("{\"message\":\"rate limited\"}").ok);
    QVERIFY(!parseGeProtonReleases("not json").ok);
    QVERIFY(!parseGeProtonReleases(QByteArray(kMaxReleaseDocumentBytes + 1, ' ')).ok);
    QVERIFY(parseGeProtonReleases("[]").ok);
  }

  void toolNamesAreSingleSafeDirectoryNames() {
    QVERIFY(isSafeToolName(QStringLiteral("GE-Proton11-6-x86_64")));
    QVERIFY(isSafeToolName(QStringLiteral("GE-Proton9-20")));
    QVERIFY(!isSafeToolName(QStringLiteral("..")));
    QVERIFY(!isSafeToolName(QStringLiteral("a/b")));
    QVERIFY(!isSafeToolName(QStringLiteral(".hidden")));
    QVERIFY(!isSafeToolName(QStringLiteral("-rf")));
    QVERIFY(!isSafeToolName(QString()));
  }

  void sha512SumFileFormats() {
    const QString name = QStringLiteral("GE-Proton11-6-x86_64.tar.gz");
    const QByteArray upper = hex128('A').toLatin1();
    QCOMPARE(parseSha512SumFile(upper + "  " + name.toUtf8() + "\n", name).value(),
             hex128('a').toLatin1());
    QCOMPARE(parseSha512SumFile(hex128('b').toLatin1() + " *" + name.toUtf8(), name).value(),
             hex128('b').toLatin1());
    QVERIFY(!parseSha512SumFile(hex128('b').toLatin1() + "  other.tar.gz", name).has_value());
    QVERIFY(!parseSha512SumFile(QByteArray("abc  ") + name.toUtf8(), name).has_value());
    QVERIFY(!parseSha512SumFile(hex128('g').toLatin1() + "  " + name.toUtf8(), name).has_value());
    QVERIFY(!parseSha512SumFile(QByteArray(), name).has_value());
  }

  void streamedHashMatchesOneShotHash() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/sha-XXXXXX"));
    const QString path = dir.filePath(QStringLiteral("blob"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray data(3 * 1024 * 1024 + 17, 'q');
    file.write(data);
    file.close();
    QCOMPARE(sha512HexOfFile(path).value(),
             QCryptographicHash::hash(data, QCryptographicHash::Sha512).toHex());
    QVERIFY(!sha512HexOfFile(dir.filePath(QStringLiteral("missing"))).has_value());
  }

  void listingWithOneTopFolderIsAccepted() {
    const auto verdict = validateSingleTopLevelListing(
        "GE-Proton11-6-x86_64/\nGE-Proton11-6-x86_64/proton\nGE-Proton11-6-x86_64/files/lib/x.so\n");
    QVERIFY(verdict.ok);
    QCOMPARE(verdict.topLevel, QStringLiteral("GE-Proton11-6-x86_64"));
    QVERIFY(validateSingleTopLevelListing("./\n./top/\n./top/a\n").ok);
  }

  void listingEscapesAreRefused_data() {
    QTest::addColumn<QByteArray>("listing");
    QTest::newRow("absolute") << QByteArray("top/\ntop/a\n/etc/passwd\n");
    QTest::newRow("leading dotdot") << QByteArray("top/\ntop/a\n../escape.txt\n");
    QTest::newRow("inner dotdot") << QByteArray("top/\ntop/../../escape\n");
    QTest::newRow("second top") << QByteArray("top/\ntop/a\nother/b\n");
    QTest::newRow("sibling file") << QByteArray("top/\ntop/a\nREADME\n");
    QTest::newRow("single file") << QByteArray("proton\n");
    QTest::newRow("empty") << QByteArray();
  }
  void listingEscapesAreRefused() {
    QFETCH(QByteArray, listing);
    const auto verdict = validateSingleTopLevelListing(listing);
    QVERIFY(!verdict.ok);
    QVERIFY(!verdict.reason.isEmpty());
    QVERIFY(verdict.topLevel.isEmpty());
  }

  void listingEntryCountIsBounded() {
    QByteArray listing("top/\n");
    for (int i = 0; i < 10; ++i) {
      listing += "top/f" + QByteArray::number(i) + "\n";
    }
    QVERIFY(!validateSingleTopLevelListing(listing, 5).ok);
    QVERIFY(validateSingleTopLevelListing(listing, 50).ok);
  }
};

QTEST_GUILESS_MAIN(tst_ge_proton_releases)
#include "tst_ge_proton_releases.moc"
