// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_about_computer/about_computer_info.h>
#include <qindaqt/apps/settings_about_computer/about_computer_info_reader.h>

#include <QtCore/QObject>

#include <memory>

namespace QindaQt::Apps::SettingsAboutComputer {

// Read-only projection of one AboutComputerInfo snapshot. Unlike every
// other Settings route this wave, this route has no mutation authority and
// no live subscription: refresh() re-reads every source once, synchronously
// (all sources are already fast local reads: one-shot D-Bus property gets,
// /proc, statvfs -- there is nothing to debounce or fence against).
class AboutComputerSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString qindaqtVersion READ qindaqtVersion NOTIFY changed)
  Q_PROPERTY(bool installedCheckpointAvailable READ installedCheckpointAvailable
                 NOTIFY changed)
  Q_PROPERTY(QString installedCheckpoint READ installedCheckpoint NOTIFY changed)
  Q_PROPERTY(bool hostnamedAvailable READ hostnamedAvailable NOTIFY changed)
  Q_PROPERTY(QString hostname READ hostname NOTIFY changed)
  Q_PROPERTY(QString chassis READ chassis NOTIFY changed)
  Q_PROPERTY(QString hardwareVendor READ hardwareVendor NOTIFY changed)
  Q_PROPERTY(QString hardwareModel READ hardwareModel NOTIFY changed)
  Q_PROPERTY(QString kernelName READ kernelName NOTIFY changed)
  Q_PROPERTY(QString kernelRelease READ kernelRelease NOTIFY changed)
  Q_PROPERTY(QString operatingSystemPrettyName READ operatingSystemPrettyName
                 NOTIFY changed)
  Q_PROPERTY(bool diskAvailable READ diskAvailable NOTIFY changed)
  Q_PROPERTY(QString diskSummary READ diskSummary NOTIFY changed)
  Q_PROPERTY(bool memoryAvailable READ memoryAvailable NOTIFY changed)
  Q_PROPERTY(QString memorySummary READ memorySummary NOTIFY changed)
  Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY changed)
  Q_PROPERTY(QString batterySummary READ batterySummary NOTIFY changed)
  Q_PROPERTY(QString batteryHealthSummary READ batteryHealthSummary NOTIFY changed)
  Q_PROPERTY(QString copyStatusText READ copyStatusText NOTIFY changed)

public:
  explicit AboutComputerSettingsModel(
      std::unique_ptr<AboutComputerInfoSource> source, QObject *parent = nullptr);

  [[nodiscard]] QString qindaqtVersion() const { return m_info.qindaqtVersion; }
  [[nodiscard]] bool installedCheckpointAvailable() const {
    return m_info.installedCheckpointAvailable;
  }
  [[nodiscard]] QString installedCheckpoint() const {
    return m_info.installedCheckpoint;
  }
  [[nodiscard]] bool hostnamedAvailable() const { return m_info.hostnamedAvailable; }
  [[nodiscard]] QString hostname() const { return m_info.hostname; }
  [[nodiscard]] QString chassis() const { return m_info.chassis; }
  [[nodiscard]] QString hardwareVendor() const { return m_info.hardwareVendor; }
  [[nodiscard]] QString hardwareModel() const { return m_info.hardwareModel; }
  [[nodiscard]] QString kernelName() const { return m_info.kernelName; }
  [[nodiscard]] QString kernelRelease() const { return m_info.kernelRelease; }
  [[nodiscard]] QString operatingSystemPrettyName() const {
    return m_info.operatingSystemPrettyName;
  }
  [[nodiscard]] bool diskAvailable() const { return m_info.diskAvailable; }
  [[nodiscard]] QString diskSummary() const;
  [[nodiscard]] bool memoryAvailable() const { return m_info.memoryAvailable; }
  [[nodiscard]] QString memorySummary() const;
  [[nodiscard]] bool batteryPresent() const { return m_info.batteryPresent; }
  [[nodiscard]] QString batterySummary() const;
  [[nodiscard]] QString batteryHealthSummary() const;
  [[nodiscard]] QString copyStatusText() const { return m_copyStatusText; }

  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool copyReport();

Q_SIGNALS:
  void changed();

private:
  std::unique_ptr<AboutComputerInfoSource> m_source;
  AboutComputerInfo m_info;
  QString m_copyStatusText;
};

} // namespace QindaQt::Apps::SettingsAboutComputer
