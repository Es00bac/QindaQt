// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QString>

namespace QindaQt::Apps::SettingsAboutComputer {

enum class BatteryChargeState {
  Unknown,
  Charging,
  Discharging,
  Empty,
  FullyCharged,
  PendingCharge,
  PendingDischarge,
};

[[nodiscard]] QString batteryChargeStateLabel(BatteryChargeState state);

struct AboutComputerInfo final {
  // org.freedesktop.hostname1, read-only.
  bool hostnamedAvailable = false;
  QString hostname;
  QString chassis;
  QString hardwareVendor;
  QString hardwareModel;
  QString kernelName;
  QString kernelRelease;
  QString operatingSystemPrettyName;

  // Compile-time QindaQt release version.
  QString qindaqtVersion;
  // The exact atom Portage has installed (e.g.
  // "qindaqt-desktop-0.1.0_pre20260917-r4"), read from /var/db/pkg. Empty
  // when unavailable (non-Gentoo host, sandboxed test environment, or the
  // package database is unreadable) -- reported as such, never guessed.
  bool installedCheckpointAvailable = false;
  QString installedCheckpoint;

  // QStorageInfo::root().
  bool diskAvailable = false;
  qint64 diskTotalBytes = 0;
  qint64 diskAvailableBytes = 0;

  // /proc/meminfo, in bytes (converted from the file's kB units).
  bool memoryAvailable = false;
  qint64 memoryTotalBytes = 0;
  qint64 memoryAvailableBytes = 0;

  // org.freedesktop.UPower, read-only, one representative present battery
  // device (Type == Battery). EnergyFullDesign is not carried by the
  // public Power1 protocol's PowerSupply struct, so this route reads
  // UPower directly for this one supplementary field rather than growing
  // that shared protocol for a single read-only info page.
  bool batteryPresent = false;
  QString batteryVendor;
  QString batteryModel;
  bool batteryPercentageKnown = false;
  double batteryPercentage = 0.0;
  BatteryChargeState batteryState = BatteryChargeState::Unknown;
  bool batteryHealthKnown = false;
  double batteryHealthPercent = 0.0;
};

// Plain-text report for the "copy report" action. Pure formatting, no I/O.
[[nodiscard]] QString formatAboutComputerReport(const AboutComputerInfo &info);

} // namespace QindaQt::Apps::SettingsAboutComputer
