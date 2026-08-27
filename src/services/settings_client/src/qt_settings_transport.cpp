// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_client/qt_settings_transport.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QPointer>
#include <QTimer>

#include <utility>

namespace QindaQt::Services::SettingsClient {
namespace {

using SettingsProtocol::WireContract;
constexpr auto BusService = "org.freedesktop.DBus";
constexpr auto BusPath = "/org/freedesktop/DBus";
constexpr auto BusInterface = "org.freedesktop.DBus";
constexpr int TransportTimeoutMilliseconds = 2'000;

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

class QtSettingsTransport::Private final {
public:
    Private(QtSettingsTransport &transport, QDBusConnection bus, QString name)
        : q(transport), connection(std::move(bus)), serviceName(std::move(name)) {}

    void resolveOwner()
    {
        if (!started) return;
        const quint64 generation = ++resolutionGeneration;
        QDBusMessage message = QDBusMessage::createMethodCall(
            QString::fromLatin1(BusService), QString::fromLatin1(BusPath),
            QString::fromLatin1(BusInterface), QStringLiteral("GetNameOwner"));
        message << serviceName;
        watch(message, [this, generation](const QDBusMessage &reply) {
            if (!started || generation != resolutionGeneration) return;
            if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
                bindOwner({});
                return;
            }
            bindOwner(reply.arguments().constFirst().toString());
        });
    }

    void bindOwner(const QString &next)
    {
        if (!started || next == owner) return;
        const QString previous = owner;
        if (!previous.isEmpty()) {
            connection.disconnect(previous, QString::fromLatin1(WireContract::ObjectPath),
                                  QString::fromLatin1(WireContract::InterfaceName),
                                  QString::fromLatin1(WireContract::SettingsChangedSignal), &q,
                                  SLOT(handleSettingsChanged(QString,qulonglong,QStringList)));
        }
        owner.clear();
        if (next.isEmpty()) {
            if (!previous.isEmpty()) Q_EMIT q.ownerChanged({});
            return;
        }
        // AGENT-GUARD: subscribe to the exact unique sender before publishing
        // it. The client's immediate baseline cannot race a missed signal.
        const bool connected = connection.connect(
            next, QString::fromLatin1(WireContract::ObjectPath),
            QString::fromLatin1(WireContract::InterfaceName),
            QString::fromLatin1(WireContract::SettingsChangedSignal), &q,
            SLOT(handleSettingsChanged(QString,qulonglong,QStringList)));
        if (!connected) {
            QTimer::singleShot(100, &q, [this] { resolveOwner(); });
            return;
        }
        owner = next;
        Q_EMIT q.ownerChanged(owner);
    }

    template<typename Callback>
    void watch(const QDBusMessage &message, Callback callback)
    {
        auto *watcher = new QDBusPendingCallWatcher(
            connection.asyncCall(message, TransportTimeoutMilliseconds), &q);
        pending.append(watcher);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
                         [this, watcher, callback = std::move(callback)] {
            pending.removeAll(watcher);
            const QDBusMessage reply = watcher->reply();
            watcher->deleteLater();
            callback(reply);
        });
    }

    void requestMap(quint64 token, const QString &targetOwner, const QString &method,
                    QVariantList arguments, bool commit)
    {
        if (!started || targetOwner.isEmpty() || targetOwner != owner) {
            Q_EMIT q.requestFailed(token, targetOwner,
                                   QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner"),
                                   QStringLiteral("settings service owner changed"));
            return;
        }
        QDBusMessage message = QDBusMessage::createMethodCall(
            targetOwner, QString::fromLatin1(WireContract::ObjectPath),
            QString::fromLatin1(WireContract::InterfaceName), method);
        message.setArguments(std::move(arguments));
        watch(message, [this, token, targetOwner, commit](const QDBusMessage &reply) {
            if (!started) return;
            if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
                Q_EMIT q.requestFailed(token, targetOwner, reply.errorName(), reply.errorMessage());
                return;
            }
            const QVariant first = reply.arguments().constFirst();
            QVariantMap result;
            if (first.metaType().id() == QMetaType::QVariantMap) {
                result = first.toMap();
            } else if (first.metaType() == QMetaType::fromType<QDBusArgument>()) {
                result = qdbus_cast<QVariantMap>(qvariant_cast<QDBusArgument>(first));
            }
            if (result.isEmpty()) {
                Q_EMIT q.requestFailed(token, targetOwner,
                                       QStringLiteral("org.qindaqt.Settings1.Error.MalformedReply"),
                                       QStringLiteral("settings service returned a malformed map"));
            } else if (commit) {
                Q_EMIT q.commitReceived(token, targetOwner, result);
            } else {
                Q_EMIT q.snapshotReceived(token, targetOwner, result);
            }
        });
    }

    QtSettingsTransport &q;
    QDBusConnection connection;
    QString serviceName;
    QString owner;
    QDBusServiceWatcher *serviceWatcher = nullptr;
    QList<QDBusPendingCallWatcher *> pending;
    quint64 resolutionGeneration = 0;
    bool started = false;
};

