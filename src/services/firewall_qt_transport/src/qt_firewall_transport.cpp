// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/firewall_qt_transport/qt_firewall_transport.h>

#include <QtCore/QDebug>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusVariant>

namespace QindaQt::Firewall::Client {
namespace {

constexpr auto kServiceName = "org.qindaqt.Firewall1";
constexpr auto kObjectPath = "/org/qindaqt/Firewall1";
constexpr auto kInterfaceName = "org.qindaqt.Firewall1";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

QString normalizedError(const QDBusError &error)
{
    switch (error.type()) {
    case QDBusError::ServiceUnknown:
    case QDBusError::Disconnected:
        return QStringLiteral("firewall service is unavailable");
    case QDBusError::NoReply:
    case QDBusError::Timeout:
        return QStringLiteral("firewall request timed out");
    default:
        return QStringLiteral("firewall request failed: %1").arg(error.message());
    }
}

} // namespace

class QtFirewallTransport::Private final {
public:
    explicit Private(QDBusConnection bus) : connection(std::move(bus)) {}

    QDBusConnection connection;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    bool running = false;
    bool available = false;
    bool signalsBound = false;
};

QtFirewallTransport::QtFirewallTransport(QDBusConnection connection, QObject *parent)
    : FirewallTransport(parent)
    , d(std::make_unique<Private>(std::move(connection)))
{
}

QtFirewallTransport::~QtFirewallTransport() { stop(); }

bool QtFirewallTransport::start(QString *error)
{
    if (d->running) {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    if (!d->connection.isConnected() || d->connection.interface() == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("firewall bus connection is unavailable");
        }
        return false;
    }
    d->running = true;
    d->watcher = std::make_unique<QDBusServiceWatcher>(
        QString::fromLatin1(kServiceName), d->connection,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
            &QtFirewallTransport::onServiceOwnerChanged);
    // AGENT-NOTE: bound against the well-known name, not a resolved unique
    // owner. Qt tracks that name's current owner internally and keeps the
    // match current across a restart; no manual rebind-on-owner-change is
    // needed the way Network1's unique-owner binding requires.
    d->connection.connect(QString::fromLatin1(kServiceName),
                          QString::fromLatin1(kObjectPath),
                          QString::fromLatin1(kInterfaceName),
                          QStringLiteral("Changed"), this, SLOT(onChanged()));
    d->connection.connect(QString::fromLatin1(kServiceName),
                          QString::fromLatin1(kObjectPath),
                          QString::fromLatin1(kInterfaceName),
                          QStringLiteral("AlertRaised"), this,
                          SLOT(onAlertRaised(QString)));
    d->connection.connect(QString::fromLatin1(kServiceName),
                          QString::fromLatin1(kObjectPath),
                          QString::fromLatin1(kInterfaceName),
                          QStringLiteral("Responded"), this,
                          SLOT(onResponded(QString)));
    d->signalsBound = true;
    if (error != nullptr) {
        error->clear();
    }
    const QDBusReply<QString> owner =
        d->connection.interface()->serviceOwner(QString::fromLatin1(kServiceName));
    d->available = owner.isValid() && !owner.value().isEmpty();
    // Emitted last, once every piece of local state is settled, in case a
    // slot connected to it calls back into this transport (requestSnapshot)
    // before start() unwinds.
    Q_EMIT serviceAvailableChanged(d->available);
    return true;
}

void QtFirewallTransport::stop()
{
    if (!d->running) {
        return;
    }
    d->running = false;
    d->watcher.reset();
    if (d->signalsBound) {
        d->connection.disconnect(QString::fromLatin1(kServiceName),
                                 QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Changed"), this, SLOT(onChanged()));
        d->connection.disconnect(QString::fromLatin1(kServiceName),
                                 QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("AlertRaised"), this,
                                 SLOT(onAlertRaised(QString)));
        d->connection.disconnect(QString::fromLatin1(kServiceName),
                                 QString::fromLatin1(kObjectPath),
                                 QString::fromLatin1(kInterfaceName),
                                 QStringLiteral("Responded"), this,
                                 SLOT(onResponded(QString)));
        d->signalsBound = false;
    }
    const bool wasAvailable = d->available;
    d->available = false;
    if (wasAvailable) {
        Q_EMIT serviceAvailableChanged(false);
    }
}

void QtFirewallTransport::onServiceOwnerChanged(const QString &name,
                                                const QString &oldOwner,
                                                const QString &newOwner)
{
    Q_UNUSED(name);
    Q_UNUSED(oldOwner);
    if (!d->running) {
        return;
    }
    const bool nowAvailable = !newOwner.isEmpty();
    if (nowAvailable == d->available) {
        return;
    }
    d->available = nowAvailable;
    Q_EMIT serviceAvailableChanged(nowAvailable);
}

void QtFirewallTransport::onChanged()
{
    if (d->running) {
        Q_EMIT changed();
    }
}

void QtFirewallTransport::onAlertRaised(const QString &alertJson)
{
    if (d->running) {
        Q_EMIT alertRaised(alertJson.toUtf8());
    }
}

void QtFirewallTransport::onResponded(const QString &responseJson)
{
    if (d->running) {
        Q_EMIT responded(responseJson.toUtf8());
    }
}

void QtFirewallTransport::requestSnapshot(const quint64 token)
{
    if (!d->running || !d->available) {
        Q_EMIT requestFailed(token, QStringLiteral("firewall service is unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kPropertiesInterface), QStringLiteral("GetAll"));
    call.setArguments({QString::fromLatin1(kInterfaceName)});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    Q_EMIT requestFailed(token, normalizedError(reply.error()));
                    return;
                }
                const QVariantMap properties = reply.value();
                const QString state = properties.value(QStringLiteral("State")).toString();
                const QByteArray headlineJson =
                    properties.value(QStringLiteral("Headline")).toString().toUtf8();
                const quint32 alertCount =
                    properties.value(QStringLiteral("AlertCount")).toUInt();
                const QByteArray topAlertJson =
                    properties.value(QStringLiteral("TopAlert")).toString().toUtf8();
                Q_EMIT snapshotReceived(token, state, headlineJson, alertCount,
                                        topAlertJson);
            });
}

void QtFirewallTransport::requestAlerts(const quint64 token)
{
    if (!d->running || !d->available) {
        Q_EMIT requestFailed(token, QStringLiteral("firewall service is unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("Alerts"));
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    Q_EMIT requestFailed(token, normalizedError(reply.error()));
                    return;
                }
                Q_EMIT alertsReceived(token, reply.value().toUtf8());
            });
}

void QtFirewallTransport::requestAlert(const quint64 token, const QString &id)
{
    if (!d->running || !d->available) {
        Q_EMIT requestFailed(token, QStringLiteral("firewall service is unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("Alert"));
    call.setArguments({id});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    Q_EMIT requestFailed(token, normalizedError(reply.error()));
                    return;
                }
                Q_EMIT alertReceived(token, reply.value().toUtf8());
            });
}

namespace {

// Shared by the three fire-and-forget actions: the production contract
// (QindaFirewall docs/monitor.md) gives none of them a return value, so a
// failure is a logged diagnostic rather than something routed back through
// FirewallTransport's signals.
void sendFireAndForget(QDBusConnection &connection, const QString &method,
                       const QString &id, QObject *parent)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), method);
    call.setArguments({id});
    auto *watcher = new QDBusPendingCallWatcher(connection.asyncCall(call), parent);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, parent,
                     [watcher, method](QDBusPendingCallWatcher *) {
                         const QDBusPendingReply<> reply = *watcher;
                         watcher->deleteLater();
                         if (reply.isError()) {
                             qWarning().noquote()
                                 << "QindaQt firewall client:" << method
                                 << "failed:" << reply.error().message();
                         }
                     });
}

} // namespace

void QtFirewallTransport::requestDismiss(const QString &id)
{
    if (!d->running) {
        return;
    }
    sendFireAndForget(d->connection, QStringLiteral("Dismiss"), id, this);
}

void QtFirewallTransport::requestConfirmMine(const QString &id)
{
    if (!d->running) {
        return;
    }
    sendFireAndForget(d->connection, QStringLiteral("ConfirmMine"), id, this);
}

void QtFirewallTransport::requestOpenWindow(const QString &id)
{
    if (!d->running) {
        return;
    }
    sendFireAndForget(d->connection, QStringLiteral("OpenWindow"), id, this);
}

} // namespace QindaQt::Firewall::Client
