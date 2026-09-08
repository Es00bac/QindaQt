// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QVariantMap>

#include <functional>

class QTimer;

namespace QindaQt::SystemMonitor {
class MonitorEngine;
}

namespace QindaQt::Apps::SystemMonitor {

// The source is injected so sampling is independently testable and the Linux
// provider stays outside both the engine and Widgets.
using HardwareSource = std::function<QVariantMap()>;

/**
 * Shares bounded asynchronous hardware sampling among all windows in one app
 * process. HardwareSampler is synchronous, so it is only called in the future.
 */
class MonitorController final : public QObject {
  Q_OBJECT

public:
  explicit MonitorController(QindaQt::SystemMonitor::MonitorEngine &engine,
                             HardwareSource hardwareSource,
                             QObject *parent = nullptr);

  [[nodiscard]] QindaQt::SystemMonitor::MonitorEngine &engine() const;
  [[nodiscard]] QVariantMap hardware() const;
  [[nodiscard]] qint64 hardwareTimestamp() const;

public Q_SLOTS:
  void refreshHardware();

Q_SIGNALS:
  void hardwareUpdated();

private:
  QindaQt::SystemMonitor::MonitorEngine &m_engine;
  HardwareSource m_hardwareSource;
  QFutureWatcher<QVariantMap> m_hardwareWatcher;
  QTimer *m_hardwareTimer = nullptr;
  QVariantMap m_hardware;
  qint64 m_hardwareTimestamp = 0;
};

} // namespace QindaQt::Apps::SystemMonitor
