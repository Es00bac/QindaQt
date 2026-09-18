// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_about_computer/about_computer_info_reader.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStorageInfo>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusReply>

namespace QindaQt::Apps::SettingsAboutComputer {
namespace {

constexpr auto HostnamedService = "org.freedesktop.hostname1";
constexpr auto HostnamedPath = "/org/freedesktop/hostname1";
constexpr auto HostnamedInterface = "org.freedesktop.hostname1";

constexpr auto UPowerService = "org.freedesktop.UPower";
constexpr auto UPowerPath = "/org/freedesktop/UPower";
constexpr auto UPowerInterface = "org.freedesktop.UPower";
constexpr auto UPowerDeviceInterface = "org.freedesktop.UPower.Device";
// UPower's own Device.Type: 2 == Battery. https://upower.freedesktop.org
constexpr quint32 UPowerDeviceTypeBattery = 2;

void readHostnamed(AboutComputerInfo &info) {
  QDBusInterface hostnamed(QString::fromLatin1(HostnamedService),
                           QString::fromLatin1(HostnamedPath),
                           QString::fromLatin1(HostnamedInterface),
                           QDBusConnection::systemBus());
  if (!hostnamed.isValid()) return;
  info.hostnamedAvailable = true;
  info.hostname = hostnamed.property("Hostname").toString();
  info.chassis = hostnamed.property("Chassis").toString();
  info.hardwareVendor = hostnamed.property("HardwareVendor").toString();
  info.hardwareModel = hostnamed.property("HardwareModel").toString();
  info.kernelName = hostnamed.property("KernelName").toString();
  info.kernelRelease = hostnamed.property("KernelRelease").toString();
  info.operatingSystemPrettyName =
      hostnamed.property("OperatingSystemPrettyName").toString();
}

void readInstalledCheckpoint(AboutComputerInfo &info) {
  // AGENT-NOTE: Gentoo/Portage-specific. Portage writes the exact installed
  // atom string to <category>/<pf>/PF under its package database; this is a
  // plain file read, no subprocess. A non-Gentoo host or an unreadable
  // database reports unavailable rather than guessing.
  const QDir packageDir(QStringLiteral("/var/db/pkg/gui-wm"));
  if (!packageDir.exists()) return;
  const QStringList matches =
      packageDir.entryList({QStringLiteral("qindaqt-desktop-*")}, QDir::Dirs);
  if (matches.isEmpty()) return;
  QFile pf(packageDir.filePath(matches.first() + QStringLiteral("/PF")));
  if (!pf.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  const QString checkpoint = QString::fromUtf8(pf.readAll()).trimmed();
  if (checkpoint.isEmpty()) return;
  info.installedCheckpointAvailable = true;
  info.installedCheckpoint = checkpoint;
}

void readDisk(AboutComputerInfo &info) {
  const QStorageInfo root = QStorageInfo::root();
  if (!root.isValid() || !root.isReady()) return;
  info.diskAvailable = true;
  info.diskTotalBytes = root.bytesTotal();
  info.diskAvailableBytes = root.bytesAvailable();
}

void readMemory(AboutComputerInfo &info) {
  QFile meminfo(QStringLiteral("/proc/meminfo"));
  if (!meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QTextStream stream(&meminfo);
  bool totalOk = false;
  bool availableOk = false;
  qint64 totalKib = 0;
  qint64 availableKib = 0;
  while (!stream.atEnd()) {
    const QString line = stream.readLine();
    if (line.startsWith(QStringLiteral("MemTotal:"))) {
      totalKib = line.mid(9).trimmed().split(QLatin1Char(' ')).constFirst()
                     .toLongLong(&totalOk);
    } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
      availableKib = line.mid(13).trimmed().split(QLatin1Char(' ')).constFirst()
                         .toLongLong(&availableOk);
    }
  }
  if (!totalOk || !availableOk) return;
  info.memoryAvailable = true;
  info.memoryTotalBytes = totalKib * 1024;
  info.memoryAvailableBytes = availableKib * 1024;
}

BatteryChargeState decodeUPowerState(const quint32 raw) {
  switch (raw) {
  case 1: return BatteryChargeState::Charging;
  case 2: return BatteryChargeState::Discharging;
  case 3: return BatteryChargeState::Empty;
  case 4: return BatteryChargeState::FullyCharged;
  case 5: return BatteryChargeState::PendingCharge;
  case 6: return BatteryChargeState::PendingDischarge;
  default: return BatteryChargeState::Unknown;
  }
}

void readBattery(AboutComputerInfo &info) {
  QDBusInterface upower(QString::fromLatin1(UPowerService),
                        QString::fromLatin1(UPowerPath),
                        QString::fromLatin1(UPowerInterface),
                        QDBusConnection::systemBus());
  if (!upower.isValid()) return;
  const QDBusReply<QList<QDBusObjectPath>> devices =
      upower.call(QStringLiteral("EnumerateDevices"));
  if (!devices.isValid()) return;
  for (const QDBusObjectPath &devicePath : devices.value()) {
    QDBusInterface device(QString::fromLatin1(UPowerService), devicePath.path(),
                          QString::fromLatin1(UPowerDeviceInterface),
                          QDBusConnection::systemBus());
    if (!device.isValid()) continue;
    bool typeOk = false;
    const quint32 type = device.property("Type").toUInt(&typeOk);
    if (!typeOk || type != UPowerDeviceTypeBattery) continue;
    if (!device.property("IsPresent").toBool()) continue;

    info.batteryPresent = true;
    info.batteryVendor = device.property("Vendor").toString();
    info.batteryModel = device.property("Model").toString();
    bool percentageOk = false;
    info.batteryPercentage = device.property("Percentage").toDouble(&percentageOk);
    info.batteryPercentageKnown = percentageOk;
    info.batteryState = decodeUPowerState(device.property("State").toUInt());
    const double energyFull = device.property("EnergyFull").toDouble();
    const double energyFullDesign = device.property("EnergyFullDesign").toDouble();
    if (energyFullDesign > 0.0) {
      info.batteryHealthKnown = true;
      info.batteryHealthPercent = (energyFull / energyFullDesign) * 100.0;
    }
    // AGENT-NOTE: report the first present battery device. Multi-battery
    // laptops are rare and UPower's own DisplayDevice aggregate does not
    // carry EnergyFullDesign, so there is no single "combined health" this
    // route could show instead.
    break;
  }
}

} // namespace

AboutComputerInfo SystemAboutComputerInfoSource::read() const {
  AboutComputerInfo info;
  info.qindaqtVersion = QStringLiteral(QINDAQT_VERSION);
  readHostnamed(info);
  readInstalledCheckpoint(info);
  readDisk(info);
  readMemory(info);
  readBattery(info);
  return info;
}

} // namespace QindaQt::Apps::SettingsAboutComputer
