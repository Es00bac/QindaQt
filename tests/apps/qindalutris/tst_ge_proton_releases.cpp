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

  void attackerForkAndCrossHostAssetsAreSkipped() {
    // The reviewer's case: a fork's tarball plus a checksum on another
    // allowlisted host must never become an installable release.
    const QString tag = QStringLiteral("GE-Proton99-1");
    const QJsonArray assets{
        asset(tag, tag + QStringLiteral("-x86_64.tar.gz"), 1,
              QStringLiteral("https://github.com/attacker/fork/releases/download/")),
        asset(tag, tag + QStringLiteral("-x86_64.sha512sum"), 1,
              QStringLiteral("https://download.amazongames.com/"))};
    QVERIFY(parseGeProtonReleases(document({release(tag, assets)})).releases.isEmpty());

    const QJsonArray crossHost{asset(tag, tag + QStringLiteral("-x86_64.tar.gz")),
                               asset(tag, tag + QStringLiteral("-x86_64.sha512sum"), 1,
                                     QStringLiteral("https://objects.githubusercontent.com/"))};
    QVERIFY(parseGeProtonReleases(document({release(tag, crossHost)})).releases.isEmpty());

    const QJsonArray otherTagFolder{
        asset(QStringLiteral("GE-Proton1-1"), tag + QStringLiteral("-x86_64.tar.gz")),
        asset(tag, tag + QStringLiteral("-x86_64.sha512sum"))};
    QVERIFY(parseGeProtonReleases(document({release(tag, otherTagFolder)})).releases.isEmpty());
  }

  void upstreamCheckGuardsHandBuiltReleases() {
    const QString tag = QStringLiteral("GE-Proton11-6");
    const auto parsed = parseGeProtonReleases(
        document({release(tag, archPair(tag, QStringLiteral("x86_64")))}));
    const GeProtonRelease good = parsed.releases.value(0);
    QVERIFY(isUpstreamGeProtonRelease(good));

    GeProtonRelease r = good;
    r.tarballUrl = QUrl(QStringLiteral("https://github.com/attacker/fork/releases/download/GE-Proton11-6/"
                                       "GE-Proton11-6-x86_64.tar.gz"));
    QVERIFY(!isUpstreamGeProtonRelease(r));
    r = good;
    r.checksumUrl = QUrl(QStringLiteral("https://download.amazongames.com/GE-Proton11-6-x86_64.sha512sum"));
    QVERIFY(!isUpstreamGeProtonRelease(r));
    r = good;
    r.toolName = QStringLiteral("GE-Proton11-7-x86_64");
    QVERIFY(!isUpstreamGeProtonRelease(r));
    r = good;
    r.tarballName = QStringLiteral("other.tar.gz");
    QVERIFY(!isUpstreamGeProtonRelease(r));
    r = good;
    r.tagName = QStringLiteral("Proton-11");
    QVERIFY(!isUpstreamGeProtonRelease(r));
    QCOMPARE(geProtonAssetUrl(tag, QStringLiteral("f.tar.gz")).toString(),
             QStringLiteral("https://github.com/GloriousEggroll/proton-ge-custom/releases/download/"
                            "GE-Proton11-6/f.tar.gz"));
  }
};

QTEST_GUILESS_MAIN(tst_ge_proton_releases)
#include "tst_ge_proton_releases.moc"
