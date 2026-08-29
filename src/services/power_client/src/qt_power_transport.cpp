// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_client/qt_power_transport.h>

#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power {
namespace {

QString normalizedError(const QDBusError &error)
{
    if (error.type() == QDBusError::NoReply || error.type() == QDBusError::Timeout) {
        return QStringLiteral("transport-timeout");
    }
    if (error.type() == QDBusError::ServiceUnknown
        || error.type() == QDBusError::Disconnected) {
        return QStringLiteral("owner-unavailable");
    }
    if (error.type() == QDBusError::InvalidArgs
        || error.type() == QDBusError::InvalidSignature) {
        return QStringLiteral("malformed-reply");
    }
    return QStringLiteral("transport-error");
}

} // namespace

class QtPowerTransport::Private
{
public:
    Private(const QDBusConnection &bus, QString name)
        : connection(bus)
        , serviceName(std::move(name))
    {
    }

    QDBusConnection connection;
    QString serviceName;
    QString owner;
    quint64 ownerGeneration = 0;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    bool running = false;
};

QtPowerTransport::QtPowerTransport(const QDBusConnection &connection,
                                   QString serviceName, QObject *parent)
    : PowerTransport(parent)
    , d(std::make_unique<Private>(
          connection, serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                            : std::move(serviceName)))
{
    registerDBusTypes();
}

QtPowerTransport::~QtPowerTransport()
{
    stop();
}

void QtPowerTransport::start()
{
    if (d->running) {
        return;
    }
    d->running = true;
    d->watcher = std::make_unique<QDBusServiceWatcher>(
        d->serviceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange,
        this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
            &QtPowerTransport::onServiceOwnerChanged);
    queryInitialOwner();
}

void QtPowerTransport::stop()
{
    if (!d->running) {
        return;
    }
    d->running = false;
    ++d->ownerGeneration;
    setOwner({});
    d->watcher.reset();
}

void QtPowerTransport::queryInitialOwner()
{
    const quint64 generation = d->ownerGeneration;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    call.setArguments({d->serviceName});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
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
                setOwner(reply.isError() ? QString{} : reply.value());
            });
}

void QtPowerTransport::onServiceOwnerChanged(const QString &service,
                                             const QString &oldOwner,
                                             const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (d->running && service == d->serviceName) {
        ++d->ownerGeneration;
        setOwner(newOwner);
    }
}

void QtPowerTransport::setOwner(const QString &owner)
{
    if (owner == d->owner) {
        return;
    }
    if (!d->owner.isEmpty()) {
        d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Changed"), this,
                                 SLOT(onChanged(quint64,quint64)));
    }
    d->owner = owner;
    if (!d->owner.isEmpty()) {
        d->connection.connect(d->owner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Changed"), this,
                              SLOT(onChanged(quint64,quint64)));
    }
    Q_EMIT ownerChanged(d->owner);
}

void QtPowerTransport::onChanged(const quint64 epoch, const quint64 revision)
{
    if (d->running && !d->owner.isEmpty()) {
        Q_EMIT invalidated(d->owner, epoch, revision);
    }
}

void QtPowerTransport::fetchSnapshot(const QString &owner, const quint64 requestId)
{
    if (!d->running || owner.isEmpty() || owner != d->owner) {
        Q_EMIT snapshotReply(owner, requestId, false, {},
                             QStringLiteral("owner-unavailable"));
        return;
    }
    const QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
        QStringLiteral("GetSnapshot"));
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, requestId](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<Snapshot> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    Q_EMIT snapshotReply(owner, requestId, false, {},
                                         normalizedError(reply.error()));
                } else {
                    Q_EMIT snapshotReply(owner, requestId, true, reply.value(), {});
                }
            });
}

void QtPowerTransport::submitOperation(const QString &owner, const quint64 requestId,
                                       const PowerClientRequest &request)
{
    if (!d->running || owner.isEmpty() || owner != d->owner) {
        Q_EMIT operationReply(owner, requestId, false, {},
                              QStringLiteral("owner-unavailable"));
        return;
    }

    QString method;
    QList<QVariant> arguments;
    switch (request.kind) {
    case OperationKind::SetProfile:
        method = QStringLiteral("SetProfile");
        arguments = {request.profileId};
        break;
    case OperationKind::AcquireProfileHold:
        method = QStringLiteral("AcquireProfileHold");
        arguments = {request.profileId, request.applicationName, request.reason};
        break;
    case OperationKind::ReleaseProfileHold:
        method = QStringLiteral("ReleaseProfileHold");
        arguments = {QVariant::fromValue(request.handle)};
        break;
    case OperationKind::SetKeyboardBrightness:
        method = QStringLiteral("SetKeyboardBrightness");
        arguments = {QVariant::fromValue(request.handle), request.value};
        break;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
        method);
    call.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, requestId](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<OperationResult> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    Q_EMIT operationReply(owner, requestId, false, {},
                                          normalizedError(reply.error()));
                } else {
                    Q_EMIT operationReply(owner, requestId, true, reply.value(), {});
                }
            });
}

} // namespace QindaQt::Power
