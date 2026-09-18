// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_about_computer/about_computer_settings_model.h>

#include <QtCore/QLocale>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <utility>

namespace QindaQt::Apps::SettingsAboutComputer {
namespace {
QString bytesToHuman(const qint64 bytes) {
  return QLocale::system().formattedDataSize(bytes);
}
} // namespace

AboutComputerSettingsModel::AboutComputerSettingsModel(
    std::unique_ptr<AboutComputerInfoSource> source, QObject *parent)
    : QObject(parent), m_source(std::move(source)) {
  m_info = m_source->read();
}

QString AboutComputerSettingsModel::diskSummary() const {
  if (!m_info.diskAvailable) return tr("Disk information unavailable");
  return tr("%1 free of %2")
      .arg(bytesToHuman(m_info.diskAvailableBytes),
           bytesToHuman(m_info.diskTotalBytes));
}

QString AboutComputerSettingsModel::memorySummary() const {
  if (!m_info.memoryAvailable) return tr("Memory information unavailable");
  return tr("%1 available of %2")
      .arg(bytesToHuman(m_info.memoryAvailableBytes),
           bytesToHuman(m_info.memoryTotalBytes));
}

QString AboutComputerSettingsModel::batterySummary() const {
  if (!m_info.batteryPresent) return tr("No battery reported");
  const QString charge = m_info.batteryPercentageKnown
      ? tr("%1%").arg(QString::number(m_info.batteryPercentage, 'f', 0))
      : tr("unknown");
  return tr("%1 %2 · %3, %4")
      .arg(m_info.batteryVendor, m_info.batteryModel, charge,
           batteryChargeStateLabel(m_info.batteryState));
}

QString AboutComputerSettingsModel::batteryHealthSummary() const {
  if (!m_info.batteryPresent) return QString();
  if (!m_info.batteryHealthKnown) return tr("Battery health unavailable");
  return tr("Battery health: %1%")
      .arg(QString::number(m_info.batteryHealthPercent, 'f', 0));
}

void AboutComputerSettingsModel::refresh() {
  m_info = m_source->read();
  m_copyStatusText.clear();
  Q_EMIT changed();
}

bool AboutComputerSettingsModel::copyReport() {
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard == nullptr) {
    m_copyStatusText = tr("Clipboard unavailable");
    Q_EMIT changed();
    return false;
  }
  clipboard->setText(formatAboutComputerReport(m_info));
  m_copyStatusText = tr("Report copied");
  Q_EMIT changed();
  return true;
}

} // namespace QindaQt::Apps::SettingsAboutComputer
