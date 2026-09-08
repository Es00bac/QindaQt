// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_window.h"

#include "../core/monitor_engine.h"
#include "metric_chart.h"
#include "monitor_controller.h"

#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QTableWidget>

#include <cmath>
#include <limits>

namespace QindaQt::Apps::SystemMonitor {
namespace {

QVariantMap selectedDevice(QComboBox *selector) {
  return selector ? selector->currentData().toMap() : QVariantMap();
}

void refreshDevices(QComboBox *selector, const QVariantList &devices,
                    const QString &identifier, bool preferNonLoopback = false) {
  if (!selector) {
    return;
  }
  const QString currentId =
      selectedDevice(selector).value(identifier).toString();
  selector->blockSignals(true);
  selector->clear();
  for (const QVariant &device : devices) {
    const QVariantMap values = device.toMap();
    selector->addItem(values.value(QStringLiteral("name")).toString(), values);
  }
  int selectedRow = -1;
  for (int row = 0; row < selector->count(); ++row) {
    if (selector->itemData(row).toMap().value(identifier).toString() ==
        currentId) {
      selectedRow = row;
      break;
    }
  }
  if (selectedRow < 0 && preferNonLoopback) {
    for (int row = 0; row < selector->count(); ++row) {
      if (selector->itemData(row).toMap().value(QStringLiteral("name")) !=
          QLatin1String("lo")) {
        selectedRow = row;
        break;
      }
    }
  }
  if (selectedRow >= 0) {
    selector->setCurrentIndex(selectedRow);
  }
  selector->blockSignals(false);
}

} // namespace

void SystemMonitorWindow::appendSample(const QString &series, qint64 timestamp,
                                       const QVariant &value) {
  const double sample = value.isValid() && !value.isNull()
                            ? value.toDouble()
                            : std::numeric_limits<double>::quiet_NaN();
  auto &history = m_histories[series];
  if (!history.isEmpty() &&
      qFuzzyCompare(history.last().x() + 1.0, double(timestamp) + 1.0)) {
    history.last().setY(sample);
    return;
  }
  history.append(QPointF(static_cast<qreal>(timestamp), sample));
  while (history.size() > m_historyLimit) {
    history.removeFirst();
  }
}

void SystemMonitorWindow::trimHistories() {
  for (auto it = m_histories.begin(); it != m_histories.end(); ++it) {
    while (it->size() > m_historyLimit) {
      it->removeFirst();
    }
  }
}

void SystemMonitorWindow::updateSnapshot() {
  const QVariantMap snapshot = m_controller.engine().snapshot();
  const qint64 timestamp =
      snapshot.value(QStringLiteral("timestamp")).toLongLong();
  appendSample(QStringLiteral("cpu"), timestamp,
               snapshot.value(QStringLiteral("cpu")));
  const QVariantMap memory = snapshot.value(QStringLiteral("memory")).toMap();
  const double total = memory.value(QStringLiteral("total")).toDouble();
  appendSample(
      QStringLiteral("memory"), timestamp,
      total > 0.0
          ? QVariant(100.0 * memory.value(QStringLiteral("used")).toDouble() /
                     total)
          : QVariant());

  if (m_cpuChart) {
    m_cpuChart->setSamples(m_histories.value(QStringLiteral("cpu")),
                           palette().color(QPalette::Highlight), 100.0);
  }
  if (m_memoryChart) {
    m_memoryChart->setSamples(m_histories.value(QStringLiteral("memory")),
                              palette().color(QPalette::Link), 100.0);
  }
  const QVariantList disks = snapshot.value(QStringLiteral("disks")).toList();
  const QVariantList networks =
      snapshot.value(QStringLiteral("network")).toList();
  qint64 diskRead = 0;
  qint64 diskWrite = 0;
  qint64 networkReceive = 0;
  qint64 networkSend = 0;
  bool diskRatesAvailable = false;
  bool networkRatesAvailable = false;
  for (const QVariant &entry : disks) {
    const QVariantMap disk = entry.toMap();
    const QVariant read = disk.value(QStringLiteral("readRate"));
    const QVariant write = disk.value(QStringLiteral("writeRate"));
    if (read.isValid() && !read.isNull() && write.isValid() && !write.isNull()) {
      diskRead += read.toLongLong();
      diskWrite += write.toLongLong();
      diskRatesAvailable = true;
    }
  }
  for (const QVariant &entry : networks) {
    const QVariantMap network = entry.toMap();
    const QVariant receive = network.value(QStringLiteral("rxRate"));
    const QVariant send = network.value(QStringLiteral("txRate"));
    if (receive.isValid() && !receive.isNull() && send.isValid() && !send.isNull()) {
      networkReceive += receive.toLongLong();
      networkSend += send.toLongLong();
      networkRatesAvailable = true;
    }
  }
  if (m_overviewSummary) {
    m_overviewSummary->setText(
        tr("CPU %1% · Memory %2 / %3 · Disk R/W %4 / %5 · Network ↓/↑ %6 / %7 · Uptime %8 h")
            .arg(QString::number(
                     snapshot.value(QStringLiteral("cpu")).toDouble(), 'f', 1),
                 bytes(memory.value(QStringLiteral("used")).toLongLong()),
                 bytes(memory.value(QStringLiteral("total")).toLongLong()),
                 rate(diskRatesAvailable ? QVariant(diskRead) : QVariant()),
                 rate(diskRatesAvailable ? QVariant(diskWrite) : QVariant()),
                 rate(networkRatesAvailable ? QVariant(networkReceive) : QVariant()),
                 rate(networkRatesAvailable ? QVariant(networkSend) : QVariant()),
                 QString::number(
                     snapshot.value(QStringLiteral("uptime")).toLongLong() /
                     3600)));
  }

  if (m_memoryDetails) {
    m_memoryDetails->setText(
        tr("Used %1 · Available %2 · Cached %3 · Swap %4 / %5")
            .arg(bytes(memory.value("used").toLongLong()),
                 bytes(memory.value("available").toLongLong()),
                 bytes(memory.value("cached").toLongLong()),
                 bytes(memory.value("swapUsed").toLongLong()),
                 bytes(memory.value("swapTotal").toLongLong())));
  }
  for (const QVariant &entry : disks) {
    const QVariantMap disk = entry.toMap();
    const QString id = disk.value("id").toString();
    appendSample(QStringLiteral("disk:") + id + QStringLiteral(":readRate"),
                 timestamp, disk.value("readRate"));
    appendSample(QStringLiteral("disk:") + id + QStringLiteral(":writeRate"),
                 timestamp, disk.value("writeRate"));
  }
  for (const QVariant &entry : networks) {
    const QVariantMap network = entry.toMap();
    const QString id = network.value("name").toString();
    appendSample(QStringLiteral("network:") + id + QStringLiteral(":rxRate"),
                 timestamp, network.value("rxRate"));
    appendSample(QStringLiteral("network:") + id + QStringLiteral(":txRate"),
                 timestamp, network.value("txRate"));
  }
  if (m_filesystemTable) {
    const QVariantList filesystems = snapshot.value("filesystems").toList();
    m_filesystemTable->setRowCount(0);
    int row = 0;
    for (const QVariant &entry : filesystems) {
      const QVariantMap filesystem = entry.toMap();
      const qint64 capacity = filesystem.value("total").toLongLong();
      if (capacity <= 0) {
        continue;
      }
      m_filesystemTable->insertRow(row);
      m_filesystemTable->setItem(
          row, 0, new QTableWidgetItem(filesystem.value("path").toString()));
      m_filesystemTable->setItem(
          row, 1, new QTableWidgetItem(
                      bytes(filesystem.value("used").toLongLong())));
      m_filesystemTable->setItem(row, 2,
                                  new QTableWidgetItem(bytes(capacity)));
      ++row;
    }
  }
  refreshDevices(m_diskSelector, disks, QStringLiteral("id"));
  refreshDevices(m_networkSelector, networks, QStringLiteral("name"), true);
  updateDeviceCharts(false);
  updateDeviceCharts(true);
}

void SystemMonitorWindow::updateDeviceCharts(bool network) {
  QComboBox *selector = network ? m_networkSelector : m_diskSelector;
  MetricChart *firstChart = network ? m_networkReceiveChart : m_diskReadChart;
  MetricChart *secondChart = network ? m_networkSendChart : m_diskWriteChart;
  const QVariantMap device = selectedDevice(selector);
  if (device.isEmpty() || !firstChart || !secondChart) {
    return;
  }

  const QString id =
      device.value(network ? QStringLiteral("name") : QStringLiteral("id"))
          .toString();
  const QString prefix =
      network ? QStringLiteral("network:") : QStringLiteral("disk:");
  const QString firstKey =
      network ? QStringLiteral("rxRate") : QStringLiteral("readRate");
  const QString secondKey =
      network ? QStringLiteral("txRate") : QStringLiteral("writeRate");
  firstChart->setSamples(
      m_histories.value(prefix + id + QLatin1Char(':') + firstKey),
      palette().color(QPalette::Highlight));
  secondChart->setSamples(
      m_histories.value(prefix + id + QLatin1Char(':') + secondKey),
      palette().color(QPalette::Link));
  firstChart->setLegend(rate(device.value(firstKey)));
  secondChart->setLegend(rate(device.value(secondKey)));
}

void SystemMonitorWindow::updateHardware() {
  const QVariantMap hardware = m_controller.hardware();
  const QString model = hardware.value(QStringLiteral("cpuModel")).toString();
  const QVariant frequency = hardware.value(QStringLiteral("frequencyMHz"));
  m_hardwareSummary->setText(
      tr("CPU: %1\nCurrent frequency: %2 MHz")
          .arg(model.isEmpty() ? tr("Unavailable") : model,
               frequency.isValid() ? frequency.toString() : tr("Unavailable")));

  const QVariantList sensors =
      hardware.value(QStringLiteral("sensors")).toList();
  if (m_sensorDetails) {
    QStringList readings;
    for (const QVariant &entry : sensors) {
      const QVariantMap sensor = entry.toMap();
      readings << tr("%1: %2 %3")
                      .arg(sensor.value("name").toString(),
                           sensor.value("value").toString(),
                           sensor.value("unit").toString());
    }
    m_sensorDetails->setText(readings.isEmpty()
                                 ? tr("Sensors unavailable")
                                 : readings.join(QLatin1Char('\n')));
  }
  const QVariantList gpus = hardware.value(QStringLiteral("gpus")).toList();
  const QString currentId =
      selectedDevice(m_gpuSelector).value(QStringLiteral("id")).toString();
  m_gpuSelector->blockSignals(true);
  m_gpuSelector->clear();
  for (const QVariant &entry : gpus) {
    const QVariantMap gpu = entry.toMap();
    m_gpuSelector->addItem(gpu.value(QStringLiteral("name")).toString(), gpu);
  }
  for (int row = 0; row < m_gpuSelector->count(); ++row) {
    if (m_gpuSelector->itemData(row).toMap().value(QStringLiteral("id")) ==
        currentId) {
      m_gpuSelector->setCurrentIndex(row);
      break;
    }
  }
  m_gpuSelector->blockSignals(false);
  const QVariantMap gpu = selectedDevice(m_gpuSelector);
  if (gpu.isEmpty()) {
    m_gpuChart->setLegend(tr("GPU telemetry unavailable"));
    return;
  }

  const qint64 timestamp = m_controller.hardwareTimestamp();
  const QString historyId =
      QStringLiteral("gpu:") + gpu.value(QStringLiteral("id")).toString();
  appendSample(historyId, timestamp, gpu.value(QStringLiteral("busy")));
  m_gpuChart->setSamples(m_histories.value(historyId),
                         palette().color(QPalette::Highlight), 100.0);
  const QVariant memoryUsed = gpu.value(QStringLiteral("memoryUsed"));
  const QVariant memoryTotal = gpu.value(QStringLiteral("memoryTotal"));
  const QString used = memoryUsed.isValid() && !memoryUsed.isNull()
                           ? bytes(memoryUsed.toLongLong())
                           : tr("Unavailable");
  const QString total = memoryTotal.isValid() && !memoryTotal.isNull()
                            ? bytes(memoryTotal.toLongLong())
                            : tr("Unavailable");
  m_gpuChart->setLegend(tr("%1 · VRAM %2 / %3")
                            .arg(gpu.value(QStringLiteral("name")).toString(),
                                 used, total));
}

} // namespace QindaQt::Apps::SystemMonitor
