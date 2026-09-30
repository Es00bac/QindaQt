// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/services/session_lock_state/native_lock_transport.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QList>
#include <QHash>
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
  void nativeStateReceipt(const QString &nonce, bool locked, bool protectedPresentation,
                         const QDBusMessage &message);
  void busLost();

private:
  struct StateReceipt {
    quint64 generation = 0;
    quint64 serial = 0;
    quint64 lifetime = 0;
    QString owner;
    bool replyReady = false;
    bool receiptReady = false;
    bool locked = false;
    bool protectedPresentation = false;
  };
  void maybeFinishState(const QString &nonce);
  void failState(const QString &nonce, const QString &error);
  void call(const QDBusMessage &message,
            std::function<void(const QDBusMessage &)> completion);
  QDBusConnection m_bus;
  QDBusServiceWatcher *m_watcher = nullptr;
  QList<QDBusPendingCallWatcher *> m_pending;
  QString m_signalOwner;
  QHash<QString, StateReceipt> m_stateReceipts;
  quint64 m_lifetime = 0;
  bool m_started = false;
};
} // namespace QindaQt::Services::SessionLockState
