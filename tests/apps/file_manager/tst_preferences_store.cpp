// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/preferences_store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool writeRaw(const QString &directory, const QByteArray &bytes) {
  QDir().mkpath(directory);
  QFile file(QDir(directory).filePath(QStringLiteral("preferences-v1.json")));
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
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
  QVERIFY(store.store(written).ok());

  const auto loaded = store.load();
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QCOMPARE(loaded.preferences, written);

  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
  QCOMPARE(object.value(QStringLiteral("version")).toInt(), 1);
  QCOMPARE(object.value(QStringLiteral("preferences")).toObject().size(), 9);
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
  QTest::newRow("view mode") << QStringLiteral("defaultViewMode")
                             << QStringLiteral("\"columns\"");
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
  QVERIFY(defaults.isValid());
}

QTEST_APPLESS_MAIN(TestPreferencesStore)
#include "tst_preferences_store.moc"
