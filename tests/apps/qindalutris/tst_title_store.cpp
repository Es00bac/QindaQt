// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "title_store.h"

using namespace QindaQt::QindaLutris;

namespace {

TitleRecord battleNet() {
  TitleRecord record;
  record.id = QStringLiteral("title/battle-net");
  record.title = QStringLiteral("Battle.net");
  record.kind = TitleKind::StoreLauncher;
  record.store = GameStore::BattleNet;
  record.prefixPath = QStringLiteral("/home/u/Games/battlenet");
  record.protonBuild = QStringLiteral("GE-Proton11-6-x86_64");
  record.protonBuildVersion = QStringLiteral("1756415527 GE-Proton11-6");
  record.umuId = QStringLiteral("umu-battlenet");
  record.umuStore = QStringLiteral("battlenet");
  record.executable = QStringLiteral(
      "/home/u/Games/battlenet/drive_c/Program Files (x86)/Battle.net/Battle.net.exe");
  record.environment = {QStringLiteral("DXVK_ASYNC=1")};
  record.winetricksApplied = {QStringLiteral("arial"),
                              QStringLiteral("vcrun2019")};
  record.installedAt = QStringLiteral("2026-09-25");
  return record;
}

TitleRecord worldOfWarcraft() {
  TitleRecord record = battleNet();
  record.id = QStringLiteral("title/world-of-warcraft");
  record.title = QStringLiteral("World of Warcraft");
  record.kind = TitleKind::StoreGame;
  record.storeGameId = QStringLiteral("wow");
  record.arguments = {QStringLiteral("--exec=launch WoW")};
  record.environment.clear();
  record.winetricksApplied.clear();
  record.launcherTitleId = QStringLiteral("title/battle-net");
  return record;
}

} // namespace

// titles-v1.json (ADR-0275 section 6): exact schema, whole refusal, atomic.
class tst_title_store : public QObject {
  Q_OBJECT

  static void writeRaw(const QString &path, const QByteArray &bytes) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
  }

  // A valid two-record document as JSON, for surgical corruption.
  static QJsonObject validDocument(const QString &root) {
    const TitleStore store(root);
    if (store.writeTitles({battleNet(), worldOfWarcraft()})
        != TitleStore::Error::None) {
      return {};
    }
    QFile file(store.titlesPath());
    if (!file.open(QIODevice::ReadOnly)) {
      return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
  }

  // Writes `mutated` and asserts the whole document is refused.
  static void expectRefused(const QString &root, const QJsonObject &mutated) {
    const TitleStore store(root);
    writeRaw(store.titlesPath(), QJsonDocument(mutated).toJson());
    TitleStore::Error error = TitleStore::Error::None;
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::Refused);
  }

  static QJsonObject withRecordField(QJsonObject document, int index,
                                     const QString &key, const QJsonValue &value) {
    QJsonArray titles = document.value(QStringLiteral("titles")).toArray();
    QJsonObject record = titles.at(index).toObject();
    if (value.isUndefined()) {
      record.remove(key);
    } else {
      record.insert(key, value);
    }
    titles.replace(index, record);
    document.insert(QStringLiteral("titles"), titles);
    return document;
  }

