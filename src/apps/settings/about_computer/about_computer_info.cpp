// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_about_computer/about_computer_info.h>

#include <QtCore/QLocale>
#include <QtCore/QStringList>

namespace QindaQt::Apps::SettingsAboutComputer {
namespace {
QString bytesToHuman(const qint64 bytes) {
  return QLocale::system().formattedDataSize(bytes);
}

QString percentText(const double value) {
  return QStringLiteral("%1%").arg(QString::number(value, 'f', 0));
}
} // namespace

QString batteryChargeStateLabel(const BatteryChargeState state) {
  switch (state) {
  case BatteryChargeState::Charging: return QStringLiteral("Charging");
  case BatteryChargeState::Discharging: return QStringLiteral("Discharging");
  case BatteryChargeState::Empty: return QStringLiteral("Empty");
  case BatteryChargeState::FullyCharged: return QStringLiteral("Fully charged");
  case BatteryChargeState::PendingCharge: return QStringLiteral("Pending charge");
  case BatteryChargeState::PendingDischarge:
    return QStringLiteral("Pending discharge");
  case BatteryChargeState::Unknown: break;
  }
  return QStringLiteral("Unknown");
}

QString formatAboutComputerReport(const AboutComputerInfo &info) {
  QStringList lines;
  lines << QStringLiteral("QindaQt version: %1").arg(
      info.qindaqtVersion.isEmpty() ? QStringLiteral("unknown")
                                    : info.qindaqtVersion);
  lines << QStringLiteral("Installed checkpoint: %1").arg(
      info.installedCheckpointAvailable ? info.installedCheckpoint
                                        : QStringLiteral("unavailable"));
  lines << QString();
  if (info.hostnamedAvailable) {
    lines << QStringLiteral("Hostname: %1").arg(info.hostname);
    lines << QStringLiteral("Hardware: %1 %2 (%3)")
                 .arg(info.hardwareVendor, info.hardwareModel, info.chassis);
    lines << QStringLiteral("Kernel: %1 %2")
                 .arg(info.kernelName, info.kernelRelease);
    lines << QStringLiteral("Operating system: %1")
                 .arg(info.operatingSystemPrettyName);
  } else {
    lines << QStringLiteral("Hostname, hardware, and kernel: unavailable");
  }
  lines << QString();
  lines << QStringLiteral("Disk (root): %1")
               .arg(info.diskAvailable
                        ? QStringLiteral("%1 free of %2")
                              .arg(bytesToHuman(info.diskAvailableBytes),
                                   bytesToHuman(info.diskTotalBytes))
                        : QStringLiteral("unavailable"));
  lines << QStringLiteral("Memory: %1")
               .arg(info.memoryAvailable
                        ? QStringLiteral("%1 available of %2")
                              .arg(bytesToHuman(info.memoryAvailableBytes),
                                   bytesToHuman(info.memoryTotalBytes))
                        : QStringLiteral("unavailable"));
  lines << QString();
  if (info.batteryPresent) {
    lines << QStringLiteral("Battery: %1 %2")
                 .arg(info.batteryVendor, info.batteryModel);
    lines << QStringLiteral("  Charge: %1, %2")
                 .arg(info.batteryPercentageKnown
                          ? percentText(info.batteryPercentage)
                          : QStringLiteral("unknown"),
                      batteryChargeStateLabel(info.batteryState));
    lines << QStringLiteral("  Health: %1")
                 .arg(info.batteryHealthKnown
                          ? percentText(info.batteryHealthPercent)
                          : QStringLiteral("unavailable"));
  } else {
    lines << QStringLiteral("Battery: none reported");
  }
  return lines.join(QLatin1Char('\n'));
}

} // namespace QindaQt::Apps::SettingsAboutComputer
