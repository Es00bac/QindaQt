// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/network_qt_transport/qt_network_transport.h>

#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_redaction.h>

#include <QtCore/QMetaType>
#include <QtCore/QTimer>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::Network::Client {
namespace {

QString normalizedError(const QDBusError &error) {
  switch (error.type()) {
  case QDBusError::NoReply:
  case QDBusError::Timeout:
    return QStringLiteral("transport-timeout");
  case QDBusError::ServiceUnknown:
  case QDBusError::Disconnected:
    return QStringLiteral("owner-unavailable");
  case QDBusError::InvalidArgs:
  case QDBusError::InvalidSignature:
    return QStringLiteral("malformed-reply");
  default:
    return QStringLiteral("transport-error");
  }
}

bool hasOnlyKeys(const QVariantMap &parameters,
                 std::initializer_list<QStringView> allowed) {
  if (parameters.size() != static_cast<qsizetype>(allowed.size())) {
    return false;
  }
  for (const QString &key : parameters.keys()) {
    bool found = false;
    for (const QStringView candidate : allowed) {
      if (key == candidate) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

} // namespace

class QtNetworkTransport::Private final {
public:
  Private(const QDBusConnection &bus, QString name)
      : connection(bus), serviceName(std::move(name)) {}

  QDBusConnection connection;
  QString serviceName;
  QString owner;
  quint64 ownerGeneration = 0;
  std::unique_ptr<QDBusServiceWatcher> watcher;
  QTimer activationRetry;
  bool running = false;
  bool localDisconnectBound = false;
  bool activationPending = false;
};

QtNetworkTransport::QtNetworkTransport(const QDBusConnection &connection,
                                       QString serviceName, QObject *parent)
    : NetworkTransport(parent),
      d(std::make_unique<Private>(
          connection, serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                            : std::move(serviceName))) {
  d->activationRetry.setSingleShot(true);
  d->activationRetry.setInterval(1'000);
  connect(&d->activationRetry, &QTimer::timeout, this,
          &QtNetworkTransport::requestActivation);
}

QtNetworkTransport::~QtNetworkTransport() { stop(); }

bool QtNetworkTransport::start(QString *error) {
  if (d->running) {
    if (error != nullptr) {
      error->clear();
    }
    return true;
  }
  if (!d->connection.isConnected() || d->connection.interface() == nullptr) {
    if (error != nullptr) {
      *error = QStringLiteral("network bus is unavailable");
    }
    return false;
  }
  d->running = true;
  d->watcher = std::make_unique<QDBusServiceWatcher>(
      d->serviceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange,
      this);
  connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
          &QtNetworkTransport::onServiceOwnerChanged);
  d->localDisconnectBound = d->connection.connect(
      QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
      QStringLiteral("org.freedesktop.DBus.Local"),
      QStringLiteral("Disconnected"), this, SLOT(onBusDisconnected()));
  if (!d->localDisconnectBound) {
    stop();
    if (error != nullptr) {
      *error = QStringLiteral("network bus lifetime binding failed");
    }
    return false;
  }
  queryInitialOwner();
  requestActivation();
  if (error != nullptr) {
    error->clear();
  }
  return true;
}

void QtNetworkTransport::stop() {
  if (!d->running) {
    return;
  }
  d->running = false;
  d->activationRetry.stop();
  d->activationPending = false;
  ++d->ownerGeneration;
  setOwner({});
  d->watcher.reset();
  if (d->localDisconnectBound) {
    d->connection.disconnect(
        QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
        QStringLiteral("org.freedesktop.DBus.Local"),
        QStringLiteral("Disconnected"), this, SLOT(onBusDisconnected()));
    d->localDisconnectBound = false;
  }
}

void QtNetworkTransport::queryInitialOwner() {
  const quint64 generation = d->ownerGeneration;
  QDBusMessage call = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
  call.setArguments({d->serviceName});
  auto *watcher =
      new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation](QDBusPendingCallWatcher *) {
            const QDBusPendingReply<QString> reply = *watcher;
            watcher->deleteLater();
            // AGENT-GUARD: The owner watcher can overtake this query. A
            // generation mismatch means the query is stale and must not
            // roll exact authority back to an earlier unique name.
            if (!d->running || generation != d->ownerGeneration) {
              return;
            }
            setOwner(reply.isError() ? QString{} : reply.value());
          });
}

void QtNetworkTransport::requestActivation() {
  if (!d->running || d->activationPending || !d->owner.isEmpty()) {
    return;
  }
  d->activationPending = true;
  QDBusMessage call =
      QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                     QStringLiteral("/org/freedesktop/DBus"),
                                     QStringLiteral("org.freedesktop.DBus"),
                                     QStringLiteral("StartServiceByName"));
  call.setArguments({d->serviceName, quint32(0)});
  auto *watcher =
      new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher](QDBusPendingCallWatcher *) {
            watcher->deleteLater();
            d->activationPending = false;
            if (d->running) {
              queryInitialOwner();
              if (d->owner.isEmpty()) {
                // Activation failures are transient, including restart races.
                // Keep one broker request in flight and retry at a bounded
                // rate.
                d->activationRetry.start();
              }
            }
          });
}

void QtNetworkTransport::onServiceOwnerChanged(const QString &service,
                                               const QString &oldOwner,
                                               const QString &newOwner) {
  Q_UNUSED(oldOwner)
  if (d->running && service == d->serviceName) {
    ++d->ownerGeneration;
    setOwner(newOwner);
    if (newOwner.isEmpty()) {
      // AGENT-CONTRACT: Owner loss retires the public service epoch.
      // Ask the broker for a fresh activated process; N0 will accept its
      // new unique owner and strictly newer boot-monotonic epoch.
      requestActivation();
    }
  }
}

