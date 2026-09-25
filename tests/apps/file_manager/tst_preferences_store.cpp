// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/preferences_store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <utility>

using namespace QindaQt::Apps::FileManager;

namespace {

// Writes the ADR-0198 document, which load() migrates while no
// preferences-v2 exists (ADR-0270).
[[nodiscard]] bool writeRaw(const QString &directory, const QByteArray &bytes,
                            const QString &name = QStringLiteral("preferences-v1.json")) {
  QDir().mkpath(directory);
  QFile file(QDir(directory).filePath(name));
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

[[nodiscard]] FolderView galleryView() {
  FolderView view;
  view.viewMode = QStringLiteral("gallery");
  view.sortColumn = QStringLiteral("modified");
  view.sortDirection = QStringLiteral("descending");
  view.groupBy = QStringLiteral("date");
  view.iconSize = 96;
  view.columns = {{QStringLiteral("name"), 0}, {QStringLiteral("dimensions"), 120},
                  {QStringLiteral("size"), 0}};
  return view;
}

[[nodiscard]] QByteArray documentWith(const QString &key, const QString &jsonValue) {
  QStringList fields = {
      QStringLiteral("\"defaultViewMode\":\"grid\""),
      QStringLiteral("\"showHidden\":false"),
      QStringLiteral("\"directoriesFirst\":true"),
      QStringLiteral("\"sortColumn\":\"name\""),
      QStringLiteral("\"sortDirection\":\"ascending\""),
      QStringLiteral("\"iconSize\":64"),
      QStringLiteral("\"discoverNearbyServers\":false"),
      QStringLiteral("\"defaultConnectScheme\":\"sftp\""),
      QStringLiteral("\"confirmTrash\":true")};
  for (QString &field : fields) {
    if (field.startsWith(QStringLiteral("\"%1\":").arg(key))) {
      field = QStringLiteral("\"%1\":%2").arg(key, jsonValue);
    }
  }
  return QStringLiteral("{\"version\":1,\"preferences\":{%1}}")
      .arg(fields.join(QLatin1Char(',')))
      .toUtf8();
}

} // namespace

class TestPreferencesStore final : public QObject {
  Q_OBJECT

private slots:
  void firstRunIsAbsentWithTheDocumentedDefaults();
  void roundTripsEveryPreference();
  void refusesAnUnknownOrMissingKey();
  void refusesAValueOutsideItsSet_data();
  void refusesAValueOutsideItsSet();
  void refusesAWrongType();
  void defaultsMatchTheApplicationsOwn();
  // ADR-0270: preferences-v2.
  void migratesAVersion1DocumentAndWritesVersion2();
  void aRefusedVersion2DocumentNeverFallsBackToVersion1();
  void roundTripsRememberedFolderViews();
  void refusesUnboundedOrInconsistentViews();
  void rememberingKeepsTheMostRecentAndForgetsTheDefaults();
};

void TestPreferencesStore::firstRunIsAbsentWithTheDocumentedDefaults() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const PreferencesStore store(temporary.filePath(QStringLiteral("state")));
  const auto loaded = store.load();
  QVERIFY(!loaded.ok());
  QCOMPARE(loaded.error, PreferencesError::Absent);
  QVERIFY(loaded.diagnostic.isEmpty());
  QCOMPARE(loaded.preferences, Preferences{});
}

void TestPreferencesStore::roundTripsEveryPreference() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const PreferencesStore store(temporary.filePath(QStringLiteral("state")));
  Preferences written;
  written.defaultViewMode = QStringLiteral("list");
  written.showHidden = true;
  written.directoriesFirst = false;
  written.sortColumn = QStringLiteral("modified");
  written.sortDirection = QStringLiteral("descending");
  written.iconSize = 128;
  written.discoverNearbyServers = true;
  written.defaultConnectScheme = QStringLiteral("smb");
  written.confirmTrash = false;
  written.groupBy = QStringLiteral("kind");
  written.detailsColumns = {{QStringLiteral("name"), 0}, {QStringLiteral("owner"), 90},
                            {QStringLiteral("created"), 0}};
  written.relativeDates = true;
  written.rowDensity = QStringLiteral("compact");
  written.showExtensions = false;
  written.fileManagerStyle = QStringLiteral("commander");
  written.layoutStyle = QStringLiteral("explorer");
  QVERIFY(store.store(written).ok());

  const auto loaded = store.load();
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QVERIFY(!loaded.migrated);
  QCOMPARE(loaded.preferences, written);

  QFile file(store.filePath());
  QVERIFY(store.filePath().endsWith(QStringLiteral("preferences-v2.json")));
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
  QCOMPARE(object.value(QStringLiteral("version")).toInt(), 2);
  QCOMPARE(object.value(QStringLiteral("preferences")).toObject().size(), 17);
}

