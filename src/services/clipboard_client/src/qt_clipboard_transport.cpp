// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_client/qt_clipboard_transport.h>

#include <qindaqt/services/clipboard_protocol/clipboard_dbus.h>

#include <QtCore/QMetaObject>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::Services::Clipboard {
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
    return QStringLiteral("transport-error");
}

} // namespace

struct QtClipboardTransport::Private {
    explicit Private(QDBusConnection bus, QString name)
        : connection(std::move(bus)), serviceName(std::move(name)) {}
    QDBusConnection connection;
    QString serviceName;
    QString owner;
    quint64 generation = 0;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    bool running = false;
};

QtClipboardTransport::QtClipboardTransport(const QDBusConnection &connection,
                                           QString serviceName, QObject *parent)
    : ClipboardTransport(parent)
    , d(std::make_unique<Private>(
          connection, serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                            : std::move(serviceName)))
{
    registerDBusTypes();
}

QtClipboardTransport::~QtClipboardTransport() { stop(); }

void QtClipboardTransport::start()
{
    if (d->running) {
        return;
    }
    d->running = true;
    d->watcher = std::make_unique<QDBusServiceWatcher>(
        d->serviceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
            &QtClipboardTransport::onOwnerChanged);
    queryOwner();
}

void QtClipboardTransport::stop()
{
    if (!d->running) {
        return;
    }
    d->running = false;
    ++d->generation;
    setOwner({});
    d->watcher.reset();
}

void QtClipboardTransport::queryOwner()
{
    const quint64 generation = d->generation;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    call.setArguments({d->serviceName});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (!d->running || generation != d->generation) {
                    return;
                }
                if (reply.isError()) {
                    QDBusMessage activate = QDBusMessage::createMethodCall(
                        QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("/org/freedesktop/DBus"),
                        QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("StartServiceByName"));
                    activate.setArguments({d->serviceName, quint32(0)});
                    d->connection.asyncCall(activate);
                } else {
                    setOwner(reply.value());
                }
            });
}

void QtClipboardTransport::onOwnerChanged(const QString &service, const QString &,
                                          const QString &newOwner)
{
    if (d->running && service == d->serviceName) {
        ++d->generation;
        setOwner(newOwner);
    }
}

void QtClipboardTransport::setOwner(const QString &owner)
{
    if (owner == d->owner) {
        return;
    }
    if (!d->owner.isEmpty()) {
        d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Changed"), this,
                                 SLOT(onChanged(quint64,quint32,quint64)));
    }
    d->owner = owner;
    if (!owner.isEmpty()) {
        d->connection.connect(owner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Changed"), this,
                              SLOT(onChanged(quint64,quint32,quint64)));
    }
    Q_EMIT ownerChanged(owner);
}

void QtClipboardTransport::onChanged(quint64 epoch, quint32 generation, quint64 revision)
{
    if (d->running && !d->owner.isEmpty()) {
        Q_EMIT invalidated(d->owner, epoch, generation, revision);
    }
}

void QtClipboardTransport::fetchSnapshot(const QString &owner, quint64 token)
{
    if (!d->running || owner != d->owner || owner.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, owner, token] {
            Q_EMIT snapshotReply(owner, token, false, {},
                                 QStringLiteral("owner-unavailable"));
        }, Qt::QueuedConnection);
        return;
    }
    const QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
        QStringLiteral("GetSnapshot"));
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, token](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<Snapshot> reply = *watcher;
                watcher->deleteLater();
                Q_EMIT snapshotReply(owner, token, !reply.isError(),
                                     reply.isError() ? Snapshot{} : reply.value(),
                                     reply.isError() ? normalizedError(reply.error()) : QString{});
            });
}

void QtClipboardTransport::submitOperation(const QString &owner, quint64 token,
                                           const OperationRequest &request)
{
    if (!d->running || owner != d->owner || owner.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, owner, token] {
            Q_EMIT operationReply(owner, token, false, {},
                                  QStringLiteral("owner-unavailable"));
        }, Qt::QueuedConnection);
        return;
    }
    QString method;
    QList<QVariant> arguments{request.requestId, request.expectedEpoch,
                              request.expectedGeneration, request.expectedRevision};
    switch (request.kind) {
    case OperationKind::Select: method = QStringLiteral("Select"); break;
    case OperationKind::Delete: method = QStringLiteral("Delete"); break;
    case OperationKind::Clear: method = QStringLiteral("Clear"); break;
    case OperationKind::Copy: method = QStringLiteral("Copy"); break;
    }
    if (request.kind == OperationKind::Clear) {
        arguments.append(request.clearAll);
    } else {
        arguments.append(QVariant::fromValue(request.entry));
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName), method);
    call.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, token](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<OperationResult> reply = *watcher;
                watcher->deleteLater();
                Q_EMIT operationReply(owner, token, !reply.isError(),
                                      reply.isError() ? OperationResult{} : reply.value(),
                                      reply.isError() ? normalizedError(reply.error()) : QString{});
            });
}

} // namespace QindaQt::Services::Clipboard
