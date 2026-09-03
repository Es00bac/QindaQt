// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>

#include <utility>

namespace QindaQt::ShellWindowActionsClient {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/CompositorShell";
constexpr auto InterfaceName = "org.qindaqt.CompositorShell1";
constexpr auto DBusService = "org.freedesktop.DBus";
constexpr auto DBusPath = "/org/freedesktop/DBus";
constexpr auto DBusInterface = "org.freedesktop.DBus";
constexpr auto GetNameOwnerMethod = "GetNameOwner";
constexpr auto IdentityMethod = "ActiveWindowIdentity";
constexpr auto IdentityChangedSignal = "ActiveWindowIdentityChanged";
constexpr int DBusTimeoutMilliseconds = 2000;

QString methodName(Compositor::ShellWindowAction action)
{
    switch (action) {
    case Compositor::ShellWindowAction::Activate:
        return QStringLiteral("ActivateWindow");
    case Compositor::ShellWindowAction::Minimize:
        return QStringLiteral("MinimizeWindow");
    case Compositor::ShellWindowAction::Unminimize:
        return QStringLiteral("UnminimizeWindow");
    case Compositor::ShellWindowAction::Close:
        return QStringLiteral("CloseWindow");
    case Compositor::ShellWindowAction::Raise:
        return QStringLiteral("RaiseWindow");
    }
    return {};
}

void setError(QString *error, QString message)
{
    if (error) *error = std::move(message);
}

} // namespace

QtShellWindowActionsTransport::QtShellWindowActionsTransport(
    QDBusConnection connection, QObject *parent)
    : ShellWindowActionsTransport(parent)
    , m_connection(std::move(connection))
{
}

QtShellWindowActionsTransport::~QtShellWindowActionsTransport()
{
    stop();
}

bool QtShellWindowActionsTransport::start(QString *error)
{
    if (m_started) {
        if (error) error->clear();
        return true;
    }
    if (!m_connection.isConnected()) {
        setError(error, QStringLiteral("session D-Bus is not connected"));
        return false;
    }
    m_serviceWatcher = new QDBusServiceWatcher(
        QString::fromLatin1(ServiceName), m_connection,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this](const QString &service, const QString &,
                         const QString &newOwner) {
                if (!m_started || service != QLatin1StringView(ServiceName)) return;
                ++m_ownerGeneration;
                bindOwner(newOwner);
            });
    m_started = true;
    resolveOwner();
    if (error) error->clear();
    return true;
}

void QtShellWindowActionsTransport::stop()
{
    if (!m_started) return;
    m_started = false;
    ++m_ownerGeneration;
    if (!m_uniqueOwner.isEmpty()) {
        m_connection.disconnect(
            m_uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName),
            QString::fromLatin1(IdentityChangedSignal), this,
            SLOT(handleIdentityInvalidation()));
    }
    m_uniqueOwner.clear();
    m_identitySignalConnected = false;
    for (const auto &pending : std::as_const(m_pendingCalls)) {
        if (pending) {
            pending->disconnect(this);
            pending->deleteLater();
        }
    }
    m_pendingCalls.clear();
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
}

void QtShellWindowActionsTransport::request(
    quint64 token,
    const QString &uniqueOwner,
    Compositor::ShellWindowAction action,
    const QString &windowId,
    const Compositor::ShellWindowGeneration &generation)
{
    if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_uniqueOwner) {
        fail(token, uniqueOwner, QStringLiteral("the compositor owner is no longer current"));
        return;
    }
    const QString method = methodName(action);
    if (method.isEmpty()) {
        fail(token, uniqueOwner, QStringLiteral("the shell window action is invalid"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        uniqueOwner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(InterfaceName), method);
    call << windowId << generation.epoch << QString::number(generation.revision);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, token, uniqueOwner] {
                m_pendingCalls.removeAll(watcher);
                QDBusPendingReply<QByteArray> reply = *watcher;
                watcher->deleteLater();
                if (!m_started) return;
                if (reply.isError()) {
                    fail(token, uniqueOwner, reply.error().message());
                    return;
                }
                Q_EMIT replyReceived(token, uniqueOwner, reply.value());
            });
}

void QtShellWindowActionsTransport::requestIdentity(
    quint64 token, const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_uniqueOwner) {
        failIdentity(token, uniqueOwner,
                     QStringLiteral("the compositor owner is no longer current"));
        return;
    }
    if (!m_identitySignalConnected) {
        failIdentity(token, uniqueOwner,
                     QStringLiteral("identity invalidation subscription is unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        uniqueOwner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(InterfaceName), QString::fromLatin1(IdentityMethod));
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, token, uniqueOwner] {
                m_pendingCalls.removeAll(watcher);
                QDBusPendingReply<QByteArray> reply = *watcher;
                watcher->deleteLater();
                if (!m_started) return;
                if (reply.isError()) {
                    failIdentity(token, uniqueOwner, reply.error().message());
                    return;
                }
                Q_EMIT identityReplyReceived(token, uniqueOwner, reply.value());
            });
}

void QtShellWindowActionsTransport::resolveOwner()
{
    if (!m_started) return;
    const quint64 generation = ++m_ownerGeneration;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(DBusService), QString::fromLatin1(DBusPath),
        QString::fromLatin1(DBusInterface), QString::fromLatin1(GetNameOwnerMethod));
    call << QString::fromLatin1(ServiceName);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, generation] {
                m_pendingCalls.removeAll(watcher);
                QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_ownerGeneration) return;
                bindOwner(reply.isError() ? QString{} : reply.value());
            });
}

void QtShellWindowActionsTransport::bindOwner(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner == m_uniqueOwner) return;
    if (!m_uniqueOwner.isEmpty()) {
        m_connection.disconnect(
            m_uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName),
            QString::fromLatin1(IdentityChangedSignal), this,
            SLOT(handleIdentityInvalidation()));
    }
    m_uniqueOwner = uniqueOwner;
    m_identitySignalConnected = false;
    if (!m_uniqueOwner.isEmpty()) {
        m_identitySignalConnected = m_connection.connect(
            m_uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName),
            QString::fromLatin1(IdentityChangedSignal), this,
            SLOT(handleIdentityInvalidation()));
    }
    Q_EMIT serviceOwnerChanged(m_uniqueOwner);
}

void QtShellWindowActionsTransport::handleIdentityInvalidation()
{
    if (m_started && !m_uniqueOwner.isEmpty()) {
        Q_EMIT identityInvalidated(m_uniqueOwner);
    }
}

void QtShellWindowActionsTransport::fail(
    quint64 token, const QString &uniqueOwner, QString message)
{
    if (message.trimmed().isEmpty()) {
        message = QStringLiteral("the shell window action D-Bus request failed");
    }
    Q_EMIT requestFailed(token, uniqueOwner, message);
}

void QtShellWindowActionsTransport::failIdentity(
    quint64 token, const QString &uniqueOwner, QString message)
{
    if (message.trimmed().isEmpty()) {
        message = QStringLiteral("the active-window identity request failed");
    }
    Q_EMIT identityRequestFailed(token, uniqueOwner, message);
}

} // namespace QindaQt::ShellWindowActionsClient
