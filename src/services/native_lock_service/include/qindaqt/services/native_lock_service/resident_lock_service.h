// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <memory>
namespace QindaQt::Services::SessionLockState {
class NativeLockStateMonitor;
}
namespace QindaQt::Services::NativeLock {
class NativeLockRequest;
// Same-thread facade with constructor-visible, borrowed monitor/request ports.
// Both ports outlive this object. Caller owns admission and monitor lifecycle.
// start claims Lock1 + ScreenSaver names atomically from this process's view;
// failure rolls back only our objects/names. No activation or lock on startup.
// Manual requests ignore idle policy. Success means admission, not Protected.
class ResidentLockService final : public QObject {
  Q_OBJECT
public:
  ResidentLockService(QDBusConnection bus,
                      SessionLockState::NativeLockStateMonitor &monitor,
                      NativeLockRequest &request, QObject *parent = nullptr);
  ~ResidentLockService() override;
  bool start(QString *error = nullptr);
  void stop();

private:
  class Private;
  std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::NativeLock
