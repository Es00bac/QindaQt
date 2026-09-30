// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_client/qt_power_transport.h>

#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <memory>
#include <utility>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power {
namespace {

QString normalizedError(const QDBusError &error)
{
    if (error.name() == QStringLiteral("org.qindaqt.Power1.Error.Unsupported")) {
        return QStringLiteral("unsupported");
    }
    if (error.name() == QStringLiteral("org.qindaqt.Power1.Error.Busy")) {
        return QStringLiteral("busy");
    }
    if (error.name() == QStringLiteral("org.qindaqt.Power1.Error.Invalid")) {
        return QStringLiteral("invalid-request");
    }
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
    QTimer activationRetry;
    bool activationPending = false;
    bool ownerResolved = false;
    bool running = false;
    QTimer idleReceiptTimeout;
    QString idleReceiptOwner;
    QString idleReceiptNonce;
    quint64 idleReceiptRequestId = 0;
    quint64 idleReceiptOwnerGeneration = 0;
    QDBusPendingCallWatcher *idleReceiptCall = nullptr;
    bool idleReceiptPending = false;
};

QtPowerTransport::QtPowerTransport(const QDBusConnection &connection,
                                   QString serviceName, QObject *parent)
    : PowerTransport(parent)
    , d(std::make_unique<Private>(
          connection, serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                            : std::move(serviceName)))
{
    registerDBusTypes();
    d->activationRetry.setSingleShot(true);
    d->activationRetry.setInterval(1000);
    d->idleReceiptTimeout.setSingleShot(true);
    d->idleReceiptTimeout.setInterval(1500);
    connect(&d->idleReceiptTimeout, &QTimer::timeout, this, [this] {
        finishIdleInhibitorQuery(false, 0, 0, QStringLiteral("receipt-timeout"));
    });
    connect(&d->activationRetry, &QTimer::timeout, this,
            &QtPowerTransport::requestActivation);
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
    requestActivation();
}

void QtPowerTransport::stop()
{
    if (!d->running) {
        return;
    }
    d->running = false;
    d->activationRetry.stop();
    d->activationPending = false;
    finishIdleInhibitorQuery(false, 0, 0, QStringLiteral("owner-unavailable"));
    ++d->ownerGeneration;
    setOwner({});
    d->watcher.reset();
    d->ownerResolved = false;
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

void QtPowerTransport::requestActivation()
{
    if (!d->running || d->activationPending || !d->owner.isEmpty()) {
        return;
    }
    d->activationPending = true;
    const quint64 generation = d->ownerGeneration;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
    call.setArguments({d->serviceName, quint32(0)});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                if (!d->running || generation != d->ownerGeneration) {
                    return;
                }
                d->activationPending = false;
                // AGENT-GUARD: Activation only requests residency. Resolve the
                // unique owner before fetching; never replay a control request.
                queryInitialOwner();
                if (d->owner.isEmpty() && d->connection.isConnected()) {
                    d->activationRetry.start();
                }
            });
}

void QtPowerTransport::onServiceOwnerChanged(const QString &service,
                                             const QString &oldOwner,
                                             const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (d->running && service == d->serviceName) {
        ++d->ownerGeneration;
        d->activationPending = false;
        setOwner(newOwner);
        if (newOwner.isEmpty()) {
            d->activationRetry.start();
        } else {
            d->activationRetry.stop();
        }
    }
}

void QtPowerTransport::setOwner(const QString &owner)
{
    if (d->idleReceiptPending && owner != d->idleReceiptOwner) {
        finishIdleInhibitorQuery(false, 0, 0, QStringLiteral("owner-replaced"));
    }
    if (owner == d->owner && d->ownerResolved) {
        return;
    }
    if (!d->owner.isEmpty()) {
        d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Changed"), this,
                                 SLOT(onChanged(quint64,quint64)));
        d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("IdleInhibitorsChanged"), this,
                                 SLOT(onIdleInhibitorsChanged(QDBusMessage)));
    }
    d->ownerResolved = true;
    d->owner = owner;
    if (!d->owner.isEmpty()) {
        d->connection.connect(d->owner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Changed"), this,
                              SLOT(onChanged(quint64,quint64)));
        d->connection.connect(d->owner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("IdleInhibitorsChanged"), this,
                              SLOT(onIdleInhibitorsChanged(QDBusMessage)));
    }
    Q_EMIT ownerChanged(d->owner);
}