QtSettingsTransport::QtSettingsTransport(QDBusConnection connection,
                                         QString serviceName, QObject *parent)
    : SettingsTransport(parent)
    , d(std::make_unique<Private>(*this, std::move(connection), std::move(serviceName)))
{
}

QtSettingsTransport::~QtSettingsTransport()
{
    stop();
}

bool QtSettingsTransport::start(QString *error)
{
    if (d->started) {
        setError(error, {});
        return true;
    }
    if (!d->connection.isConnected()) {
        setError(error, QStringLiteral("settings session D-Bus is not connected"));
        return false;
    }
    d->serviceWatcher = new QDBusServiceWatcher(
        d->serviceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(d->serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &service, const QString &, const QString &owner) {
        if (d->started && service == d->serviceName) {
            ++d->resolutionGeneration;
            d->bindOwner(owner);
        }
    });
    if (!d->connection.connect(QString{},
                               QStringLiteral("/org/freedesktop/DBus/Local"),
                               QStringLiteral("org.freedesktop.DBus.Local"),
                               QStringLiteral("Disconnected"), this,
                               SLOT(handleBusDisconnected()))) {
        delete d->serviceWatcher;
        d->serviceWatcher = nullptr;
        setError(error, QStringLiteral("cannot observe local session bus disconnect"));
        return false;
    }
    d->started = true;
    d->resolveOwner();
    setError(error, {});
    return true;
}

void QtSettingsTransport::stop()
{
    if (!d->started) return;
    d->started = false;
    ++d->resolutionGeneration;
    d->bindOwner({});
    for (auto *watcher : std::as_const(d->pending)) {
        watcher->disconnect(this);
        watcher->deleteLater();
    }
    d->pending.clear();
    delete d->serviceWatcher;
    d->serviceWatcher = nullptr;
}

void QtSettingsTransport::requestSnapshot(quint64 token, const QString &owner,
                                          const QStringList &keys)
{
    d->requestMap(token, owner, QString::fromLatin1(WireContract::GetSnapshotMethod),
                  {keys}, false);
}

void QtSettingsTransport::commit(quint64 token, const QString &owner,
                                 const QString &epoch, quint64 baseRevision,
                                 const QVariantList &operations)
{
    d->requestMap(token, owner, QString::fromLatin1(WireContract::CommitUserTransactionMethod),
                  {epoch, baseRevision, operations}, true);
}

void QtSettingsTransport::requestActivation()
{
    if (!d->started) return;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(BusService), QString::fromLatin1(BusPath),
        QString::fromLatin1(BusInterface), QStringLiteral("StartServiceByName"));
    message << d->serviceName << quint32(0);
    d->watch(message, [this](const QDBusMessage &reply) {
        if (!d->started) return;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            Q_EMIT activationFailed(reply.errorMessage().left(512));
            return;
        }
        d->resolveOwner();
    });
}

void QtSettingsTransport::handleBusDisconnected()
{
    if (!d->started) return;
    d->started = false;
    d->owner.clear();
    Q_EMIT busDisconnected();
}

void QtSettingsTransport::handleSettingsChanged(const QString &epoch,
                                                quint64 revision,
                                                const QStringList &keys)
{
    if (d->started && !d->owner.isEmpty()) {
        Q_EMIT settingsChanged(d->owner, epoch, revision, keys);
    }
}

} // namespace QindaQt::Services::SettingsClient
