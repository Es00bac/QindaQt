// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/services/session_lock_state/native_lock_transport.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QList>
#include <functional>
class QDBusServiceWatcher;
class QDBusPendingCallWatcher;
namespace QindaQt::Services::SessionLockState {
// QCoreApplication-compatible QtDBus transport. Owns watchers/subscriptions,
// borrows the connection handle by value, never connects to a display.
class QtNativeLockTransport final : public NativeLockTransport {
  Q_OBJECT
public:
  explicit QtNativeLockTransport(QDBusConnection connection,
                                 QObject *parent = nullptr);
  ~QtNativeLockTransport() override;
  bool start(QString *error = nullptr) override;
  void stop() override;
  void requestOwner(quint64 generation) override;
  void requestPid(quint64 generation, const QString &uniqueOwner) override;
  bool subscribe(const QString &uniqueOwner) override;
  void unsubscribe() override;
  void requestState(quint64 generation, quint64 serial,
                    const QString &uniqueOwner) override;
private Q_SLOTS:
  void nativeChanged(bool value, const QDBusMessage &message);
  void busLost();

private:
  void call(const QDBusMessage &message,
            std::function<void(const QDBusMessage &)> completion);
  QDBusConnection m_bus;
  QDBusServiceWatcher *m_watcher = nullptr;
  QList<QDBusPendingCallWatcher *> m_pending;
  QString m_signalOwner;
  quint64 m_lifetime = 0;
  bool m_started = false;
};
} // namespace QindaQt::Services::SessionLockState