void TestPreferencesStore::refusesAnUnknownOrMissingKey() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const PreferencesStore store(directory);

  QVERIFY(writeRaw(directory, QByteArray("not json")));
  QCOMPARE(store.load().error, PreferencesError::Malformed);

  QVERIFY(writeRaw(directory, QByteArray("{\"version\":2,\"preferences\":{}}")));
  QCOMPARE(store.load().error, PreferencesError::Malformed);

  // AGENT-CONTRACT: a document from a newer schema is refused whole, so a v2
  // preference can never be silently dropped on the next write.
  QByteArray extended = documentWith(QStringLiteral("iconSize"), QStringLiteral("64"));
  extended.replace("\"confirmTrash\":true",
                   "\"confirmTrash\":true,\"rememberTabs\":true");
  QVERIFY(writeRaw(directory, extended));
  QCOMPARE(store.load().error, PreferencesError::Malformed);

  QByteArray missing = documentWith(QStringLiteral("iconSize"), QStringLiteral("64"));
  missing.replace(",\"confirmTrash\":true", "");
  QVERIFY(writeRaw(directory, missing));
  QCOMPARE(store.load().error, PreferencesError::Malformed);
}

void TestPreferencesStore::refusesAValueOutsideItsSet_data() {
  QTest::addColumn<QString>("key");
  QTest::addColumn<QString>("jsonValue");
  // ADR-0270: "columns" and "gallery" are views now; this one is not.
  QTest::newRow("view mode") << QStringLiteral("defaultViewMode")
                             << QStringLiteral("\"carousel\"");
  QTest::newRow("sort column") << QStringLiteral("sortColumn")
                               << QStringLiteral("\"owner\"");
  QTest::newRow("sort direction") << QStringLiteral("sortDirection")
                                  << QStringLiteral("\"sideways\"");
  // AGENT-GUARD: an icon size off the zoom ladder is refused, not snapped: a
  // view at a size the zoom controls cannot return to is a trap.
  QTest::newRow("icon size") << QStringLiteral("iconSize") << QStringLiteral("57");
  QTest::newRow("scheme") << QStringLiteral("defaultConnectScheme")
                          << QStringLiteral("\"ftp\"");
}

void TestPreferencesStore::refusesAValueOutsideItsSet() {
  QFETCH(QString, key);
  QFETCH(QString, jsonValue);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const PreferencesStore store(directory);
  QVERIFY(writeRaw(directory, documentWith(key, jsonValue)));
  const auto loaded = store.load();
  QCOMPARE(loaded.error, PreferencesError::Malformed);
  // The whole document is refused, so the defaults stand rather than a
  // half-understood mixture.
  QCOMPARE(loaded.preferences, Preferences{});

  Preferences invalid;
  invalid.sortColumn = QStringLiteral("owner");
  QCOMPARE(store.store(invalid).error, PreferencesError::Malformed);
}

void TestPreferencesStore::refusesAWrongType() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const PreferencesStore store(directory);
  QVERIFY(writeRaw(directory,
                   documentWith(QStringLiteral("showHidden"), QStringLiteral("\"yes\""))));
  QCOMPARE(store.load().error, PreferencesError::Malformed);
  QVERIFY(writeRaw(directory,
                   documentWith(QStringLiteral("iconSize"), QStringLiteral("\"64\""))));
  QCOMPARE(store.load().error, PreferencesError::Malformed);
}

