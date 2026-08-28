// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_client/qt_display_transport.h>

#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_protocol/display_limits.h>

#include <QtCore/QMetaObject>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::DisplayClient {
namespace {

QString normalizedError(const QDBusError &error) {
  if (error.type() == QDBusError::NoReply ||
      error.type() == QDBusError::Timeout ||
      error.type() == QDBusError::TimedOut) {
    return QStringLiteral("transport-timeout");
  }
  if (error.type() == QDBusError::ServiceUnknown ||
      error.type() == QDBusError::Disconnected) {
    return QStringLiteral("owner-unavailable");
  }
  if (error.type() == QDBusError::InvalidArgs ||
      error.type() == QDBusError::InvalidSignature) {
    return QStringLiteral("malformed-reply");
  }
  if (error.name() ==
      QStringLiteral("org.qindaqt.Display1.Error.Unavailable")) {
    return QStringLiteral("service-unavailable");
  }
  return QStringLiteral("transport-error");
}

} // namespace

class QtDisplayTransport::Private {
public:
  Private(const QDBusConnection &bus, QString name)
      : connection(bus), serviceName(std::move(name)) {}

  QDBusConnection connection;
  QString serviceName;
  QString owner;
  quint64 ownerGeneration = 0;
  std::unique_ptr<QDBusServiceWatcher> watcher;
  bool running = false;
  bool activationInFlight = false;
};

QtDisplayTransport::QtDisplayTransport(const QDBusConnection &connection,
                                       QString serviceName, QObject *parent)
    : DisplayTransport(parent),
      d(std::make_unique<Private>(
          connection, serviceName.isEmpty()
                          ? QString::fromLatin1(Display::kServiceName)
                          : std::move(serviceName))) {
  Display::registerDBusTypes();
}

QtDisplayTransport::~QtDisplayTransport() { stop(); }

void QtDisplayTransport::start() {
  if (d->running) {
    return;
  }
  d->running = true;
  d->watcher = std::make_unique<QDBusServiceWatcher>(
      d->serviceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange,
      this);
  connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
          &QtDisplayTransport::onServiceOwnerChanged);
  d->connection.connect(
      QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
      QStringLiteral("org.freedesktop.DBus.Local"),
      QStringLiteral("Disconnected"), this, SLOT(onBusDisconnected()));
  queryInitialOwner();
}

void QtDisplayTransport::stop() {
  if (!d->running) {
    return;
  }
  d->running = false;
  d->activationInFlight = false;
  ++d->ownerGeneration;
  setOwner({});
  d->connection.disconnect(
      QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
      QStringLiteral("org.freedesktop.DBus.Local"),
      QStringLiteral("Disconnected"), this, SLOT(onBusDisconnected()));
  d->watcher.reset();
}

void QtDisplayTransport::requestActivation() {
  if (!d->running || d->activationInFlight) {
    QMetaObject::invokeMethod(
        this,
        [this]() {
          if (d->running) {
            Q_EMIT activationFinished(false,
                                      QStringLiteral("activation-unavailable"));
          }
        },
        Qt::QueuedConnection);
    return;
  }

  d->activationInFlight = true;
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
            const QDBusMessage reply = watcher->reply();
            watcher->deleteLater();
            if (!d->running) {
              return;
            }
            d->activationInFlight = false;
            if (reply.type() == QDBusMessage::ErrorMessage) {
              Q_EMIT activationFinished(false,
                                        normalizedError(QDBusError(reply)));
            } else {
              Q_EMIT activationFinished(true, {});
              // AGENT-GUARD: activation success is not an owner baseline.
              // Resolve again and wait for exact-owner publication.
              queryInitialOwner();
            }
          });
}

void QtDisplayTransport::queryInitialOwner() {
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
            // AGENT-GUARD: A watcher notification may overtake the initial
            // owner query. Never let that stale reply roll transport
            // authority back to an older unique name (or to empty).
            if (!d->running || generation != d->ownerGeneration) {
              return;
            }
            const QString resolvedOwner =
                reply.isError() ? QString{} : reply.value();
            const bool unchanged = resolvedOwner == d->owner;
            setOwner(resolvedOwner);
            if (unchanged) {
              // Initial empty resolution is observable; otherwise Client
              // would remain Starting forever when Display1 is absent.
              Q_EMIT ownerChanged(d->owner);
            }
          });
}

void QtDisplayTransport::onServiceOwnerChanged(const QString &service,
                                               const QString &oldOwner,
                                               const QString &newOwner) {
  Q_UNUSED(oldOwner)
  if (d->running && service == d->serviceName) {
    ++d->ownerGeneration;
    setOwner(newOwner);
  }
}

void QtDisplayTransport::setOwner(const QString &owner) {
  if (owner == d->owner) {
    return;
  }
  if (!d->owner.isEmpty()) {
    d->connection.disconnect(
        d->owner, QString::fromLatin1(Display::kObjectPath),
        QString::fromLatin1(Display::kInterfaceName), QStringLiteral("Changed"),
        this, SLOT(onChanged(QString, quint64, bool)));
  }
  d->owner = owner;
  if (!d->owner.isEmpty()) {
    d->connection.connect(d->owner, QString::fromLatin1(Display::kObjectPath),
                          QString::fromLatin1(Display::kInterfaceName),
                          QStringLiteral("Changed"), this,
                          SLOT(onChanged(QString, quint64, bool)));
  }
  Q_EMIT ownerChanged(d->owner);
}

