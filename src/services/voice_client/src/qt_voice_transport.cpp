// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/voice_client/qt_voice_transport.h>

#include <qindaqt/services/voice_protocol/voice_dbus.h>

#include <QtCore/QMetaObject>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::Services::Voice {
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

struct QtVoiceTransport::Private {
    explicit Private(QDBusConnection bus, QString name)
        : connection(std::move(bus)), serviceName(std::move(name)) {}
    QDBusConnection connection;
    QString serviceName;
    QString owner;
    quint64 generation = 0;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    bool running = false;
};

QtVoiceTransport::QtVoiceTransport(const QDBusConnection &connection, QString serviceName,
                                   QObject *parent)
    : VoiceTransport(parent)
    , d(std::make_unique<Private>(connection,
                                  serviceName.isEmpty()
                                      ? QString::fromLatin1(kServiceName)
                                      : std::move(serviceName)))
{
}

QtVoiceTransport::~QtVoiceTransport() { stop(); }

void QtVoiceTransport::start()
{
    if (d->running) {
        return;
    }
    d->running = true;
    d->watcher = std::make_unique<QDBusServiceWatcher>(
        d->serviceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
            &QtVoiceTransport::onOwnerChanged);
    queryOwner();
}

void QtVoiceTransport::stop()
{
    if (!d->running) {
        return;
    }
    d->running = false;
    ++d->generation;
    setOwner({});
    d->watcher.reset();
}

void QtVoiceTransport::queryOwner()
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
                    requestActivation(generation);
                } else {
                    setOwner(reply.value());
                }
            });
}

// The provider is D-Bus activatable, so asking for it is how a fresh session
// gets voice without an autostart race. A refusal is the definitive answer
// that no provider is installed, and it must be reported: nothing else will
// arrive, and a consumer waiting on the owner-change watch would wait forever.
void QtVoiceTransport::requestActivation(const quint64 generation)
{
    QDBusMessage activate = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
    activate.setArguments({d->serviceName, quint32(0)});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(activate), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<quint32> reply = *watcher;
                watcher->deleteLater();
                if (!d->running || generation != d->generation) {
                    return;
                }
                if (reply.isError()) {
                    Q_EMIT ownerChanged(QString());
                }
                // On success the owner-change watch delivers the new owner.
            });
}

void QtVoiceTransport::onOwnerChanged(const QString &service, const QString &,
                                      const QString &newOwner)
{
    if (d->running && service == d->serviceName) {
        ++d->generation;
        setOwner(newOwner);
    }
}

void QtVoiceTransport::setOwner(const QString &owner)
{
    if (owner == d->owner) {
        return;
    }
    if (!d->owner.isEmpty()) {
        d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Changed"), this, SLOT(onChanged(quint64)));
        d->connection.disconnect(d->owner, QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Level"), this, SLOT(onLevel(uint)));
    }
    d->owner = owner;
    if (!owner.isEmpty()) {
        d->connection.connect(owner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Changed"), this, SLOT(onChanged(quint64)));
        d->connection.connect(owner, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Level"), this, SLOT(onLevel(uint)));
    }
    Q_EMIT ownerChanged(owner);
}

void QtVoiceTransport::onChanged(const quint64 revision)
{
    if (d->running && !d->owner.isEmpty()) {
        Q_EMIT invalidated(d->owner, revision);
    }
}

void QtVoiceTransport::onLevel(const uint levelPercent)
{
    if (d->running && !d->owner.isEmpty()) {
        Q_EMIT levelReported(d->owner, static_cast<quint32>(levelPercent));
    }
}

void QtVoiceTransport::fetchSnapshot(const QString &owner, const quint64 token)
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
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                Q_EMIT snapshotReply(
                    owner, token, !reply.isError(),
                    reply.isError() ? Snapshot{} : decodeSnapshot(reply.value()),
                    reply.isError() ? normalizedError(reply.error()) : QString{});
            });
}

void QtVoiceTransport::submitOperation(const QString &owner, const quint64 token,
                                       const OperationRequest &request)
{
    const QString method = methodNameForKind(request.kind);
    if (!d->running || owner != d->owner || owner.isEmpty() || method.isEmpty()) {
        const QString reason = method.isEmpty() ? QStringLiteral("unknown-kind")
                                                : QStringLiteral("owner-unavailable");
        QMetaObject::invokeMethod(this, [this, owner, token, reason] {
            Q_EMIT operationReply(owner, token, false, {}, reason);
        }, Qt::QueuedConnection);
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
        method);
    call.setArguments(argumentsForRequest(request));
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, token](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                Q_EMIT operationReply(
                    owner, token, !reply.isError(),
                    reply.isError() ? OperationResult{}
                                    : decodeOperationResult(reply.value()),
                    reply.isError() ? normalizedError(reply.error()) : QString{});
            });
}

} // namespace QindaQt::Services::Voice
