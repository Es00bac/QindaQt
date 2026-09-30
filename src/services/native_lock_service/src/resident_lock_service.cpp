// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/native_lock_service/resident_lock_service.h"
#include "lock_objects_p.h"
#include <QDBusError>
#include <utility>
namespace QindaQt::Services::NativeLock {
class ResidentLockService::Private {
public:
  Private(QDBusConnection connection,
          SessionLockState::NativeLockStateMonitor &monitor,
          NativeLockRequest &request)
      : bus(std::move(connection)), gateway(request), snapshot(monitor),
        native(bus, gateway, snapshot), screenSaver(bus, gateway, snapshot) {}
  QDBusConnection bus;
  ManualLockGateway gateway;
  LockSnapshot snapshot;
  NativeLockObject native;
  ScreenSaverObject screenSaver;
  QStringList paths, names;
  bool started = false;
};
ResidentLockService::ResidentLockService(
    QDBusConnection bus, SessionLockState::NativeLockStateMonitor &monitor,
    NativeLockRequest &request, QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(std::move(bus), monitor, request)) {}
ResidentLockService::~ResidentLockService() { stop(); }
bool ResidentLockService::start(QString *error) {
  if (d->started)
    return true;
  const auto fail = [this, error] {
    if (error)
      *error = d->bus.lastError().message();
    stop();
    return false;
  };
  if (!d->bus.isConnected())
    return fail();
  const QStringList paths = {QStringLiteral("/org/qindaqt/Lock1"),
                             QStringLiteral("/ScreenSaver"),
                             QStringLiteral("/org/freedesktop/ScreenSaver")};
  for (const auto &path : paths) {
    QObject *object = path == paths.first()
                          ? static_cast<QObject *>(&d->native)
                          : static_cast<QObject *>(&d->screenSaver);
    if (!d->bus.registerObject(path, object,
                               QDBusConnection::ExportAllSlots |
                                   QDBusConnection::ExportAllSignals))
      return fail();
    d->paths.append(path);
  }
  for (const auto &name : {QStringLiteral("org.qindaqt.Lock1"),
                           QStringLiteral("org.freedesktop.ScreenSaver"),
                           QStringLiteral("org.kde.screensaver")}) {
    if (!d->bus.registerService(name))
      return fail();
    d->names.append(name);
  }
  d->snapshot.restart();
  d->started = true;
  return true;
}
void ResidentLockService::stop() {
  d->started = false;
  d->gateway.cancel();
  for (const auto &name : std::as_const(d->names))
    d->bus.unregisterService(name);
  d->names.clear();
  for (const auto &path : std::as_const(d->paths))
    d->bus.unregisterObject(path);
  d->paths.clear();
}
} // namespace QindaQt::Services::NativeLock
