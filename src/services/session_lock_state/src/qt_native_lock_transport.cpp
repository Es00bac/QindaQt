// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/session_lock_state/qt_native_lock_transport.h"
#include "qindaqt/compositor_names/compositor_names.h"
#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <utility>
namespace QindaQt::Services::SessionLockState {
namespace {
QString service() { return QString(CompositorNames::service); }
QString path() { return QString(CompositorNames::nativeLockPath); }
QString interface() { return QString(CompositorNames::nativeLockInterface); }
QDBusMessage daemon(const QString &method, const QString &argument) {
  auto message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), method);
  message << argument;
  return message;
}
} // namespace
QtNativeLockTransport::QtNativeLockTransport(QDBusConnection bus,
                                             QObject *parent)
    : NativeLockTransport(parent), m_bus(std::move(bus)) {}
QtNativeLockTransport::~QtNativeLockTransport() { stop(); }
bool QtNativeLockTransport::start(QString *error) {
  if (m_started)
    return true;
  if (!m_bus.isConnected()) {
    if (error)
      *error = QStringLiteral("native lock bus is disconnected");
    return false;
  }
  m_watcher = new QDBusServiceWatcher(
      service(), m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &) {
            if (m_started)
              Q_EMIT ownerChanged();
          });
  if (!m_bus.connect({}, QStringLiteral("/org/freedesktop/DBus/Local"),
                     QStringLiteral("org.freedesktop.DBus.Local"),
                     QStringLiteral("Disconnected"), this, SLOT(busLost()))) {
    delete m_watcher;
    m_watcher = nullptr;
    if (error)
      *error = QStringLiteral("cannot observe native lock bus loss");
    return false;
  }
  ++m_lifetime;
  m_started = true;
  return true;
}
void QtNativeLockTransport::stop() {
  m_started = false;
  ++m_lifetime;
  unsubscribe();
  m_bus.disconnect({}, QStringLiteral("/org/freedesktop/DBus/Local"),
                   QStringLiteral("org.freedesktop.DBus.Local"),
                   QStringLiteral("Disconnected"), this, SLOT(busLost()));
  for (auto *pending : std::as_const(m_pending)) {
    pending->disconnect(this);
    pending->deleteLater();
  }
  m_pending.clear();
  delete m_watcher;
  m_watcher = nullptr;
}
void QtNativeLockTransport::busLost() {
  if (m_started) {
    stop();
    Q_EMIT lost();
  }
}
void QtNativeLockTransport::call(
    const QDBusMessage &message,
    std::function<void(const QDBusMessage &)> completion) {
  if (!m_started)
    return;
  const auto lifetime = m_lifetime;
  auto *watcher =
      new QDBusPendingCallWatcher(m_bus.asyncCall(message, 2000), this);
  m_pending.append(watcher);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, lifetime, completion = std::move(completion)] {
            m_pending.removeAll(watcher);
            if (m_started && lifetime == m_lifetime)
              completion(watcher->reply());
            watcher->deleteLater();
          });
}
void QtNativeLockTransport::requestOwner(quint64 generation) {
  call(daemon(QStringLiteral("GetNameOwner"), service()),
       [this, generation](const QDBusMessage &message) {
         const QDBusPendingReply<QString> reply(message);
         if (reply.isError()) {
           if (reply.error().name() ==
               QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner"))
             Q_EMIT ownerResolved(generation, {});
           else
             Q_EMIT failed(generation, 0, {}, reply.error().name());
         } else
           Q_EMIT ownerResolved(generation, reply.value());
       });
}
void QtNativeLockTransport::requestPid(quint64 generation,
                                       const QString &owner) {
  call(daemon(QStringLiteral("GetConnectionUnixProcessID"), owner),
       [this, generation, owner](const QDBusMessage &message) {
         const QDBusPendingReply<quint32> reply(message);
         if (reply.isError())
           Q_EMIT failed(generation, 0, owner, reply.error().name());
         else
           Q_EMIT pidResolved(generation, owner, reply.value());
       });
}
bool QtNativeLockTransport::subscribe(const QString &owner) {
  unsubscribe();
  if (!m_started || owner.isEmpty())
    return false;
  bool connected =
      m_bus.connect(owner, path(), interface(), QStringLiteral("lockedChanged"),
                    this, SLOT(nativeChanged(bool, QDBusMessage)));
  connected = m_bus.connect(owner, path(), interface(),
                            QStringLiteral("protectedChanged"), this,
                            SLOT(nativeChanged(bool, QDBusMessage))) &&
              connected;
  if (!connected) {
    m_bus.disconnect(owner, path(), interface(),
                     QStringLiteral("lockedChanged"), this,
                     SLOT(nativeChanged(bool, QDBusMessage)));
    m_bus.disconnect(owner, path(), interface(),
                     QStringLiteral("protectedChanged"), this,
                     SLOT(nativeChanged(bool, QDBusMessage)));
    return false;
  }
  m_signalOwner = owner;
  return true;
}
void QtNativeLockTransport::unsubscribe() {
  if (m_signalOwner.isEmpty())
    return;
  const auto owner = std::exchange(m_signalOwner, {});
  m_bus.disconnect(owner, path(), interface(), QStringLiteral("lockedChanged"),
                   this, SLOT(nativeChanged(bool, QDBusMessage)));
  m_bus.disconnect(owner, path(), interface(),
                   QStringLiteral("protectedChanged"), this,
                   SLOT(nativeChanged(bool, QDBusMessage)));
}
void QtNativeLockTransport::nativeChanged(bool, const QDBusMessage &message) {
  // Queued signals may outlive an old subscription. Retain the authenticated
  // sender from the actual message, never relabel it with the new owner.
  if (m_started && !m_signalOwner.isEmpty() &&
      message.service() == m_signalOwner)
    Q_EMIT stateInvalidated(message.service());
}
void QtNativeLockTransport::requestState(quint64 generation, quint64 serial,
                                         const QString &owner) {
  auto request = QDBusMessage::createMethodCall(
      owner, path(), QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("GetAll"));
  request << interface();
  call(request, [this, generation, serial, owner](const QDBusMessage &message) {
    const QDBusPendingReply<QVariantMap> reply(message);
    if (reply.isError()) {
      Q_EMIT failed(generation, serial, owner, reply.error().name());
      return;
    }
    const auto properties = reply.value();
    const auto locked = properties.value(QStringLiteral("Locked"));
    const auto protectedPresentation =
        properties.value(QStringLiteral("Protected"));
    if (locked.metaType() != QMetaType::fromType<bool>() ||
        protectedPresentation.metaType() != QMetaType::fromType<bool>()) {
      Q_EMIT failed(generation, serial, owner,
                    QStringLiteral("malformed-native-lock-properties"));
      return;
    }
    Q_EMIT stateResolved(generation, serial, owner, locked.toBool(),
                         protectedPresentation.toBool());
  });
}
} // namespace QindaQt::Services::SessionLockState
