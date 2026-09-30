// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "manual_lock_gateway_p.h"
#include "qindaqt/services/session_lock_state/native_lock_state_monitor.h"
#include <QDBusConnection>
#include <QDBusContext>
#include <QVariantMap>
namespace QindaQt::Services::NativeLock {
class LockSnapshot final : public QObject {
  Q_OBJECT
public:
  explicit LockSnapshot(SessionLockState::NativeLockStateMonitor &monitor,
                        QObject *parent = nullptr);
  QVariantMap get();
  void restart();
  SessionLockState::LockState state();
  QString epoch;
  quint64 revision = 1;
Q_SIGNALS:
  void changed(const QString &epoch, quint64 revision);
  void activeChanged(bool active);

private:
  void update();
  SessionLockState::NativeLockStateMonitor &m_monitor;
  SessionLockState::LockState m_state = SessionLockState::LockState::Unknown;
  bool m_protected = false;
};
class LockObjectBase : public QObject, protected QDBusContext {
  Q_OBJECT
public:
  LockObjectBase(QDBusConnection bus, ManualLockGateway &gateway,
                 QObject *parent = nullptr);

protected:
  bool authorized();
  void unsupported();
  void request(bool nativeReply);
  QDBusConnection m_bus;
  ManualLockGateway &m_gateway;
};
class NativeLockObject final : public LockObjectBase {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Lock1")
  Q_CLASSINFO(
      "D-Bus Introspection",
      "<interface name=\"org.qindaqt.Lock1\"><method name=\"GetState\"><arg "
      "direction=\"out\" type=\"a{sv}\" name=\"state\"/></method><method "
      "name=\"RequestLock\"><arg direction=\"out\" type=\"b\" "
      "name=\"admitted\"/></method><signal name=\"Changed\"><arg type=\"s\" "
      "name=\"epoch\"/><arg type=\"t\" "
      "name=\"revision\"/></signal></interface>")
public:
  NativeLockObject(QDBusConnection bus, ManualLockGateway &gateway,
                   LockSnapshot &snapshot, QObject *parent = nullptr);
public Q_SLOTS:
  QVariantMap GetState();
  void RequestLock();
Q_SIGNALS:
  void Changed(const QString &epoch, quint64 revision);

private:
  LockSnapshot &m_snapshot;
};
class ScreenSaverObject final : public LockObjectBase {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")
public:
  ScreenSaverObject(QDBusConnection bus, ManualLockGateway &gateway,
                    LockSnapshot &snapshot, QObject *parent = nullptr);
public Q_SLOTS:
  void Lock();
  bool GetActive();
  bool SetActive(bool active);
  uint GetActiveTime();
  uint GetSessionIdleTime();
  uint Throttle(const QString &application, const QString &reason);
  void UnThrottle(uint cookie);
  void SimulateUserActivity();
  uint Inhibit(const QString &application, const QString &reason);
  void UnInhibit(uint cookie);
Q_SIGNALS:
  void ActiveChanged(bool active);

private:
  LockSnapshot &m_snapshot;
};
} // namespace QindaQt::Services::NativeLock