void TestPreferencesStore::defaultsMatchTheApplicationsOwn() {
  // AGENT-GUARD: a first run must behave exactly as the application did
  // before preferences existed -- NavigationController's own grid view,
  // hidden files off, folders first, name ascending, 64 px icons.
  const Preferences defaults;
  QCOMPARE(defaults.defaultViewMode, QStringLiteral("grid"));
  QCOMPARE(defaults.showHidden, false);
  QCOMPARE(defaults.directoriesFirst, true);
  QCOMPARE(defaults.sortColumn, QStringLiteral("name"));
  QCOMPARE(defaults.sortDirection, QStringLiteral("ascending"));
  QCOMPARE(defaults.iconSize, 64);
  QCOMPARE(defaults.discoverNearbyServers, false);
  QCOMPARE(defaults.confirmTrash, true);
  // ADR-0270: no grouping, the four Details columns it always had, absolute
  // dates, comfortable rows, whole names, and no folder of its own.
  QCOMPARE(defaults.groupBy, QStringLiteral("none"));
  QCOMPARE(defaults.detailsColumns.size(), 4);
  QCOMPARE(defaults.detailsColumns.constFirst().key, QStringLiteral("name"));
  QCOMPARE(defaults.relativeDates, false);
  QCOMPARE(defaults.rowDensity, QStringLiteral("comfortable"));
  QCOMPARE(defaults.showExtensions, true);
  QVERIFY(defaults.folderViews.isEmpty());
  QVERIFY(defaults.isValid());
}

void TestPreferencesStore::migratesAVersion1DocumentAndWritesVersion2() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const PreferencesStore store(directory);
  QByteArray version1 = documentWith(QStringLiteral("defaultViewMode"), QStringLiteral("\"list\""));
  version1.replace("\"iconSize\":64", "\"iconSize\":128");
  QVERIFY(writeRaw(directory, version1));

  const auto migrated = store.load();
  QVERIFY2(migrated.ok(), qPrintable(migrated.diagnostic));
  QVERIFY(migrated.migrated);
  QCOMPARE(migrated.preferences.defaultViewMode, QStringLiteral("list"));
  QCOMPARE(migrated.preferences.iconSize, 128);
  // Everything v1 did not have starts at its default.
  QCOMPARE(migrated.preferences.groupBy, QStringLiteral("none"));
  QCOMPARE(migrated.preferences.detailsColumns, FolderView::defaultColumns());
  QVERIFY(migrated.preferences.folderViews.isEmpty());

  // The next write is v2; the v1 document stays for an older build.
  QVERIFY(store.store(migrated.preferences).ok());
  QVERIFY(QFile::exists(QDir(directory).filePath(QStringLiteral("preferences-v2.json"))));
  QVERIFY(QFile::exists(QDir(directory).filePath(QStringLiteral("preferences-v1.json"))));
  const auto reloaded = store.load();
  QVERIFY2(reloaded.ok(), qPrintable(reloaded.diagnostic));
  QVERIFY(!reloaded.migrated);
  QCOMPARE(reloaded.preferences, migrated.preferences);
}

void TestPreferencesStore::aRefusedVersion2DocumentNeverFallsBackToVersion1() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const PreferencesStore store(directory);
  QVERIFY(writeRaw(directory, documentWith(QStringLiteral("showHidden"), QStringLiteral("true"))));
  QVERIFY(writeRaw(directory, QByteArray("{\"version\":2,\"preferences\":{}}"),
                   QStringLiteral("preferences-v2.json")));
  // AGENT-GUARD: falling back would silently resurrect the old settings.
  const auto loaded = store.load();
  QCOMPARE(loaded.error, PreferencesError::Malformed);
  QCOMPARE(loaded.preferences, Preferences{});
}

