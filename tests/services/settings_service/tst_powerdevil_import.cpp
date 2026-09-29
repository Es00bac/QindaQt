// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/power_policy/powerdevil_import.h"
#include "qindaqt/services/settings_service/settings_repository.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::PowerPolicy;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class PowerDevilImportTests final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void importsDocumentedValuesForEveryProfile();
  void existingNativeChoicesWinAndMarkerMakesImportIdempotent();
  void invalidLegacyFieldsStayAbsentAndSafe();
  void preservesDisabledActionsAndSubMinuteTimeouts();
  void missingAndMalformedFilesAreRetryable();
  void schemaBoundsPolicyValues();
  void repositoryCommitIsAtomicAndRetryable();

private:
  std::optional<SettingsSchema> m_schema;
};

void PowerDevilImportTests::initTestCase() {
  QString error;
  m_schema = SettingsSchema::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
      nullptr, &error);
  QVERIFY2(m_schema.has_value(), qPrintable(error));
}

void PowerDevilImportTests::importsDocumentedValuesForEveryProfile() {
  QVariantMap legacy;
  for (const QString &profile :
       {QStringLiteral("AC"), QStringLiteral("Battery"),
        QStringLiteral("LowBattery")}) {
    const QString p = profile + QLatin1Char('/');
    legacy.insert(p + QStringLiteral("SuspendAndShutdown/LidAction"), 1);
    legacy.insert(
        p + QStringLiteral("SuspendAndShutdown/"
                           "InhibitLidActionWhenExternalMonitorPresent"),
        true);
    legacy.insert(p + QStringLiteral("SuspendAndShutdown/AutoSuspendAction"),
                  2);
    legacy.insert(
        p + QStringLiteral("SuspendAndShutdown/AutoSuspendIdleTimeoutSec"),
        1800);
    legacy.insert(p + QStringLiteral("Display/DimDisplayWhenIdle"), true);
    legacy.insert(p + QStringLiteral("Display/DimDisplayIdleTimeoutSec"), 300);
    legacy.insert(p + QStringLiteral("Display/TurnOffDisplayWhenIdle"), true);
    legacy.insert(p + QStringLiteral("Display/TurnOffDisplayIdleTimeoutSec"),
                  600);
    legacy.insert(p + QStringLiteral("Display/LockBeforeTurnOffDisplay"),
                  false);
    legacy.insert(p + QStringLiteral("Performance/PowerProfile"),
                  QStringLiteral("balanced"));
  }
  legacy.insert(QStringLiteral("BatteryManagement/BatteryCriticalAction"), 8);
  legacy.insert(QStringLiteral("AC/SuspendAndShutdown/SleepMode"), 1);
  legacy.insert(QStringLiteral("Battery/SuspendAndShutdown/SleepMode"), 2);
  legacy.insert(QStringLiteral("LowBattery/SuspendAndShutdown/SleepMode"), 3);
  const ImportPlan plan = planPowerDevilImport(legacy, {});
  QVERIFY(plan.sourceSupported);
  QVariantMap imported;
  for (const auto &entry : plan.values)
    imported.insert(entry.first, entry.second);
  QCOMPARE(imported.value(QStringLiteral("power.lid.ac.action")).toString(),
           QStringLiteral("suspend"));
  QCOMPARE(imported.value(QStringLiteral("power.lid.battery.dockedAction"))
               .toString(),
           QStringLiteral("none"));
  QCOMPARE(imported.value(QStringLiteral("power.idle.lowBattery.dimSeconds"))
               .toInt(),
           300);
  QCOMPARE(
      imported.value(QStringLiteral("power.idle.ac.displayOffSeconds")).toInt(),
      600);
  QCOMPARE(imported.value(QStringLiteral("power.idle.battery.suspendAction"))
               .toString(),
           QStringLiteral("hibernate"));
  QCOMPARE(
      imported.value(QStringLiteral("power.idle.lowBattery.suspendSeconds"))
          .toInt(),
      1800);
  QCOMPARE(imported.value(QStringLiteral("power.profile.battery")).toString(),
           QStringLiteral("balanced"));
  QCOMPARE(imported.value(QStringLiteral("power.critical.action")).toString(),
           QStringLiteral("power-off"));
  QCOMPARE(imported.value(QStringLiteral("power.sleep.ac.mode")).toString(),
           QStringLiteral("suspend"));
  QCOMPARE(
      imported.value(QStringLiteral("power.sleep.battery.mode")).toString(),
      QStringLiteral("hybrid-sleep"));
  QCOMPARE(
      imported.value(QStringLiteral("power.sleep.lowBattery.mode")).toString(),
      QStringLiteral("suspend-then-hibernate"));
  QVERIFY(imported.value(QStringLiteral("power.migration.powerDevilImported"))
              .toBool());
}

