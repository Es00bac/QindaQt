// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_client/qt_settings_transport.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

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

class SettingsChangeRelay final : public QObject {
    Q_OBJECT
public:
    SettingsChangeRelay(QString owner, quint64 generation, QObject *parent)
        : QObject(parent), m_owner(std::move(owner)), m_generation(generation) {}

Q_SIGNALS:
    void observed(const QString &owner, quint64 ownerGeneration,
                  const QString &epoch, quint64 revision, const QStringList &keys);

private Q_SLOTS:
    void receive(const QString &epoch, quint64 revision, const QStringList &keys)
    {
        Q_EMIT observed(m_owner, m_generation, epoch, revision, keys);
    }

private:
    const QString m_owner;
    const quint64 m_generation;
};

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
        ++ownerGeneration;
        if (subscription != nullptr) {
            connection.disconnect(previous, QString::fromLatin1(WireContract::ObjectPath),
                                  QString::fromLatin1(WireContract::InterfaceName),
                                  QString::fromLatin1(WireContract::SettingsChangedSignal),
                                  subscription,
                                  SLOT(receive(QString,qulonglong,QStringList)));
            subscription->deleteLater();
            subscription = nullptr;
        }
        owner.clear();
        if (!previous.isEmpty()) {
            // Retire the client's old lineage before attempting to subscribe
            // to a replacement. Subscription failure must fail closed rather
            // than leave pending old-owner requests acceptable.
            Q_EMIT q.ownerChanged({});
        }
        if (next.isEmpty()) {
            return;
        }
        // AGENT-GUARD: subscribe to the exact unique sender before publishing
        // it. The client's immediate baseline cannot race a missed signal.
        auto *nextSubscription = new SettingsChangeRelay(next, ownerGeneration, &q);
        QObject::connect(nextSubscription, &SettingsChangeRelay::observed, &q,
                         [this](const QString &observedOwner, quint64 observedGeneration,
                                const QString &epoch, quint64 revision,
                                const QStringList &keys) {
            // AGENT-GUARD: the relay captures the owner generation that
            // installed the subscription. A queued signal from a retired
            // relay must never be relabelled as traffic from its replacement.
            if (started && observedGeneration == ownerGeneration
                && observedOwner == owner) {
                Q_EMIT q.settingsChanged(observedOwner, epoch, revision, keys);
            }
        });
        const bool connected = connection.connect(
            next, QString::fromLatin1(WireContract::ObjectPath),
            QString::fromLatin1(WireContract::InterfaceName),
            QString::fromLatin1(WireContract::SettingsChangedSignal), nextSubscription,
            SLOT(receive(QString,qulonglong,QStringList)));
        if (!connected) {
            delete nextSubscription;
            QTimer::singleShot(100, &q, [this] { resolveOwner(); });
            return;
        }
        subscription = nextSubscription;
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
        const quint64 requestGeneration = ownerGeneration;
        watch(message, [this, token, targetOwner, requestGeneration,
                        commit](const QDBusMessage &reply) {
            // AGENT-GUARD: owner loss/replacement retires every pending reply
            // even when a replacement subscription cannot be installed.
            if (!started || requestGeneration != ownerGeneration
                || targetOwner != owner) return;
            if (reply.type() == QDBusMessage::ErrorMessage
                || reply.arguments().size() != 1) {
                const bool remoteError = reply.type() == QDBusMessage::ErrorMessage;
                Q_EMIT q.requestFailed(
                    token, targetOwner,
                    remoteError ? reply.errorName()
                                : QStringLiteral("org.qindaqt.Settings1.Error.MalformedReply"),
                    remoteError ? reply.errorMessage()
                                : QStringLiteral("settings service returned wrong reply arity"));
                return;
            }
            const QVariant first = reply.arguments().constFirst();
            const qsizetype maximumFields = commit ? WireContract::CommitReplyFieldCount
                                                   : WireContract::SnapshotReplyFieldCount;
            const auto result = SettingsProtocol::decodeBoundedVariantMap(first, maximumFields);
            if (!result.has_value() || result->isEmpty()) {
                Q_EMIT q.requestFailed(token, targetOwner,
                                       QStringLiteral("org.qindaqt.Settings1.Error.MalformedReply"),
                                       QStringLiteral("settings service returned a malformed map"));
            } else if (commit) {
                Q_EMIT q.commitReceived(token, targetOwner, *result);
            } else {
                Q_EMIT q.snapshotReceived(token, targetOwner, *result);
            }
        });
    }

    QtSettingsTransport &q;
    QDBusConnection connection;
    QString serviceName;
    QString owner;
    QDBusServiceWatcher *serviceWatcher = nullptr;
    SettingsChangeRelay *subscription = nullptr;
    QList<QDBusPendingCallWatcher *> pending;
    quint64 resolutionGeneration = 0;
    quint64 ownerGeneration = 0;
    bool activationInFlight = false;
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
    ++d->resolutionGeneration;
    d->bindOwner({});
    // AGENT-GUARD: start()/stop() is a reusable lifecycle. Leave neither the
    // exact-owner relay nor the bus-local disconnect match installed, or a
    // second start on the same connection is rejected as a duplicate match.
    d->connection.disconnect(QString{},
                             QStringLiteral("/org/freedesktop/DBus/Local"),
                             QStringLiteral("org.freedesktop.DBus.Local"),
                             QStringLiteral("Disconnected"), this,
                             SLOT(handleBusDisconnected()));
    d->started = false;
    d->activationInFlight = false;
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
    if (!d->started || d->activationInFlight) return;
    d->activationInFlight = true;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(BusService), QString::fromLatin1(BusPath),
        QString::fromLatin1(BusInterface), QStringLiteral("StartServiceByName"));
    message << d->serviceName << quint32(0);
    d->watch(message, [this](const QDBusMessage &reply) {
        if (!d->started) return;
        d->activationInFlight = false;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            Q_EMIT activationFailed(reply.errorMessage().left(512));
            return;
        }
        Q_EMIT activationCompleted();
        d->resolveOwner();
    });
}

void QtSettingsTransport::handleBusDisconnected()
{
    if (!d->started) return;
    d->started = false;
    ++d->resolutionGeneration;
    ++d->ownerGeneration;
    d->activationInFlight = false;
    d->owner.clear();
    if (d->subscription != nullptr) {
        d->subscription->deleteLater();
        d->subscription = nullptr;
    }
    for (auto *watcher : std::as_const(d->pending)) {
        watcher->disconnect(this);
        watcher->deleteLater();
    }
    d->pending.clear();
    delete d->serviceWatcher;
    d->serviceWatcher = nullptr;
    Q_EMIT busDisconnected();
}

} // namespace QindaQt::Services::SettingsClient

#include "qt_settings_transport.moc"