private Q_SLOTS:
  void absentIsFirstRun() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TitleStore store(root.path());
    TitleStore::Error error = TitleStore::Error::None;
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::Absent);
    QCOMPARE(store.titlesPath(), root.path() + QStringLiteral("/titles-v1.json"));
  }

  void roundTrip() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TitleStore store(root.path() + QStringLiteral("/nested/config"));
    const QVector<TitleRecord> records{battleNet(), worldOfWarcraft()};
    QCOMPARE(store.writeTitles(records), TitleStore::Error::None);
    TitleStore::Error error = TitleStore::Error::Refused;
    QCOMPARE(store.readTitles(&error), records);
    QCOMPARE(error, TitleStore::Error::None);
    // An empty list is a valid document too.
    QCOMPARE(store.writeTitles({}), TitleStore::Error::None);
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::None);
  }

  void unknownKeysAreRefusedWhole() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QJsonObject valid = validDocument(root.path());
    QVERIFY(!valid.isEmpty());
    expectRefused(root.path(),
                  withRecordField(valid, 1, QStringLiteral("proton"),
                                  QStringLiteral("GE-Proton")));
    QJsonObject rootKey = valid;
    rootKey.insert(QStringLiteral("extra"), true);
    expectRefused(root.path(), rootKey);
    // A missing key is the same failure.
    expectRefused(root.path(), withRecordField(valid, 0, QStringLiteral("umuId"),
                                               QJsonValue::Undefined));
  }

  void outOfSetEnumsAreRefusedNotSnapped() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QJsonObject valid = validDocument(root.path());
    expectRefused(root.path(), withRecordField(valid, 0, QStringLiteral("kind"),
                                               QStringLiteral("launcher")));
    expectRefused(root.path(), withRecordField(valid, 0, QStringLiteral("store"),
                                               QStringLiteral("origin")));
    expectRefused(root.path(), withRecordField(valid, 0, QStringLiteral("store"),
                                               QStringLiteral("Steam")));
  }

  void emptyOrAliasProtonBuildIsRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QJsonObject valid = validDocument(root.path());
    for (const QString &bad :
         {QString(), QStringLiteral("GE-Proton"), QStringLiteral("GE-Latest"),
          QStringLiteral("UMU-Latest"), QStringLiteral("latest"),
          QStringLiteral("/abs/GE-Proton11-6"), QStringLiteral("../x")}) {
      expectRefused(root.path(), withRecordField(valid, 1,
                                                 QStringLiteral("protonBuild"),
                                                 bad));
    }
    // Identity is name + version: a missing version is refused too.
    expectRefused(root.path(), withRecordField(valid, 1,
                                               QStringLiteral("protonBuildVersion"),
                                               QString()));
    expectRefused(root.path(), withRecordField(valid, 1,
                                               QStringLiteral("protonBuildVersion"),
                                               QJsonValue::Undefined));
    // The writer refuses the same record, so it can never be persisted.
    TitleRecord floating = worldOfWarcraft();
    floating.protonBuild = QStringLiteral("GE-Proton");
    QCOMPARE(TitleStore(root.path()).writeTitles({floating}),
             TitleStore::Error::WriteFailed);
    floating.protonBuild.clear();
    QCOMPARE(TitleStore(root.path()).writeTitles({floating}),
             TitleStore::Error::WriteFailed);
    TitleRecord unversioned = worldOfWarcraft();
    unversioned.protonBuildVersion.clear();
    QCOMPARE(TitleStore(root.path()).writeTitles({unversioned}),
             TitleStore::Error::WriteFailed);
  }

  void invalidEnvironmentIsRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QJsonObject valid = validDocument(root.path());
    expectRefused(root.path(),
                  withRecordField(valid, 0, QStringLiteral("environment"),
                                  QJsonArray{QStringLiteral("NOT VALID")}));
    expectRefused(root.path(),
                  withRecordField(valid, 0, QStringLiteral("environment"),
                                  QJsonArray{QStringLiteral("1BAD=x")}));
    // A title may never set a key the umu plan owns.
    expectRefused(root.path(),
                  withRecordField(valid, 0, QStringLiteral("environment"),
                                  QJsonArray{QStringLiteral("PROTONPATH=GE-Proton")}));
    // Review R5b: nor any key that lets umu bypass the pinned build.
    for (const char *bypass : {"UMU_NO_PROTON=1", "RUNTIMEPATH=steamrt3",
                               "PROTON_VERB=run", "UMU_RUNTIME_UPDATE=1",
                               // umu-run's Python interpreter and loader.
                               "PYTHONWARNINGS=ignore::evil.x",
                               "PYTHONPATH=/tmp", "PYTHONSTARTUP=/x.py",
                               "LD_PRELOAD=/x.so", "LD_LIBRARY_PATH=/x",
                               "LD_AUDIT=/x.so"}) {
      expectRefused(root.path(),
                    withRecordField(valid, 0, QStringLiteral("environment"),
                                    QJsonArray{QString::fromLatin1(bypass)}));
    }
    expectRefused(root.path(),
                  withRecordField(valid, 0, QStringLiteral("environment"),
                                  QStringLiteral("DXVK_ASYNC=1")));
  }

  void otherFieldViolationsAreRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QJsonObject valid = validDocument(root.path());
    const auto refuse = [&](const QString &key, const QJsonValue &value) {
      expectRefused(root.path(), withRecordField(valid, 1, key, value));
    };
    refuse(QStringLiteral("id"), QStringLiteral("wine/world-of-warcraft"));
    refuse(QStringLiteral("id"), QStringLiteral("title/Upper"));
    refuse(QStringLiteral("id"), QStringLiteral("title/battle-net")); // duplicate
    refuse(QStringLiteral("title"), QString());
    refuse(QStringLiteral("prefixPath"), QStringLiteral("Games/battlenet"));
    refuse(QStringLiteral("executable"), QStringLiteral("C:\\Games\\wow.exe"));
    refuse(QStringLiteral("umuId"), QStringLiteral("umu 1"));
    refuse(QStringLiteral("arguments"), QJsonArray{QStringLiteral("a\nb")});
    refuse(QStringLiteral("arguments"), 3);
    refuse(QStringLiteral("launcherTitleId"), QStringLiteral("title/world-of-warcraft"));
    refuse(QStringLiteral("winetricksApplied"), QJsonArray{QStringLiteral("rm -rf")});
    refuse(QStringLiteral("winetricksApplied"), QJsonArray{QStringLiteral("-q")});
    refuse(QStringLiteral("umuId"), QStringLiteral("-umu"));
    refuse(QStringLiteral("umuStore"), QStringLiteral("-none"));
    refuse(QStringLiteral("installedAt"), QStringLiteral("2026-13-40"));
    refuse(QStringLiteral("installedAt"), QStringLiteral("yesterday"));
  }

  void newerSchemaIsRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QJsonObject newer = validDocument(root.path());
    newer.insert(QStringLiteral("version"), 2);
    expectRefused(root.path(), newer);
    newer.insert(QStringLiteral("version"), QStringLiteral("1"));
    expectRefused(root.path(), newer);
  }

  void oversizedDocumentIsRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TitleStore store(root.path());
    QByteArray huge("{\"version\":1,\"titles\":[],\"pad\":\"");
    huge.append(QByteArray(int(kMaxStoreBytes), 'x'));
    huge.append("\"}");
    writeRaw(store.titlesPath(), huge);
    TitleStore::Error error = TitleStore::Error::None;
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::Refused);
    // Too many records is refused on both sides.
    QVector<TitleRecord> many;
    for (int i = 0; i <= kMaxTitles; ++i) {
      TitleRecord record = battleNet();
      record.id = QStringLiteral("title/game-%1").arg(i);
      many.append(record);
    }
    QCOMPARE(store.writeTitles(many), TitleStore::Error::WriteFailed);
  }

  void symlinkIsRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TitleStore real(root.path() + QStringLiteral("/real"));
    QCOMPARE(real.writeTitles({battleNet()}), TitleStore::Error::None);
    const TitleStore store(root.path());
    QVERIFY(QFile::link(real.titlesPath(), store.titlesPath()));
    TitleStore::Error error = TitleStore::Error::None;
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::Refused);
  }

  // Review R4: a symlinked destination is never written through.
  void writeRefusesASymlinkedDestination() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString victim = root.filePath(QStringLiteral("victim.txt"));
    writeRaw(victim, "precious\n");
    const TitleStore store(root.filePath(QStringLiteral("config")));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("config"))));
    QVERIFY(QFile::link(victim, store.titlesPath()));
    QCOMPARE(store.writeTitles({battleNet()}), TitleStore::Error::WriteFailed);
    QFile check(victim);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("precious\n"));
    QVERIFY(QFileInfo(store.titlesPath()).isSymLink());
    // A dangling link is refused on read (it is not "absent").
    QVERIFY(QFile::remove(victim));
    TitleStore::Error error = TitleStore::Error::None;
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::Refused);
    QCOMPARE(store.writeTitles({battleNet()}), TitleStore::Error::WriteFailed);
    QVERIFY(!QFileInfo::exists(victim));
  }

  void reservedKeysMatchByPrefixAndCase() {
    QVERIFY(isReservedUmuEnvironmentKey(QStringLiteral("PYTHONEXECUTABLE")));
    QVERIFY(isReservedUmuEnvironmentKey(QStringLiteral("PYTHON")));
    QVERIFY(!isReservedUmuEnvironmentKey(QStringLiteral("python_path")));
    QVERIFY(!isReservedUmuEnvironmentKey(QStringLiteral("MYPYTHON")));
    QVERIFY(!isReservedUmuEnvironmentKey(QStringLiteral("LD_BIND_NOW")));
    QVERIFY(!isReservedUmuEnvironmentKey(QStringLiteral("DXVK_ASYNC")));
  }

  void garbageIsRefused() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TitleStore store(root.path());
    writeRaw(store.titlesPath(), "not json {");
    TitleStore::Error error = TitleStore::Error::None;
    QVERIFY(store.readTitles(&error).isEmpty());
    QCOMPARE(error, TitleStore::Error::Refused);
  }

  void titleIdsAreStableAndUnique() {
    QCOMPARE(makeTitleId(QStringLiteral("World of Warcraft"), {}),
             QStringLiteral("title/world-of-warcraft"));
    QCOMPARE(makeTitleId(QStringLiteral("World of Warcraft"),
                         {QStringLiteral("title/world-of-warcraft")}),
             QStringLiteral("title/world-of-warcraft-2"));
    QCOMPARE(makeTitleId(QStringLiteral("Battle.net"), {}),
             QStringLiteral("title/battle-net"));
    QVERIFY(isValidTitleId(makeTitleId(QStringLiteral("日本語"), {})));
    QVERIFY(isValidTitleId(makeTitleId(QString(), {})));
  }
};

QTEST_GUILESS_MAIN(tst_title_store)
#include "tst_title_store.moc"
