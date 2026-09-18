// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_about_computer/about_computer_settings_model.h>

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtTest>

using QindaQt::Apps::SettingsAboutComputer::AboutComputerInfo;
using QindaQt::Apps::SettingsAboutComputer::AboutComputerInfoSource;
using QindaQt::Apps::SettingsAboutComputer::AboutComputerSettingsModel;
using QindaQt::Apps::SettingsAboutComputer::BatteryChargeState;

namespace {
class FakeInfoSource final : public AboutComputerInfoSource {
public:
  AboutComputerInfo info;
  mutable int readCalls = 0;

  AboutComputerInfo read() const override {
    ++readCalls;
    return info;
  }
};
} // namespace

class AboutComputerSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void readsOnceAtConstructionAndProjectsEveryField();
  void unavailableSourcesReportUnavailableSummaries();
  void refreshReReadsTheSourceAndClearsCopyStatus();
  void copyReportWritesTheFormattedReportToTheClipboard();
};

void AboutComputerSettingsModelTest::
    readsOnceAtConstructionAndProjectsEveryField() {
  auto sourceOwned = std::make_unique<FakeInfoSource>();
  FakeInfoSource *source = sourceOwned.get();
  source->info.qindaqtVersion = QStringLiteral("0.1.0");
  source->info.hostnamedAvailable = true;
  source->info.hostname = QStringLiteral("qinda-top");
  source->info.diskAvailable = true;
  source->info.diskTotalBytes = 100;
  source->info.diskAvailableBytes = 50;
  source->info.batteryPresent = true;
  source->info.batteryVendor = QStringLiteral("CSMX202");
  source->info.batteryHealthKnown = true;
  source->info.batteryHealthPercent = 90.0;

  AboutComputerSettingsModel model(std::move(sourceOwned));
  QCOMPARE(source->readCalls, 1);
  QCOMPARE(model.qindaqtVersion(), QStringLiteral("0.1.0"));
  QCOMPARE(model.hostnamedAvailable(), true);
  QCOMPARE(model.hostname(), QStringLiteral("qinda-top"));
  QCOMPARE(model.diskAvailable(), true);
  QVERIFY(model.diskSummary().contains(QStringLiteral("50")));
  QVERIFY(model.batterySummary().contains(QStringLiteral("CSMX202")));
  QVERIFY(model.batteryHealthSummary().contains(QStringLiteral("90")));
}

void AboutComputerSettingsModelTest::
    unavailableSourcesReportUnavailableSummaries() {
  AboutComputerSettingsModel model(std::make_unique<FakeInfoSource>());
  QCOMPARE(model.hostnamedAvailable(), false);
  QCOMPARE(model.diskAvailable(), false);
  QCOMPARE(model.memoryAvailable(), false);
  QCOMPARE(model.batteryPresent(), false);
  QVERIFY(model.diskSummary().contains(QStringLiteral("unavailable")));
  QVERIFY(model.batterySummary().contains(QStringLiteral("No battery")));
}

void AboutComputerSettingsModelTest::
    refreshReReadsTheSourceAndClearsCopyStatus() {
  auto sourceOwned = std::make_unique<FakeInfoSource>();
  FakeInfoSource *source = sourceOwned.get();
  AboutComputerSettingsModel model(std::move(sourceOwned));
  QCOMPARE(source->readCalls, 1);

  QSignalSpy changedSpy(&model, &AboutComputerSettingsModel::changed);
  model.copyReport();
  QVERIFY(!model.copyStatusText().isEmpty());

  source->info.hostnamedAvailable = true;
  source->info.hostname = QStringLiteral("updated-host");
  model.refresh();
  QCOMPARE(source->readCalls, 2);
  QCOMPARE(model.hostname(), QStringLiteral("updated-host"));
  QVERIFY(model.copyStatusText().isEmpty());
  QVERIFY(changedSpy.size() >= 2);
}

void AboutComputerSettingsModelTest::
    copyReportWritesTheFormattedReportToTheClipboard() {
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard == nullptr) QSKIP("no clipboard available in this environment");

  auto sourceOwned = std::make_unique<FakeInfoSource>();
  sourceOwned->info.qindaqtVersion = QStringLiteral("0.1.0-copy-report-marker");
  AboutComputerSettingsModel model(std::move(sourceOwned));
  QVERIFY(model.copyReport());
  QVERIFY(clipboard->text().contains(QStringLiteral("0.1.0-copy-report-marker")));
  QVERIFY(!model.copyStatusText().isEmpty());
}

QTEST_MAIN(AboutComputerSettingsModelTest)
#include "tst_about_computer_settings_model.moc"