void QtPowerTransport::onChanged(const quint64 epoch, const quint64 revision)
{
    if (d->running && !d->owner.isEmpty()) {
        Q_EMIT invalidated(d->owner, epoch, revision);
    }
}

void QtPowerTransport::onIdleInhibitorsChanged(const QDBusMessage &message)
{
    const auto arguments = message.arguments();
    if (!d->running || d->owner.isEmpty() ||
        message.type() != QDBusMessage::SignalMessage ||
        message.service() != d->owner ||
        message.path() != QString::fromLatin1(kObjectPath) ||
        message.interface() != QString::fromLatin1(kInterfaceName) ||
        message.member() != QStringLiteral("IdleInhibitorsChanged") ||
        message.signature() != QStringLiteral("uu") || arguments.size() != 2 ||
        arguments.at(0).metaType() != QMetaType::fromType<quint32>() ||
        arguments.at(1).metaType() != QMetaType::fromType<quint32>()) {
        return;
    }
    Q_EMIT idleInhibitorsChanged(d->owner, arguments.at(0).toUInt(),
                                 arguments.at(1).toUInt());
}

void QtPowerTransport::onIdleInhibitorStateReceipt(const QDBusMessage &message)
{
    const auto arguments = message.arguments();
    if (!d->idleReceiptPending || !d->running ||
        d->idleReceiptOwner != d->owner ||
        d->idleReceiptOwnerGeneration != d->ownerGeneration ||
        message.type() != QDBusMessage::SignalMessage ||
        message.service() != d->idleReceiptOwner ||
        message.path() != QString::fromLatin1(kObjectPath) ||
        message.interface() != QString::fromLatin1(kInterfaceName) ||
        message.member() != QStringLiteral("IdleInhibitorStateReceipt") ||
        message.signature() != QStringLiteral("suu") || arguments.size() != 3 ||
        arguments.at(0).metaType() != QMetaType::fromType<QString>() ||
        arguments.at(1).metaType() != QMetaType::fromType<quint32>() ||
        arguments.at(2).metaType() != QMetaType::fromType<quint32>() ||
        arguments.at(0).toString() != d->idleReceiptNonce) {
        return;
    }
    finishIdleInhibitorQuery(true, arguments.at(1).toUInt(),
                             arguments.at(2).toUInt(), {});
}

void QtPowerTransport::finishIdleInhibitorQuery(
    const bool succeeded, const quint32 supportedScopes,
    const quint32 activeScopes, const QString &reasonCode)
{
    if (!d->idleReceiptPending) {
        return;
    }
    const QString owner = d->idleReceiptOwner;
    const quint64 requestId = d->idleReceiptRequestId;
    d->idleReceiptTimeout.stop();
    d->connection.disconnect(
        owner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName),
        QStringLiteral("IdleInhibitorStateReceipt"), this,
        SLOT(onIdleInhibitorStateReceipt(QDBusMessage)));
    d->idleReceiptPending = false;
    d->idleReceiptOwner.clear();
    d->idleReceiptNonce.clear();
    d->idleReceiptRequestId = 0;
    d->idleReceiptOwnerGeneration = 0;
    d->idleReceiptCall = nullptr;
    Q_EMIT idleInhibitorStateReply(owner, requestId, succeeded,
                                   supportedScopes, activeScopes, reasonCode);
}

