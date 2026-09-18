// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_about_computer/about_computer_info.h>

#include <QtTest>

using QindaQt::Apps::SettingsAboutComputer::AboutComputerInfo;
using QindaQt::Apps::SettingsAboutComputer::BatteryChargeState;
using QindaQt::Apps::SettingsAboutComputer::batteryChargeStateLabel;
using QindaQt::Apps::SettingsAboutComputer::formatAboutComputerReport;

class AboutComputerInfoTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void batteryChargeStateLabelsAreHumanReadable();
  void reportIncludesEveryAvailableSource();
  void reportSaysUnavailableForEveryMissingSource();
};

void AboutComputerInfoTest::batteryChargeStateLabelsAreHumanReadable() {
  QCOMPARE(batteryChargeStateLabel(BatteryChargeState::Charging),
           QStringLiteral("Charging"));
  QCOMPARE(batteryChargeStateLabel(BatteryChargeState::Discharging),
           QStringLiteral("Discharging"));
  QCOMPARE(batteryChargeStateLabel(BatteryChargeState::FullyCharged),
           QStringLiteral("Fully charged"));
  QCOMPARE(batteryChargeStateLabel(BatteryChargeState::Unknown),
           QStringLiteral("Unknown"));
}

void AboutComputerInfoTest::reportIncludesEveryAvailableSource() {
  AboutComputerInfo info;
  info.qindaqtVersion = QStringLiteral("0.1.0");
  info.installedCheckpointAvailable = true;
  info.installedCheckpoint = QStringLiteral("qindaqt-desktop-0.1.0_pre20260917-r4");
  info.hostnamedAvailable = true;
  info.hostname = QStringLiteral("qinda-top");
  info.hardwareVendor = QStringLiteral("Lenovo");
  info.hardwareModel = QStringLiteral("IdeaPad Slim 3 15ABR8");
  info.chassis = QStringLiteral("laptop");
  info.kernelName = QStringLiteral("Linux");
  info.kernelRelease = QStringLiteral("6.18.48-gentoo-dist-bin");
  info.operatingSystemPrettyName = QStringLiteral("Gentoo Linux");
  info.diskAvailable = true;
  info.diskTotalBytes = 500'000'000'000;
  info.diskAvailableBytes = 250'000'000'000;
  info.memoryAvailable = true;
  info.memoryTotalBytes = 16'000'000'000;
  info.memoryAvailableBytes = 8'000'000'000;
  info.batteryPresent = true;
  info.batteryVendor = QStringLiteral("CSMX202");
  info.batteryModel = QStringLiteral("L22X3PF2");
  info.batteryPercentageKnown = true;
  info.batteryPercentage = 98.0;
  info.batteryState = BatteryChargeState::FullyCharged;
  info.batteryHealthKnown = true;
  info.batteryHealthPercent = 99.4;

  const QString report = formatAboutComputerReport(info);
  QVERIFY(report.contains(QStringLiteral("0.1.0")));
  QVERIFY(report.contains(QStringLiteral("qindaqt-desktop-0.1.0_pre20260917-r4")));
  QVERIFY(report.contains(QStringLiteral("qinda-top")));
  QVERIFY(report.contains(QStringLiteral("Lenovo")));
  QVERIFY(report.contains(QStringLiteral("Linux")));
  QVERIFY(report.contains(QStringLiteral("Gentoo Linux")));
  QVERIFY(report.contains(QStringLiteral("CSMX202")));
  QVERIFY(report.contains(QStringLiteral("Fully charged")));
  QVERIFY(report.contains(QStringLiteral("99%")));
}

void AboutComputerInfoTest::reportSaysUnavailableForEveryMissingSource() {
  const AboutComputerInfo info;
  const QString report = formatAboutComputerReport(info);
  QVERIFY(report.contains(QStringLiteral("Hostname, hardware, and kernel: unavailable")));
  QVERIFY(report.contains(QStringLiteral("Disk (root): unavailable")));
  QVERIFY(report.contains(QStringLiteral("Memory: unavailable")));
  QVERIFY(report.contains(QStringLiteral("Battery: none reported")));
  QVERIFY(report.contains(QStringLiteral("Installed checkpoint: unavailable")));
}

QTEST_GUILESS_MAIN(AboutComputerInfoTest)
#include "tst_about_computer_info.moc"
