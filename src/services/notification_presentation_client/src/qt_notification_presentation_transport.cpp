// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_client/qt_notification_presentation_transport.h"

#include "qindaqt/services/notification_presentation/wire_contract.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>

#include <utility>

namespace QindaQt::Services::NotificationPresentationClient {
namespace {

using NotificationPresentation::WireContract;

constexpr auto DBusService = "org.freedesktop.DBus";
constexpr auto DBusPath = "/org/freedesktop/DBus";
constexpr auto DBusInterface = "org.freedesktop.DBus";
constexpr auto GetNameOwnerMethod = "GetNameOwner";
constexpr int DBusTimeoutMilliseconds = 2'000;

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

QtNotificationPresentationTransport::QtNotificationPresentationTransport(
    QDBusConnection connection, QObject *parent)
    : PresentationTransport(parent)
    , m_connection(std::move(connection))
{
}

QtNotificationPresentationTransport::~QtNotificationPresentationTransport()
{
    stop();
}

bool QtNotificationPresentationTransport::start(QString *error)
{
    if (m_started) {
        setError(error, {});
        return true;
    }
    if (!m_connection.isConnected()) {
        setError(error, QStringLiteral("notification session D-Bus is not connected"));
        return false;
    }
    m_serviceWatcher = new QDBusServiceWatcher(
        QString::fromLatin1(WireContract::DefaultServiceName), m_connection,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &service, const QString &, const QString &newOwner) {
                if (!m_started ||
                    service != QLatin1StringView(WireContract::DefaultServiceName)) {
                    return;
                }
                ++m_resolutionGeneration;
                bindOwner(newOwner);
            });
    m_started = true;
    resolveInitialOwner();
    setError(error, {});
    return true;
}

void QtNotificationPresentationTransport::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    ++m_resolutionGeneration;
    if (!m_uniqueOwner.isEmpty()) {
        m_connection.disconnect(
            m_uniqueOwner, QString::fromLatin1(WireContract::ObjectPath),
            QString::fromLatin1(WireContract::InterfaceName),
            QStringLiteral("SnapshotChanged"), this,
            SLOT(handleInvalidation(QString,qulonglong)));
    }
    m_uniqueOwner.clear();
    for (auto *pending : std::as_const(m_pendingCalls)) {
        if (pending != nullptr) {
            pending->disconnect(this);
            pending->deleteLater();
        }
    }
    m_pendingCalls.clear();
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
}

void QtNotificationPresentationTransport::registerPresenter(
    quint64 token, const QString &uniqueOwner, const QString &accessToken)
{
    requestMap(token, uniqueOwner, QStringLiteral("RegisterPresenter"),
               {accessToken}, false);
}

void QtNotificationPresentationTransport::requestSnapshot(
    quint64 token, const QString &uniqueOwner)
{
    requestMap(token, uniqueOwner, QStringLiteral("GetSnapshot"), {}, false);
}

void QtNotificationPresentationTransport::releasePresenter(
    const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner != m_uniqueOwner) {
        return;
    }
    const QDBusMessage message = QDBusMessage::createMethodCall(
        uniqueOwner, QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QStringLiteral("ReleasePresenter"));
    m_connection.call(message, QDBus::NoBlock);
}

void QtNotificationPresentationTransport::dismiss(
    quint64 token, const QString &uniqueOwner, quint32 id)
{
    requestMap(token, uniqueOwner, QStringLiteral("Dismiss"), {id}, true);
}

void QtNotificationPresentationTransport::invokeAction(
    quint64 token, const QString &uniqueOwner, quint32 id,
    const QString &actionKey, const QString &activationToken)
{
    requestMap(token, uniqueOwner, QStringLiteral("InvokeAction"),
               {id, actionKey, activationToken}, true);
}

void QtNotificationPresentationTransport::requestMap(
    quint64 token, const QString &uniqueOwner, const QString &method,
    const QVariantList &arguments, bool operation)
{
    if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_uniqueOwner) {
        const QString name = QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner");
        const QString message = QStringLiteral("notification service owner changed");
        if (operation) {
            Q_EMIT operationFailed(token, uniqueOwner, name, message);
        } else {
            Q_EMIT requestFailed(token, uniqueOwner, name, message);
        }
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        uniqueOwner, QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName), method);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token, uniqueOwner, operation] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (!m_started) {
                    return;
                }
                if (reply.isError()) {
                    if (operation) {
                        Q_EMIT operationFailed(token, uniqueOwner,
                                               reply.error().name(),
                                               reply.error().message());
                    } else {
                        failRequest(token, uniqueOwner, reply.error().name(),
                                    reply.error().message());
                    }
                    return;
                }
                if (operation) {
                    Q_EMIT operationFinished(token, uniqueOwner, reply.value());
                } else {
                    Q_EMIT snapshotReceived(token, uniqueOwner, reply.value());
                }
            });
}

void QtNotificationPresentationTransport::handleInvalidation(
    const QString &epoch, quint64 revision)
{
    if (m_started && !m_uniqueOwner.isEmpty()) {
        Q_EMIT snapshotInvalidated(m_uniqueOwner, epoch, revision);
    }
}

void QtNotificationPresentationTransport::resolveInitialOwner()
{
    if (!m_started) {
        return;
    }
    const quint64 generation = ++m_resolutionGeneration;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(DBusService), QString::fromLatin1(DBusPath),
        QString::fromLatin1(DBusInterface), QString::fromLatin1(GetNameOwnerMethod));
    message << QString::fromLatin1(WireContract::DefaultServiceName);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_resolutionGeneration) {
                    return;
                }
                bindOwner(reply.isError() ? QString{} : reply.value());
            });
}

void QtNotificationPresentationTransport::bindOwner(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner == m_uniqueOwner) {
        return;
    }
    const QString previous = m_uniqueOwner;
    if (!previous.isEmpty()) {
        m_connection.disconnect(
            previous, QString::fromLatin1(WireContract::ObjectPath),
            QString::fromLatin1(WireContract::InterfaceName),
            QStringLiteral("SnapshotChanged"), this,
            SLOT(handleInvalidation(QString,qulonglong)));
    }
    m_uniqueOwner.clear();
    if (uniqueOwner.isEmpty()) {
        if (!previous.isEmpty()) {
            Q_EMIT serviceOwnerChanged({});
        }
        return;
    }

    // AGENT-GUARD: subscribe to the exact unique owner before exposing it.
    // Registration can then never miss an invalidation in the binding gap.
    const bool connected = m_connection.connect(
        uniqueOwner, QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QStringLiteral("SnapshotChanged"), this,
        SLOT(handleInvalidation(QString,qulonglong)));
    if (!connected) {
        if (!previous.isEmpty()) {
            Q_EMIT serviceOwnerChanged({});
        }
        QTimer::singleShot(250, this, [this] { resolveInitialOwner(); });
        return;
    }
    m_uniqueOwner = uniqueOwner;
    Q_EMIT serviceOwnerChanged(m_uniqueOwner);
}

void QtNotificationPresentationTransport::failRequest(
    quint64 token, const QString &uniqueOwner, QString errorName, QString message)
{
    if (errorName.trimmed().isEmpty()) {
        errorName = QStringLiteral("org.freedesktop.DBus.Error.Failed");
    }
    if (message.trimmed().isEmpty()) {
        message = QStringLiteral("notification presentation D-Bus request failed");
    }
    Q_EMIT requestFailed(token, uniqueOwner, errorName, message);
}

} // namespace QindaQt::Services::NotificationPresentationClient