void PowerDevilImportTests::
    existingNativeChoicesWinAndMarkerMakesImportIdempotent() {
  const QVariantMap legacy{
      {QStringLiteral("AC/Display/TurnOffDisplayWhenIdle"), false},
      {QStringLiteral("AC/Display/TurnOffDisplayIdleTimeoutSec"), 1200},
      {QStringLiteral("AC/SuspendAndShutdown/LidAction"), 2}};
  const QVariantMap native{
      {QStringLiteral("power.idle.ac.displayOffEnabled"), true},
      {QStringLiteral("power.idleDisplayOffMinutes"), 7},
      {QStringLiteral("power.lid.ac.action"), QStringLiteral("lock")},
      {QStringLiteral("power.sleep.ac.mode"), QStringLiteral("hybrid-sleep")}};
  const ImportPlan plan = planPowerDevilImport(legacy, native);
  QVariantMap imported;
  for (const auto &entry : plan.values)
    imported.insert(entry.first, entry.second);
  QVERIFY(
      !imported.contains(QStringLiteral("power.idle.ac.displayOffEnabled")));
  QVERIFY(!imported.contains(QStringLiteral("power.lid.ac.action")));
  QVERIFY(!imported.contains(QStringLiteral("power.sleep.ac.mode")));
  QCOMPARE(
      imported.value(QStringLiteral("power.idle.ac.displayOffSeconds")).toInt(),
      420);
  QVERIFY(!planPowerDevilImport(
               legacy,
               {{QStringLiteral("power.migration.powerDevilImported"), true}})
               .sourceSupported);
  const ImportPlan disabled = planPowerDevilImport(
      {}, {{QStringLiteral("power.idleDisplayOffMinutes"), -1}});
  QVariantMap disabledValues;
  for (const auto &entry : disabled.values)
    disabledValues.insert(entry.first, entry.second);
  QCOMPARE(
      disabledValues.value(QStringLiteral("power.idle.ac.displayOffEnabled"))
          .toBool(),
      false);
  QCOMPARE(
      disabledValues.value(QStringLiteral("power.idle.ac.displayOffSeconds"))
          .toInt(),
      0);
  const ImportPlan existingMinutes = planPowerDevilImport(
      {{QStringLiteral("AC/Display/TurnOffDisplayWhenIdle"), false}},
      {{QStringLiteral("power.idleDisplayOffMinutes"), 7}});
  int enabledKeyCount = 0;
  for (const auto &entry : existingMinutes.values) {
    if (entry.first == QStringLiteral("power.idle.ac.displayOffEnabled")) {
      ++enabledKeyCount;
      QCOMPARE(entry.second.toBool(), true);
    }
  }
  QCOMPARE(enabledKeyCount, 1);
  const ImportPlan nativeProfileWins = planPowerDevilImport(
      {}, {{QStringLiteral("power.idleDisplayOffMinutes"), -1},
           {QStringLiteral("power.idle.ac.displayOffEnabled"), true}});
  for (const auto &entry : nativeProfileWins.values)
    QVERIFY(entry.first != QStringLiteral("power.idle.ac.displayOffSeconds"));
}