void TestPreferencesStore::roundTripsRememberedFolderViews() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const PreferencesStore store(temporary.filePath(QStringLiteral("state")));
  Preferences written;
  written.rememberFolderView(QStringLiteral("/home/user/Pictures"), galleryView());
  FolderView applications;
  applications.viewMode = QStringLiteral("list");
  written.rememberFolderView(QStringLiteral("applications:"), applications);
  written.rememberFolderView(QStringLiteral("smb://server/share"), galleryView());
  QCOMPARE(written.folderViews.size(), 3);
  QVERIFY(store.store(written).ok());

  const auto loaded = store.load();
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QCOMPARE(loaded.preferences, written);
  QCOMPARE(loaded.preferences.folderViewFor(QStringLiteral("/home/user/Pictures")), galleryView());
  QCOMPARE(loaded.preferences.folderViewFor(QStringLiteral("/elsewhere")),
           loaded.preferences.defaultFolderView());
  QVERIFY(loaded.preferences.remembers(QStringLiteral("applications:")));
}

void TestPreferencesStore::refusesUnboundedOrInconsistentViews() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const PreferencesStore store(temporary.filePath(QStringLiteral("state")));
  const auto refused = [&store](const Preferences &preferences) {
    return store.store(preferences).error == PreferencesError::Malformed;
  };

  Preferences tooMany;
  for (int i = 0; i <= Preferences::maximumFolderViews; ++i) {
    tooMany.folderViews.append({QStringLiteral("/folder/%1").arg(i), galleryView()});
  }
  QVERIFY(refused(tooMany));

  Preferences duplicate;
  duplicate.folderViews = {{QStringLiteral("/same"), galleryView()},
                           {QStringLiteral("/same"), galleryView()}};
  QVERIFY(refused(duplicate));

  const auto withColumns = [](QList<DetailsColumn> columns) {
    Preferences preferences;
    preferences.detailsColumns = std::move(columns);
    return preferences;
  };
  // Name leads, every key once, known keys only, and a width is 0 or 40-1000.
  QVERIFY(refused(withColumns({{QStringLiteral("size"), 0}, {QStringLiteral("name"), 0}})));
  QVERIFY(refused(withColumns({{QStringLiteral("name"), 0}, {QStringLiteral("size"), 0},
                               {QStringLiteral("size"), 0}})));
  QVERIFY(refused(withColumns({{QStringLiteral("name"), 0}, {QStringLiteral("colour"), 0}})));
  QVERIFY(refused(withColumns({{QStringLiteral("name"), 0}, {QStringLiteral("size"), 20}})));
  QVERIFY(refused(withColumns({})));

  Preferences badDensity;
  badDensity.rowDensity = QStringLiteral("roomy");
  QVERIFY(refused(badDensity));
  Preferences badGroup;
  badGroup.groupBy = QStringLiteral("colour");
  QVERIFY(refused(badGroup));
  // ADR-0271: a style is one of the three, or empty to match the layout; the
  // layout's applied style is always one of the three.
  Preferences badStyle;
  badStyle.fileManagerStyle = QStringLiteral("norton");
  QVERIFY(refused(badStyle));
  Preferences badLayoutStyle;
  badLayoutStyle.layoutStyle = QString();
  QVERIFY(refused(badLayoutStyle));
}

void TestPreferencesStore::rememberingKeepsTheMostRecentAndForgetsTheDefaults() {
  Preferences preferences;
  for (int i = 0; i < Preferences::maximumFolderViews + 5; ++i) {
    preferences.rememberFolderView(QStringLiteral("/folder/%1").arg(i), galleryView());
  }
  // The oldest are forgotten first; the newest leads.
  QCOMPARE(preferences.folderViews.size(), Preferences::maximumFolderViews);
  QCOMPARE(preferences.folderViews.constFirst().location,
           QStringLiteral("/folder/%1").arg(Preferences::maximumFolderViews + 4));
  QVERIFY(!preferences.remembers(QStringLiteral("/folder/0")));

  // Remembering again moves a folder to the front.
  preferences.rememberFolderView(QStringLiteral("/folder/10"), galleryView());
  QCOMPARE(preferences.folderViews.constFirst().location, QStringLiteral("/folder/10"));

  // A view equal to the defaults is forgotten, so the folder follows them.
  preferences.rememberFolderView(QStringLiteral("/folder/10"), preferences.defaultFolderView());
  QVERIFY(!preferences.remembers(QStringLiteral("/folder/10")));
  QVERIFY(preferences.isValid());
}

QTEST_APPLESS_MAIN(TestPreferencesStore)
#include "tst_preferences_store.moc"