void QtDisplayTransport::onChanged(const QString &epoch, const quint64 revision,
                                   const bool available) {
  if (d->running && !d->owner.isEmpty()) {
    Q_EMIT invalidated(d->owner, epoch, revision, available);
  }
}

void QtDisplayTransport::onBusDisconnected() {
  if (!d->running) {
    return;
  }
  d->activationInFlight = false;
  ++d->ownerGeneration;
  setOwner({});
}

void QtDisplayTransport::fetchSnapshot(const QString &owner,
                                       const quint64 requestId) {
  if (!d->running || owner.isEmpty() || owner != d->owner) {
    QMetaObject::invokeMethod(
        this,
        [this, owner, requestId]() {
          if (d->running) {
            Q_EMIT snapshotReply(owner, requestId, false, {},
                                 QStringLiteral("owner-unavailable"));
          }
        },
        Qt::QueuedConnection);
    return;
  }
  const QDBusMessage call = QDBusMessage::createMethodCall(
      owner, QString::fromLatin1(Display::kObjectPath),
      QString::fromLatin1(Display::kInterfaceName),
      QStringLiteral("GetSnapshot"));
  auto *watcher =
      new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, owner, requestId](QDBusPendingCallWatcher *) {
            const QDBusMessage reply = watcher->reply();
            watcher->deleteLater();
            if (!d->running) {
              return;
            }
            if (reply.type() == QDBusMessage::ErrorMessage) {
              Q_EMIT snapshotReply(owner, requestId, false, {},
                                   normalizedError(QDBusError(reply)));
              return;
            }
            if (reply.arguments().size() != 1 ||
                !reply.arguments().constFirst().canConvert<QDBusArgument>()) {
              Q_EMIT snapshotReply(owner, requestId, false, {},
                                   QStringLiteral("malformed-reply"));
              return;
            }
            Display::Snapshot snapshot;
            const auto decoded = Display::decodeSnapshotArgument(
                qvariant_cast<QDBusArgument>(reply.arguments().constFirst()),
                snapshot);
            if (!decoded.accepted) {
              Q_EMIT snapshotReply(owner, requestId, false, {},
                                   QStringLiteral("malformed-reply"));
              return;
            }
            Q_EMIT snapshotReply(owner, requestId, true, snapshot, {});
          });
}

void QtDisplayTransport::submitStage(const QString &owner,
                                     const quint64 requestId,
                                     const QString &transactionId,
                                     const Display::Candidate &candidate) {
  submitMethod(owner, requestId, QStringLiteral("Stage"),
               {transactionId, QVariant::fromValue(candidate)});
}

void QtDisplayTransport::submitPreview(const QString &owner,
                                       const quint64 requestId,
                                       const QString &transactionId) {
  submitMethod(owner, requestId, QStringLiteral("Preview"), {transactionId});
}

void QtDisplayTransport::submitConfirm(const QString &owner,
                                       const quint64 requestId,
                                       const QString &transactionId) {
  submitMethod(owner, requestId, QStringLiteral("Confirm"), {transactionId});
}

void QtDisplayTransport::submitCancel(const QString &owner,
                                      const quint64 requestId,
                                      const QString &transactionId) {
  submitMethod(owner, requestId, QStringLiteral("Cancel"), {transactionId});
}

void QtDisplayTransport::submitMethod(const QString &owner,
                                      const quint64 requestId,
                                      const QString &method,
                                      const QVariantList &arguments) {
  if (!d->running || owner.isEmpty() || owner != d->owner) {
    QMetaObject::invokeMethod(
        this,
        [this, owner, requestId]() {
          if (d->running) {
            Q_EMIT operationReply(owner, requestId, false, {},
                                  QStringLiteral("owner-unavailable"));
          }
        },
        Qt::QueuedConnection);
    return;
  }
  QDBusMessage call = QDBusMessage::createMethodCall(
      owner, QString::fromLatin1(Display::kObjectPath),
      QString::fromLatin1(Display::kInterfaceName), method);
  call.setArguments(arguments);
  auto *watcher =
      new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, owner, requestId](QDBusPendingCallWatcher *) {
            const QDBusMessage reply = watcher->reply();
            watcher->deleteLater();
            if (!d->running) {
              return;
            }
            if (reply.type() == QDBusMessage::ErrorMessage) {
              Q_EMIT operationReply(owner, requestId, false, {},
                                    normalizedError(QDBusError(reply)));
              return;
            }
            if (reply.arguments().size() != 1 ||
                !reply.arguments().constFirst().canConvert<QDBusArgument>()) {
              Q_EMIT operationReply(owner, requestId, false, {},
                                    QStringLiteral("malformed-reply"));
              return;
            }
            Display::OperationResult result;
            const auto decoded = Display::decodeOperationResultArgument(
                qvariant_cast<QDBusArgument>(reply.arguments().constFirst()),
                result);
            if (!decoded.accepted) {
              Q_EMIT operationReply(owner, requestId, false, {},
                                    QStringLiteral("malformed-reply"));
              return;
            }
            Q_EMIT operationReply(owner, requestId, true, result, {});
          });
}

} // namespace QindaQt::DisplayClient
