// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/lock_preferences/legacy_lock_import.h"
#include "qindaqt/services/lock_preferences/lock_preferences.h"
#include "qindaqt/services/settings_service/settings_repository.h"
#include "qindaqt/settings/settings_document.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Services::LockPreferences;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;
class LockPreferencesImportTests final : public QObject {
  Q_OBJECT
private slots:
  void importsOnlyFourDocumentedKeysAndRetainsExplicitChoices();
  void rejectsMalformedOrUnsupportedValues_data();
  void rejectsMalformedOrUnsupportedValues();
  void readsBoundedUtf8SnapshotsWithoutWritingLegacy();
  void failedAtomicCommitLeavesValuesAndMarkerRetryable();
  void typedSchemaAndDecoderEnforceBounds();
};
void LockPreferencesImportTests::
    importsOnlyFourDocumentedKeysAndRetainsExplicitChoices() {
  const QVariantMap legacy{{"Autolock", false},
                           {"Timeout", 7},
                           {"LockOnResume", false},
                           {"LockGrace", 9},
                           {"RequirePassword", false}};
  const auto plan = planLegacyImport(legacy, {{"lock.automaticEnabled", true}});
  QVERIFY(plan.sourceSupported);
  QCOMPARE(plan.values.size(), 4);
  QVERIFY(!plan.values.contains("lock.automaticEnabled"));
  QCOMPARE(plan.values.value("lock.idleTimeoutSeconds").toLongLong(), 420);
  QCOMPARE(plan.values.value("lock.onResume").toBool(), false);
  QCOMPARE(plan.values.value("lock.graceSeconds").toLongLong(), 9);
  QCOMPARE(plan.values.value("lock.migration.kscreenlockerImported").toBool(),
           true);
  QVERIFY(!planLegacyImport(legacy,
                            {{"lock.migration.kscreenlockerImported", true}})
               .sourceSupported);
  QVERIFY(!planLegacyImport({{"RequirePassword", false}}, {}).sourceSupported);
  const auto allExplicit =
      planLegacyImport(legacy, {{"lock.automaticEnabled", true},
                                {"lock.idleTimeoutSeconds", 300},
                                {"lock.onResume", true},
                                {"lock.graceSeconds", 5}});
  QCOMPARE(allExplicit.values.size(), 1);
}
void LockPreferencesImportTests::rejectsMalformedOrUnsupportedValues_data() {
  QTest::addColumn<QString>("key");
  QTest::addColumn<QVariant>("value");
  QTest::newRow("bad-bool") << QString("Autolock") << QVariant("perhaps");
  QTest::newRow("resume-bypass")
      << QString("LockOnResume") << QVariant("never-password");
  QTest::newRow("timeout-zero") << QString("Timeout") << QVariant(0);
  QTest::newRow("timeout-too-large") << QString("Timeout") << QVariant(241);
  QTest::newRow("timeout-fraction") << QString("Timeout") << QVariant("1.5");
  QTest::newRow("grace-negative") << QString("LockGrace") << QVariant(-1);
  QTest::newRow("grace-too-large") << QString("LockGrace") << QVariant(301);
  QTest::newRow("grace-fraction") << QString("LockGrace") << QVariant("0.5");
}
void LockPreferencesImportTests::rejectsMalformedOrUnsupportedValues() {
  QFETCH(QString, key);
  QFETCH(QVariant, value);
  const auto plan = planLegacyImport({{"Autolock", true}, {key, value}}, {});
  QVERIFY(!plan.sourceSupported);
  QVERIFY(plan.values.isEmpty());
}
void LockPreferencesImportTests::
    readsBoundedUtf8SnapshotsWithoutWritingLegacy() {
  QTemporaryDir directory;
  const auto path = directory.filePath("kscreenlockerrc");
  QFile file(path);
  const QByteArray supported(
      "[Daemon]\nAutolock=false\nTimeout=7\nLockOnResume=true\nLockGrace="
      "9\nRequirePassword=false\n[Other]\nAutolock=true\n");
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(supported), supported.size());
  file.close();
  QVariantMap values;
  QVERIFY(readLegacyPreferences(path, &values));
  QCOMPARE(values.size(), 4);
  QCOMPARE(values.value("Autolock").toString(), QString("false"));
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), supported);
  file.close();
  const QList<QByteArray> invalid{QByteArray("[Daemon\nTimeout=7\n"),
                                  QByteArray("[Daemon]\ninvalid\n"),
                                  QByteArray("[Daemon]\nAutolock=") +
                                      char(0xff),
                                  QByteArray("[Daemon]\nAutolock=") + char(0),
                                  QByteArray(1024 * 1024 + 1, 'x'),
                                  QByteArray("[Other]\nTimeout=7\n"),
                                  QByteArray()};
  for (const auto &bytes : invalid) {
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(bytes), bytes.size());
    file.close();
    QVariantMap unchanged{{"sentinel", true}};
    QVERIFY(!readLegacyPreferences(path, &unchanged));
    QCOMPARE(unchanged, QVariantMap({{"sentinel", true}}));
  }
  QVERIFY(!readLegacyPreferences(directory.filePath("missing"), &values));
  QVERIFY(!readLegacyPreferences(path, nullptr));
}
void LockPreferencesImportTests::
    failedAtomicCommitLeavesValuesAndMarkerRetryable() {
  QString error;
  const auto schema = SettingsSchema::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
      nullptr, &error);
  QVERIFY2(schema, qPrintable(error));
  const auto plan = planLegacyImport({{"Autolock", false}, {"Timeout", 7}}, {});
  QVector<SettingsRepository::Operation> operations;
  for (auto it = plan.values.cbegin(); it != plan.values.cend(); ++it)
    operations.push_back({it.key(), false, it.value()});
  QTemporaryDir directory;
  SettingsRepository failed(LayeredSettings(*schema), directory.path(),
                            "failed");
  QVERIFY(!failed.commitUserOverrides(0, operations).ok());
  const QStringList keys{"lock.automaticEnabled", "lock.idleTimeoutSeconds",
                         "lock.migration.kscreenlockerImported"};
  QCOMPARE(failed.snapshot(keys).values.value("lock.automaticEnabled").toBool(),
           true);
  QCOMPARE(failed.snapshot(keys)
               .values.value("lock.idleTimeoutSeconds")
               .toLongLong(),
           300);
  QCOMPARE(failed.snapshot(keys)
               .values.value("lock.migration.kscreenlockerImported")
               .toBool(),
           false);
  const auto storage = directory.filePath("native.json");
  SettingsRepository retry(LayeredSettings(*schema), storage, "retry");
  QVERIFY(retry.commitUserOverrides(0, operations).ok());
  const auto persisted = SettingsFileStore::load(storage, *schema);
  QVERIFY(persisted.ok);
  QCOMPARE(persisted.document.values, plan.values);
}
void LockPreferencesImportTests::typedSchemaAndDecoderEnforceBounds() {
  QString error;
  const auto schema = SettingsSchema::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
      nullptr, &error);
  QVERIFY2(schema, qPrintable(error));
  SettingDomain domain = SettingDomain::Power;
  QVERIFY(parseSettingDomain(QStringLiteral("lock"), &domain));
  QVERIFY(domain == SettingDomain::Lock);
  QCOMPARE(static_cast<int>(domain), 11);
  QCOMPARE(domainKeyPrefix(domain), QStringLiteral("lock"));
  QVERIFY(schema->definition("lock.automaticEnabled")->domain == SettingDomain::Lock);
  Preferences output;
  auto values = schema->systemDefaults();
  QVERIFY(decodePreferences(values, &output));
  QCOMPARE(output, Preferences{});
  for (const auto seconds : {qint64(60), qint64(14400)}) {
    values.insert("lock.idleTimeoutSeconds", seconds);
    QVERIFY(decodePreferences(values, &output));
    QCOMPARE(output.idleTimeoutSeconds, seconds);
  }
  const auto previous = output;
  for (const auto &invalid :
       {QVariant(qint64(59)), QVariant(qint64(14401)), QVariant("300"),
        QVariant(300.0), QVariant(true)}) {
    values.insert("lock.idleTimeoutSeconds", invalid);
    QVERIFY(!decodePreferences(values, &output));
    QCOMPARE(output, previous);
  }
  values = schema->systemDefaults();
  values.insert("lock.graceSeconds", 301);
  QVERIFY(!decodePreferences(values, &output));
  QVERIFY(!schema->validateValue("lock.graceSeconds", 301).isValid());
  QVERIFY(!schema->validateValue("lock.idleTimeoutSeconds", 59).isValid());
  values.remove("lock.onResume");
  QVERIFY(!decodePreferences(values, &output));
  QVERIFY(!decodePreferences(schema->systemDefaults(), nullptr));
}
QTEST_GUILESS_MAIN(LockPreferencesImportTests)
#include "tst_lock_preferences_import.moc"