void PowerDevilImportTests::preservesDisabledActionsAndSubMinuteTimeouts() {
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("powerdevilrc"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("[BatteryManagement]\nBatteryCriticalAction=0\n"
             "[AC][Display]\nDimDisplayIdleTimeoutSec=30\n"
             "[Battery][SuspendAndShutdown]\nLidAction=8\n"
             "AutoSuspendIdleTimeoutSec=901\nSleepMode=3\n");
  file.close();
  QVariantMap legacy;
  QVERIFY(readPowerDevilPreferences(path, &legacy));
  const auto plan = planPowerDevilImport(legacy, {});
  QVariantMap imported;
  for (const auto &entry : plan.values)
    imported.insert(entry.first, entry.second);
  QCOMPARE(imported.value(QStringLiteral("power.critical.action")).toString(),
           QStringLiteral("none"));
  QCOMPARE(
      imported.value(QStringLiteral("power.lid.battery.action")).toString(),
      QStringLiteral("power-off"));
  QCOMPARE(
      imported.value(QStringLiteral("power.sleep.battery.mode")).toString(),
      QStringLiteral("suspend-then-hibernate"));
  QCOMPARE(imported.value(QStringLiteral("power.idle.ac.dimSeconds")).toInt(),
           30);
  QCOMPARE(imported.value(QStringLiteral("power.idle.battery.suspendSeconds"))
               .toInt(),
           901);
  for (const auto &entry : plan.values)
    QVERIFY(m_schema->validateValue(entry.first, entry.second).isValid());
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(QByteArray(1024 * 1024 + 1, 'x'));
  file.close();
  QVariantMap unchanged{{QStringLiteral("sentinel"), true}};
  QVERIFY(!readPowerDevilPreferences(path, &unchanged));
  QCOMPARE(unchanged.size(), 1);
}

void PowerDevilImportTests::invalidLegacyFieldsStayAbsentAndSafe() {
  const QVariantMap legacy{
      {QStringLiteral("AC/SuspendAndShutdown/LidAction"), 128},
      {QStringLiteral("AC/Display/TurnOffDisplayWhenIdle"),
       QStringLiteral("perhaps")},
      {QStringLiteral("AC/Display/TurnOffDisplayIdleTimeoutSec"), 14401},
      {QStringLiteral("AC/SuspendAndShutdown/AutoSuspendAction"), 8},
      {QStringLiteral("BatteryManagement/BatteryCriticalAction"), 128}};
  const ImportPlan plan = planPowerDevilImport(legacy, {});
  QVariantMap imported;
  for (const auto &entry : plan.values)
    imported.insert(entry.first, entry.second);
  QVERIFY(!imported.contains(QStringLiteral("power.lid.ac.action")));
  QVERIFY(
      !imported.contains(QStringLiteral("power.idle.ac.displayOffEnabled")));
  QVERIFY(
      !imported.contains(QStringLiteral("power.idle.ac.displayOffSeconds")));
  QVERIFY(!imported.contains(QStringLiteral("power.idle.ac.suspendAction")));
  QVERIFY(!imported.contains(QStringLiteral("power.critical.action")));
}

void PowerDevilImportTests::missingAndMalformedFilesAreRetryable() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVariantMap entries{{QStringLiteral("sentinel"), 1}};
  QVERIFY(!readPowerDevilPreferences(
      dir.filePath(QStringLiteral("missing.ini")), &entries));
  QVERIFY(entries.contains(QStringLiteral("sentinel")));
  const QString path = dir.filePath(QStringLiteral("powerdevilrc"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("[AC\nnot an ini document\n");
  file.close();
  QVERIFY(!readPowerDevilPreferences(path, &entries));
  QVERIFY(entries.contains(QStringLiteral("sentinel")));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write("[AC][Display]\nTurnOffDisplayWhenIdle=true\n");
  file.close();
  QVERIFY(readPowerDevilPreferences(path, &entries));
  QVERIFY(
      entries.contains(QStringLiteral("AC/Display/TurnOffDisplayWhenIdle")));
}

void PowerDevilImportTests::schemaBoundsPolicyValues() {
  QVERIFY(m_schema
              ->validateValue(QStringLiteral("power.lid.ac.action"),
                              QStringLiteral("suspend"))
              .isValid());
  QVERIFY(!m_schema
               ->validateValue(QStringLiteral("power.lid.ac.action"),
                               QStringLiteral("shutdown"))
               .isValid());
  QVERIFY(
      m_schema
          ->validateValue(QStringLiteral("power.critical.countdownSeconds"), 30)
          .isValid());
  QVERIFY(
      !m_schema
           ->validateValue(QStringLiteral("power.critical.countdownSeconds"), 0)
           .isValid());
  QVERIFY(!m_schema
               ->validateValue(
                   QStringLiteral("power.idle.ac.displayOffSeconds"), 14401)
               .isValid());
  QCOMPARE(m_schema->systemDefaults()
               .value(QStringLiteral("power.profile.ac"))
               .toString(),
           QStringLiteral("none"));
  QVERIFY(m_schema
              ->validateValue(QStringLiteral("power.profile.ac"),
                              QStringLiteral("none"))
              .isValid());
}

void PowerDevilImportTests::repositoryCommitIsAtomicAndRetryable() {
  const QVariantMap legacy{
      {QStringLiteral("AC/SuspendAndShutdown/LidAction"), 1}};
  const ImportPlan plan = planPowerDevilImport(legacy, {});
  QVector<SettingsRepository::Operation> ops;
  for (const auto &entry : plan.values)
    ops.push_back({entry.first, false, entry.second});
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  SettingsRepository failed(LayeredSettings(*m_schema), dir.path(),
                            QStringLiteral("failed"));
  QVERIFY(!failed.commitUserOverrides(0, ops).ok());
  const auto afterFailure =
      failed.snapshot({QStringLiteral("power.migration.powerDevilImported")});
  QVERIFY(afterFailure.ok);
  QVERIFY(!afterFailure.values
               .value(QStringLiteral("power.migration.powerDevilImported"))
               .toBool());
  const QString goodPath = dir.filePath(QStringLiteral("user.json"));
  SettingsRepository retry(LayeredSettings(*m_schema), goodPath,
                           QStringLiteral("retry"));
  QVERIFY(retry.commitUserOverrides(0, ops).ok());
  QVERIFY(
      retry.snapshot({QStringLiteral("power.migration.powerDevilImported")})
          .values.value(QStringLiteral("power.migration.powerDevilImported"))
          .toBool());
}

QTEST_GUILESS_MAIN(PowerDevilImportTests)
#include "tst_powerdevil_import.moc"
