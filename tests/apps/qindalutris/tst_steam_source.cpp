// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTest>

#include "steam_source.h"

using namespace QindaQt::QindaLutris;

// Fixture-backed Steam discovery rows (ADR-0231): synthetic roots and
// manifests only -- nothing here is or contains real game content.
class tst_steam_source : public QObject {
  Q_OBJECT

  static void writeFile(const QString &path, const QByteArray &bytes) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
  }

  static QByteArray manifest(const char *name) {
    return QByteArray("\"AppState\"\n{\n\t\"appid\"\t\t\"0\"\n\t\"name\"\t\t\"")
           + name + QByteArray("\"\n}\n");
  }

private Q_SLOTS:
  // Two roots declared by one libraryfolders.vdf contribute both libraries.
  void twoRoots() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString install = home.path() + QStringLiteral("/steam");
    writeFile(install + QStringLiteral("/steamapps/libraryfolders.vdf"),
              "\"libraryfolders\"\n{\n"
              "\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + install.toUtf8() + "\"\n\t}\n"
              "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" + home.path().toUtf8()
                  + "/second\"\n\t}\n}\n");
    writeFile(install + QStringLiteral("/steamapps/appmanifest_10.acf"),
              manifest("Root Game"));
    QVERIFY(QDir().mkpath(home.path() + QStringLiteral("/second/steamapps")));
    writeFile(home.path() + QStringLiteral("/second/steamapps/appmanifest_20.acf"),
              manifest("Second Library Game"));

    const SteamDiscovery out = scanSteamLibraries({install});
    QCOMPARE(out.games.size(), 2);
    QCOMPARE(out.games.at(0).title, QStringLiteral("Root Game"));
    QCOMPARE(out.games.at(0).id, QStringLiteral("steam/10"));
    QCOMPARE(out.games.at(0).source, GameSource::Steam);
    QCOMPARE(out.games.at(1).title, QStringLiteral("Second Library Game"));
    QCOMPARE(out.games.at(1).appId, quint64(20));
  }

  // Steam absent (no candidate exists) is an empty result with no warnings.
  void absentSteam() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const SteamDiscovery out =
        scanSteamLibraries({home.path() + QStringLiteral("/nope")});
    QVERIFY(out.games.isEmpty());
    QVERIFY(out.warnings.isEmpty());
  }

  // A malformed manifest is skipped with a warning; the good one survives.
  void malformedManifest() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString install = home.path() + QStringLiteral("/steam");
    writeFile(install + QStringLiteral("/steamapps/appmanifest_1.acf"),
              manifest("Good Game"));
    writeFile(install + QStringLiteral("/steamapps/appmanifest_2.acf"),
              "\"AppState\"\n{\n\t\"name\" \"unterminated\n");
    writeFile(install + QStringLiteral("/steamapps/appmanifest_notanumber.acf"),
              manifest("Ghost"));

    const SteamDiscovery out = scanSteamLibraries({install});
    QCOMPARE(out.games.size(), 1);
    QCOMPARE(out.games.at(0).title, QStringLiteral("Good Game"));
    QVERIFY(!out.warnings.isEmpty());
  }

  // A malformed libraryfolders.vdf does not hide a present steamapps tree.
  void malformedIndexFallsBackToCandidate() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString install = home.path() + QStringLiteral("/steam");
    writeFile(install + QStringLiteral("/steamapps/libraryfolders.vdf"),
              "not vdf at all {{{");
    writeFile(install + QStringLiteral("/steamapps/appmanifest_7.acf"),
              manifest("Fallback Game"));
    const SteamDiscovery out = scanSteamLibraries({install});
    QCOMPARE(out.games.size(), 1);
    QCOMPARE(out.games.at(0).title, QStringLiteral("Fallback Game"));
    QVERIFY(!out.warnings.isEmpty());
  }

  // Cover art comes from the install root's appcache by fixed name only.
  void coverLookup() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString install = home.path() + QStringLiteral("/steam");
    writeFile(install + QStringLiteral("/steamapps/appmanifest_5.acf"),
              manifest("Covered Game"));
    writeFile(install + QStringLiteral("/appcache/librarycache/5/library_600x900.jpg"),
              QByteArray("\xff\xd8\xff", 3)); // not a real JPEG; only the path matters
    const SteamDiscovery out = scanSteamLibraries({install});
    QCOMPARE(out.games.size(), 1);
    QVERIFY(out.games.at(0).coverPath.endsWith(
        QStringLiteral("library_600x900.jpg")));
  }

  // Proton discovery finds steamapps/common/Proton* executables only.
  void protonDiscovery() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString root = home.path();
    const QString script = root + QStringLiteral("/steamapps/common/Proton 9.0/proton");
    writeFile(script, "#!/bin/sh\n");
    QFile::setPermissions(script, QFile::permissions(script) | QFile::ExeUser);
    writeFile(root + QStringLiteral("/steamapps/common/NotProton/proton"),
              "#!/bin/sh\n");
    const QVector<ProtonInstall> protons = discoverProtonInstalls({root});
    QCOMPARE(protons.size(), 1);
    QCOMPARE(protons.at(0).name, QStringLiteral("Proton 9.0"));
  }
};

QTEST_GUILESS_MAIN(tst_steam_source)
#include "tst_steam_source.moc"
