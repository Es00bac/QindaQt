// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor_controller.h"

#include "../core/monitor_engine.h"

#include <QDateTime>
#include <QTimer>
#include <QtConcurrentRun>

namespace QindaQt::Apps::SystemMonitor {

MonitorController::MonitorController(
    QindaQt::SystemMonitor::MonitorEngine &engine,
    HardwareSource hardwareSource, QObject *parent)
    : QObject(parent), m_engine(engine),
      m_hardwareSource(std::move(hardwareSource)),
      m_hardwareTimer(new QTimer(this)) {
  m_hardwareTimer->setInterval(m_engine.interval());
  connect(m_hardwareTimer, &QTimer::timeout, this,
          &MonitorController::refreshHardware);
  connect(&m_hardwareWatcher, &QFutureWatcher<QVariantMap>::finished, this,
          [this] {
            m_hardware = m_hardwareWatcher.result();
            m_hardwareTimestamp = QDateTime::currentMSecsSinceEpoch();
            Q_EMIT hardwareUpdated();
          });
  connect(&m_engine, &QindaQt::SystemMonitor::MonitorEngine::intervalChanged,
          this, [this] { m_hardwareTimer->setInterval(m_engine.interval()); });
  connect(&m_engine, &QindaQt::SystemMonitor::MonitorEngine::pausedChanged,
          this, [this] {
            if (m_engine.paused()) {
              m_hardwareTimer->stop();
            } else {
              m_hardwareTimer->start();
              refreshHardware();
            }
          });

  m_hardwareTimer->start();
  refreshHardware();
}

QindaQt::SystemMonitor::MonitorEngine &MonitorController::engine() const {
  return m_engine;
}

QVariantMap MonitorController::hardware() const { return m_hardware; }

qint64 MonitorController::hardwareTimestamp() const {
  return m_hardwareTimestamp;
}

void MonitorController::refreshHardware() {
  if (!m_hardwareSource || m_hardwareWatcher.isRunning()) {
    return;
  }
  m_hardwareWatcher.setFuture(QtConcurrent::run(m_hardwareSource));
}

} // namespace QindaQt::Apps::SystemMonitor