void QtNetworkTransport::setOwner(const QString &owner) {
  if (owner == d->owner) {
    return;
  }
  if (!d->owner.isEmpty()) {
    d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                             QString::fromLatin1(kInterfaceName),
                             QStringLiteral("Changed"), this,
                             SLOT(onChanged(quint64, quint64)));
  }
  d->owner = owner;
  if (!d->owner.isEmpty()) {
    d->activationRetry.stop();
    d->connection.connect(d->owner, QString::fromLatin1(kObjectPath),
                          QString::fromLatin1(kInterfaceName),
                          QStringLiteral("Changed"), this,
                          SLOT(onChanged(quint64, quint64)));
  }
  Q_EMIT ownerChanged(d->owner);
}

void QtNetworkTransport::onChanged(const quint64 epoch,
                                   const quint64 revision) {
  Q_UNUSED(epoch)
  Q_UNUSED(revision)
  if (d->running && !d->owner.isEmpty()) {
    Q_EMIT snapshotInvalidated(d->owner);
  }
}

void QtNetworkTransport::onBusDisconnected() {
  if (!d->running) {
    return;
  }
  d->running = false;
  d->activationRetry.stop();
  d->activationPending = false;
  ++d->ownerGeneration;
  d->owner.clear();
  d->watcher.reset();
  d->localDisconnectBound = false;
  Q_EMIT busDisconnected();
}

void QtNetworkTransport::requestSnapshot(const quint64 token,
                                         const QString &owner) {
  if (!d->running || owner.isEmpty() || owner != d->owner) {
    fail(token, owner, QStringLiteral("owner-unavailable"));
    return;
  }
  QDBusMessage call = QDBusMessage::createMethodCall(
      owner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), QStringLiteral("GetSnapshot"));
  auto *watcher =
      new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, owner](QDBusPendingCallWatcher *) {
            const QDBusPendingReply<QByteArray> reply = *watcher;
            watcher->deleteLater();
            if (reply.isError()) {
              fail(token, owner, normalizedError(reply.error()));
              return;
            }
            Q_EMIT snapshotReceived(token, owner, reply.value());
          });
}

void QtNetworkTransport::requestOperation(const quint64 token,
                                          const QString &owner,
                                          const quint64 epoch,
                                          const quint64 revision,
                                          const OperationKind kind,
                                          const QVariantMap &parameters) {
  if (!d->running || owner.isEmpty() || owner != d->owner) {
    fail(token, owner, QStringLiteral("owner-unavailable"));
    return;
  }
  if (wireContainsSecrets(parameters)) {
    fail(token, owner, QStringLiteral("operation-parameters-contain-secrets"));
    return;
  }

  QString method;
  QList<QVariant> arguments{epoch, revision};
  switch (kind) {
  case OperationKind::RequestScan:
    if (!hasOnlyKeys(parameters, {u"deadlineMs"}) ||
        parameters.value(QStringLiteral("deadlineMs")).metaType().id() !=
            QMetaType::LongLong) {
      fail(token, owner, QStringLiteral("operation-parameters-invalid"));
      return;
    }
    method = QStringLiteral("RequestScan");
    arguments.append(parameters.value(QStringLiteral("deadlineMs")));
    break;
  case OperationKind::ConnectKnownNetwork:
    if (!hasOnlyKeys(parameters, {u"knownNetworkId"}) ||
        parameters.value(QStringLiteral("knownNetworkId")).metaType().id() !=
            QMetaType::QString) {
      fail(token, owner, QStringLiteral("operation-parameters-invalid"));
      return;
    }
    method = QStringLiteral("ConnectKnownNetwork");
    arguments.append(parameters.value(QStringLiteral("knownNetworkId")));
    break;
  case OperationKind::DisconnectActive:
    if (!hasOnlyKeys(parameters, {u"deviceInterface"}) ||
        parameters.value(QStringLiteral("deviceInterface")).metaType().id() !=
            QMetaType::QString) {
      fail(token, owner, QStringLiteral("operation-parameters-invalid"));
      return;
    }
    method = QStringLiteral("DisconnectActive");
    arguments.append(parameters.value(QStringLiteral("deviceInterface")));
    break;
  case OperationKind::SetRadio:
    if (!hasOnlyKeys(parameters, {u"radioKind", u"enable"}) ||
        parameters.value(QStringLiteral("radioKind")).metaType().id() !=
            QMetaType::Int ||
        parameters.value(QStringLiteral("enable")).metaType().id() !=
            QMetaType::Bool) {
      fail(token, owner, QStringLiteral("operation-parameters-invalid"));
      return;
    }
    method = QStringLiteral("SetRadio");
    arguments.append(static_cast<quint32>(
        parameters.value(QStringLiteral("radioKind")).toInt()));
    arguments.append(parameters.value(QStringLiteral("enable")));
    break;
  }

  QDBusMessage call = QDBusMessage::createMethodCall(
      owner, QString::fromLatin1(kObjectPath),
      QString::fromLatin1(kInterfaceName), method);
  call.setArguments(arguments);
  auto *watcher =
      new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, owner](QDBusPendingCallWatcher *) {
            const QDBusPendingReply<QByteArray> reply = *watcher;
            watcher->deleteLater();
            if (reply.isError()) {
              fail(token, owner, normalizedError(reply.error()));
              return;
            }
            Q_EMIT operationReceived(token, owner, reply.value());
          });
}

void QtNetworkTransport::fail(const quint64 token, const QString &owner,
                              const QString &reason) {
  const QString bounded = redactDiagnostic(reason);
  Q_EMIT requestFailed(token, owner, bounded, bounded);
}

} // namespace QindaQt::Network::Client