void QtPowerTransport::queryIdleInhibitorState(const QString &owner,
                                               const quint64 requestId)
{
    if (!d->running || owner.isEmpty() || owner != d->owner) {
        Q_EMIT idleInhibitorStateReply(owner, requestId, false, 0, 0,
                                       QStringLiteral("owner-unavailable"));
        return;
    }
    if (d->idleReceiptPending) {
        finishIdleInhibitorQuery(false, 0, 0, QStringLiteral("superseded"));
    }
    d->idleReceiptPending = true;
    d->idleReceiptOwner = owner;
    d->idleReceiptRequestId = requestId;
    d->idleReceiptNonce = QUuid::createUuid().toString(QUuid::Id128).toLower();
    d->idleReceiptOwnerGeneration = d->ownerGeneration;
    const auto slot = SLOT(onIdleInhibitorStateReceipt(QDBusMessage));
    if (!d->connection.connect(
            owner, QString::fromLatin1(kObjectPath),
            QString::fromLatin1(kInterfaceName),
            QStringLiteral("IdleInhibitorStateReceipt"), this, slot)) {
        finishIdleInhibitorQuery(false, 0, 0, QStringLiteral("receipt-subscribe-failed"));
        return;
    }
    d->idleReceiptTimeout.start();
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName),
        QStringLiteral("RequestIdleInhibitorStateWithReceipt"));
    call.setArguments({d->idleReceiptNonce});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call, 1500), this);
    d->idleReceiptCall = watcher;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, requestId](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<> reply = *watcher;
                watcher->deleteLater();
                if (!d->idleReceiptPending || d->idleReceiptOwner != owner ||
                    d->idleReceiptRequestId != requestId) {
                    return;
                }
                d->idleReceiptCall = nullptr;
                if (reply.isError()) {
                    finishIdleInhibitorQuery(false, 0, 0,
                                             normalizedError(reply.error()));
                }
                // A successful method reply is transport progress only. The
                // matching current-owner nonce receipt alone supplies state.
            });
}

void QtPowerTransport::acquireIdleInhibitor(const QString &owner,
                                            const quint64 requestId,
                                            const QString &application,
                                            const QString &reason,
                                            const IdleInhibitorScopes scopes)
{
    if (!d->running || owner.isEmpty() || owner != d->owner) {
        Q_EMIT idleInhibitorAcquireReply(owner, requestId, false, {},
                                         QStringLiteral("owner-unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("AcquireIdleInhibitor"));
    call.setArguments({application, reason,
                       static_cast<quint32>(scopes.toInt())});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, requestId] {
                const QDBusPendingReply<Handle> reply = *watcher;
                watcher->deleteLater();
                if (!d->running || d->owner != owner) {
                    Q_EMIT idleInhibitorAcquireReply(owner, requestId, false, {},
                                                     QStringLiteral("owner-replaced"));
                } else if (reply.isError()) {
                    Q_EMIT idleInhibitorAcquireReply(owner, requestId, false, {},
                                                     normalizedError(reply.error()));
                } else {
                    Q_EMIT idleInhibitorAcquireReply(owner, requestId, true,
                                                     reply.value(), {});
                }
            });
}

void QtPowerTransport::releaseIdleInhibitor(const QString &owner,
                                            const quint64 requestId,
                                            const Handle &handle)
{
    if (!d->running || owner.isEmpty() || owner != d->owner) {
        Q_EMIT idleInhibitorReleaseReply(owner, requestId, false, false,
                                         QStringLiteral("owner-unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("ReleaseIdleInhibitor"));
    call.setArguments({QVariant::fromValue(handle)});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [watcher, owner, requestId, this] {
                const QDBusPendingReply<bool> reply = *watcher;
                watcher->deleteLater();
                if (!d->running || d->owner != owner) {
                    Q_EMIT idleInhibitorReleaseReply(owner, requestId, false, false,
                                                     QStringLiteral("owner-replaced"));
                } else if (reply.isError()) {
                    Q_EMIT idleInhibitorReleaseReply(owner, requestId, false, false,
                                                     normalizedError(reply.error()));
                } else {
                    Q_EMIT idleInhibitorReleaseReply(owner, requestId, true,
                                                     reply.value(), {});
                }
            });
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
    case OperationKind::SetInternalBrightness:
        method = QStringLiteral("SetInternalBrightness");
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
