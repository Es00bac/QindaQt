// SPDX-License-Identifier: GPL-3.0-or-later
#include "lock_objects_p.h"
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QUuid>
#include <unistd.h>
#include <utility>
namespace QindaQt::Services::NativeLock {
using SessionLockState::LockState;
LockSnapshot::LockSnapshot(SessionLockState::NativeLockStateMonitor &monitor,
                           QObject *parent)
    : QObject(parent),
      epoch(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_monitor(monitor) {
  connect(&monitor, &SessionLockState::NativeLockStateMonitor::stateChanged,
          this, [this] { update(); });
  connect(&monitor,
          &SessionLockState::NativeLockStateMonitor::protectionChanged, this,
          [this] { update(); });
  update();
}
void LockSnapshot::restart() {
  epoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
  revision = 1;
  update();
  Q_EMIT changed(epoch, revision);
}
void LockSnapshot::update() {
  const auto state = m_monitor.state();
  const bool protectedPresentation =
      state == LockState::Locked && m_monitor.presentationProtected();
  if (state == m_state && protectedPresentation == m_protected)
    return;
  m_state = state;
  m_protected = protectedPresentation;
  if (++revision == 0) {
    epoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
    revision = 1;
  }
  Q_EMIT changed(epoch, revision);
  // Unknown never emits false. Legacy consumers must call GetActive and handle
  // Unavailable; only a fresh admitted Unlocked snapshot authorizes false.
  if (m_monitor.state() != state ||
      m_monitor.presentationProtected() != protectedPresentation) {
    update();
    return;
  }
  if (state != LockState::Unknown)
    Q_EMIT activeChanged(state != LockState::Unlocked);
}
LockState LockSnapshot::state() {
  update();
  return m_state;
}
QVariantMap LockSnapshot::get() {
  update();
  QString state = QStringLiteral("unknown");
  if (m_state == LockState::Unlocked)
    state = QStringLiteral("unlocked");
  else if (m_state == LockState::Locking)
    state = QStringLiteral("locking");
  else if (m_state == LockState::Locked)
    state = QStringLiteral("locked");
  return {{QStringLiteral("version"), uint(1)},
          {QStringLiteral("epoch"), epoch},
          {QStringLiteral("revision"), QVariant::fromValue(revision)},
          {QStringLiteral("state"), state},
          {QStringLiteral("available"), m_state != LockState::Unknown},
          {QStringLiteral("protected"), m_protected}};
}
LockObjectBase::LockObjectBase(QDBusConnection bus, ManualLockGateway &gateway,
                               QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_gateway(gateway) {}
bool LockObjectBase::authorized() {
  if (!calledFromDBus())
    return false;
  const auto sender = message().service();
  auto query =
      QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                     QStringLiteral("/org/freedesktop/DBus"),
                                     QStringLiteral("org.freedesktop.DBus"),
                                     QStringLiteral("GetConnectionUnixUser"));
  query << sender;
  const QDBusReply<uint> uid = m_bus.call(query, QDBus::Block, 250);
  if (!sender.startsWith(QLatin1Char(':')) || !uid.isValid() ||
      uid.value() != uint(geteuid())) {
    sendErrorReply(
        QStringLiteral("org.qindaqt.Lock1.Error.AccessDenied"),
        QStringLiteral("Caller is not a live same-user session peer"));
    return false;
  }
  return true;
}
void LockObjectBase::unsupported() {
  if (authorized())
    sendErrorReply(
        QStringLiteral("org.qindaqt.Lock1.Error.Unsupported"),
        QStringLiteral("This operation has no qualified native consumer"));
}
void LockObjectBase::request(bool nativeReply) {
  if (!authorized())
    return;
  const auto call = message();
  setDelayedReply(true);
  const auto connection = m_bus;
  if (!m_gateway.submit([call, connection, nativeReply](RequestResult result) {
        if (result == RequestResult::Uncertain)
          connection.send(call.createErrorReply(
              QStringLiteral("org.qindaqt.Lock1.Error.Uncertain"),
              QStringLiteral(
                  "Lock admission is unavailable or uncertain; not replayed")));
        else if (nativeReply)
          connection.send(call.createReply(result == RequestResult::Admitted));
        else if (result == RequestResult::Admitted)
          connection.send(call.createReply());
        else
          connection.send(call.createErrorReply(
              QStringLiteral("org.qindaqt.Lock1.Error.Rejected"),
              QStringLiteral("Compositor rejected lock admission")));
      }))
    connection.send(call.createErrorReply(
        QStringLiteral("org.qindaqt.Lock1.Error.Busy"),
        QStringLiteral("A manual lock request is already pending")));
}
NativeLockObject::NativeLockObject(QDBusConnection bus,
                                   ManualLockGateway &gateway,
                                   LockSnapshot &snapshot, QObject *parent)
    : LockObjectBase(std::move(bus), gateway, parent), m_snapshot(snapshot) {
  connect(&snapshot, &LockSnapshot::changed, this, &NativeLockObject::Changed);
}
QVariantMap NativeLockObject::GetState() {
  return authorized() ? m_snapshot.get() : QVariantMap{};
}
void NativeLockObject::RequestLock() { request(true); }
ScreenSaverObject::ScreenSaverObject(QDBusConnection bus,
                                     ManualLockGateway &gateway,
                                     LockSnapshot &snapshot, QObject *parent)
    : LockObjectBase(std::move(bus), gateway, parent), m_snapshot(snapshot) {
  connect(&snapshot, &LockSnapshot::activeChanged, this,
          &ScreenSaverObject::ActiveChanged);
}
void ScreenSaverObject::Lock() { request(false); }
bool ScreenSaverObject::GetActive() {
  if (!authorized())
    return false;
  const auto state = m_snapshot.state();
  if (state == LockState::Unknown) {
    sendErrorReply(QStringLiteral("org.qindaqt.Lock1.Error.Unavailable"),
                   QStringLiteral("Native lock state is unknown"));
    return false;
  }
  return state != LockState::Unlocked;
}
bool ScreenSaverObject::SetActive(bool active) {
  if (active)
    request(true);
  else
    unsupported();
  return false; // Delayed D-Bus reply carries the admission result.
}
uint ScreenSaverObject::GetActiveTime() {
  unsupported();
  return 0;
}
uint ScreenSaverObject::GetSessionIdleTime() {
  unsupported();
  return 0;
}
uint ScreenSaverObject::Throttle(const QString &, const QString &) {
  unsupported();
  return 0;
}
void ScreenSaverObject::UnThrottle(uint) { unsupported(); }
void ScreenSaverObject::SimulateUserActivity() { unsupported(); }
uint ScreenSaverObject::Inhibit(const QString &, const QString &) {
  unsupported();
  return 0;
}
void ScreenSaverObject::UnInhibit(uint) { unsupported(); }
} // namespace QindaQt::Services::NativeLock
