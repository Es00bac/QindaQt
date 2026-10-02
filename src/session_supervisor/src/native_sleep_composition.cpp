// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_sleep_composition.h"
#include <qindaqt/session/native_sleep/logind_sleep_transport.h>
#include <qindaqt/session/native_sleep/sleep_coordinator.h>
#include <qindaqt/session/native_sleep/sleep_service.h>
#include <QProcessEnvironment>
#include <unistd.h>
namespace QindaQt::SessionSupervisor {
class NativeSleepComposition::Private final {
public:
  Private(QDBusConnection sessionBus, Session::NativeLockRuntime::Runtime &runtime,
      Services::SessionLockState::NativeLockStateMonitor &monitor,
      std::function<bool()> admission)
      : transport(QDBusConnection::systemBus(),
            QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_SESSION_ID")),
            static_cast<quint32>(getuid()), static_cast<quint32>(getpid()), std::move(admission)),
        coordinator(transport, runtime, monitor),
        service(std::move(sessionBus), coordinator, static_cast<quint32>(getuid())) {}
  Session::NativeSleep::LogindSleepTransport transport;
  Session::NativeSleep::SleepCoordinator coordinator;
  Session::NativeSleep::SleepService service;
};
NativeSleepComposition::NativeSleepComposition(QDBusConnection sessionBus,
    Session::NativeLockRuntime::Runtime &runtime,
    Services::SessionLockState::NativeLockStateMonitor &monitor,
    std::function<bool()> admission)
    : d(std::make_unique<Private>(std::move(sessionBus), runtime, monitor, std::move(admission))) {}
NativeSleepComposition::~NativeSleepComposition() { stop(); }
bool NativeSleepComposition::start() {
  if (!d->service.start()) return false;
  d->coordinator.start();
  return true;
}
Session::NativeSleep::SleepCoordinator &NativeSleepComposition::coordinator() { return d->coordinator; }
void NativeSleepComposition::stop() {
  d->coordinator.stop();
  d->service.stop();
}
}
