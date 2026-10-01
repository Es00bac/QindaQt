// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <functional>
#include <memory>
namespace QindaQt::Services::SessionLockState { class NativeLockStateMonitor; }
namespace QindaQt::Session::NativeLockRuntime { class Runtime; }
namespace QindaQt::SessionSupervisor {
// Composition only. Borrowed same-thread runtime, monitor and readonly
// supervisor/ordinary-attachment admission must outlive this owner.
class NativeSleepComposition final {
public:
  NativeSleepComposition(QDBusConnection sessionBus,
      Session::NativeLockRuntime::Runtime &runtime,
      Services::SessionLockState::NativeLockStateMonitor &monitor,
      std::function<bool()> admission);
  ~NativeSleepComposition();
  bool start();
  void stop();
private:
  class Private;
  std::unique_ptr<Private> d;
};
}
