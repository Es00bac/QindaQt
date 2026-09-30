// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/session_lock_state/qt_native_lock_transport.h"
#include "qindaqt/compositor_names/compositor_names.h"
#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QTimer>
#include <QUuid>
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
  const auto receiptNonces = m_stateReceipts.keys();
  for (const auto &nonce : receiptNonces)
    failState(nonce, QStringLiteral("native-lock-transport-stopped"));
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
  bool connected = m_bus.connect(
      owner, path(), interface(), QStringLiteral("lockedChanged"),
      QStringLiteral("b"), this, SLOT(nativeChanged(bool,QDBusMessage)));
  connected = m_bus.connect(
                  owner, path(), interface(),
                  QStringLiteral("protectedChanged"), QStringLiteral("b"),
                  this, SLOT(nativeChanged(bool,QDBusMessage))) &&
              connected;
  if (!connected) {
    m_bus.disconnect(owner, path(), interface(),
                     QStringLiteral("lockedChanged"), QStringLiteral("b"),
                     this, SLOT(nativeChanged(bool,QDBusMessage)));
    m_bus.disconnect(owner, path(), interface(),
                     QStringLiteral("protectedChanged"), QStringLiteral("b"),
                     this, SLOT(nativeChanged(bool,QDBusMessage)));
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
                   QStringLiteral("b"), this,
                   SLOT(nativeChanged(bool,QDBusMessage)));
  m_bus.disconnect(owner, path(), interface(),
                   QStringLiteral("protectedChanged"), QStringLiteral("b"),
                   this, SLOT(nativeChanged(bool,QDBusMessage)));
}
void QtNativeLockTransport::nativeChanged(bool value,
                                          const QDBusMessage &message) {
  const auto args = message.arguments();
  if (!m_started || m_signalOwner.isEmpty() ||
      message.type() != QDBusMessage::SignalMessage ||
      message.service() != m_signalOwner || message.path() != path() ||
      message.interface() != interface() ||
      (message.member() != QStringLiteral("lockedChanged") &&
       message.member() != QStringLiteral("protectedChanged")) ||
      message.signature() != QStringLiteral("b") || args.size() != 1 ||
      args.first().metaType() != QMetaType::fromType<bool>() ||
      args.first().toBool() != value)
    return;
  Q_EMIT stateInvalidated(message.service());
}

void QtNativeLockTransport::failState(const QString &nonce,
                                      const QString &error) {
  const auto it = m_stateReceipts.find(nonce);
  if (it == m_stateReceipts.end())
    return;
  const auto request = it.value();
  m_stateReceipts.erase(it);
  m_bus.disconnect(request.owner, path(), interface(),
                   QStringLiteral("stateReceipt"), QStringList{nonce},
                   QStringLiteral("sbb"), this,
                   SLOT(nativeStateReceipt(QString,bool,bool,QDBusMessage)));
  Q_EMIT failed(request.generation, request.serial, request.owner, error);
}

void QtNativeLockTransport::maybeFinishState(const QString &nonce) {
  const auto it = m_stateReceipts.find(nonce);
  if (it == m_stateReceipts.end() || !it->replyReady || !it->receiptReady)
    return;
  const auto request = it.value();
  if (!m_started || request.lifetime != m_lifetime ||
      request.owner != m_signalOwner) {
    failState(nonce, QStringLiteral("native-lock-receipt-generation-changed"));
    return;
  }
  m_stateReceipts.erase(it);
  m_bus.disconnect(request.owner, path(), interface(),
                   QStringLiteral("stateReceipt"), QStringList{nonce},
                   QStringLiteral("sbb"), this,
                   SLOT(nativeStateReceipt(QString,bool,bool,QDBusMessage)));
  Q_EMIT stateResolved(request.generation, request.serial, request.owner,
                       request.locked, request.protectedPresentation);
}

void QtNativeLockTransport::nativeStateReceipt(
    const QString &nonce, bool locked, bool protectedPresentation,
    const QDBusMessage &message) {
  auto it = m_stateReceipts.find(nonce);
  if (it == m_stateReceipts.end())
    return;
  const auto args = message.arguments();
  if (message.type() != QDBusMessage::SignalMessage ||
      message.service() != it->owner || message.path() != path() ||
      message.interface() != interface() ||
      message.member() != QStringLiteral("stateReceipt") ||
      message.signature() != QStringLiteral("sbb") || args.size() != 3 ||
      args.at(0).metaType() != QMetaType::fromType<QString>() ||
      args.at(1).metaType() != QMetaType::fromType<bool>() ||
      args.at(2).metaType() != QMetaType::fromType<bool>() ||
      args.at(0).toString() != nonce || args.at(1).toBool() != locked ||
      args.at(2).toBool() != protectedPresentation || it->receiptReady) {
    failState(nonce, QStringLiteral("malformed-native-lock-state-receipt"));
    return;
  }
  it->receiptReady = true;
  it->locked = locked;
  it->protectedPresentation = protectedPresentation;
  maybeFinishState(nonce);
}

void QtNativeLockTransport::requestState(quint64 generation, quint64 serial,
                                         const QString &owner) {
  if (!m_started || owner != m_signalOwner) {
    Q_EMIT failed(generation, serial, owner,
                  QStringLiteral("native-lock-state-owner-not-subscribed"));
    return;
  }
  QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
  nonce.remove(QLatin1Char('-'));
  nonce = nonce.toLower();
  if (!m_bus.connect(
          owner, path(), interface(), QStringLiteral("stateReceipt"),
          QStringList{nonce}, QStringLiteral("sbb"), this,
          SLOT(nativeStateReceipt(QString,bool,bool,QDBusMessage)))) {
    Q_EMIT failed(generation, serial, owner,
                  QStringLiteral("native-lock-state-receipt-subscribe-failed"));
    return;
  }
  StateReceipt state;
  state.generation = generation;
  state.serial = serial;
  state.lifetime = m_lifetime;
  state.owner = owner;
  m_stateReceipts.insert(nonce, state);

  auto request = QDBusMessage::createMethodCall(
      owner, path(), interface(), QStringLiteral("RequestStateWithReceipt"));
  request << nonce;
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 1500), this);
  m_pending.append(watcher);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, nonce] {
            m_pending.removeAll(watcher);
            const auto it = m_stateReceipts.find(nonce);
            if (it != m_stateReceipts.end()) {
              const auto reply = watcher->reply();
              if (reply.type() != QDBusMessage::ReplyMessage ||
                  !reply.signature().isEmpty() || !reply.arguments().isEmpty()) {
                failState(nonce, QStringLiteral("native-lock-state-request-failed"));
              } else {
                it->replyReady = true;
                maybeFinishState(nonce);
              }
            }
            watcher->deleteLater();
          });
  QTimer::singleShot(1500, this, [this, nonce] {
    if (m_stateReceipts.contains(nonce))
      failState(nonce, QStringLiteral("native-lock-state-receipt-timeout"));
  });
}
} // namespace QindaQt::Services::SessionLockState
