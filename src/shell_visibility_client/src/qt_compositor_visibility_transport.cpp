// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_visibility_client/qt_compositor_visibility_transport.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>

#include <utility>

namespace QindaQt::ShellVisibilityClient {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/Compositor";
constexpr auto InterfaceName = "org.qindaqt.Compositor1";
constexpr auto SnapshotMethod = "ShellVisibilitySnapshot";
constexpr auto ChangedSignal = "ShellVisibilityChanged";
constexpr auto DBusService = "org.freedesktop.DBus";
constexpr auto DBusPath = "/org/freedesktop/DBus";
constexpr auto DBusInterface = "org.freedesktop.DBus";
constexpr auto GetNameOwnerMethod = "GetNameOwner";
constexpr int DBusTimeoutMilliseconds = 2000;

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

} // namespace

QtCompositorVisibilityTransport::QtCompositorVisibilityTransport(
    QDBusConnection connection,
    QObject *parent)
    : CompositorVisibilityTransport(parent)
    , m_connection(std::move(connection))
{
}

QtCompositorVisibilityTransport::~QtCompositorVisibilityTransport()
{
    stop();
}

bool QtCompositorVisibilityTransport::start(QString *error)
{
    if (m_started) {
        if (error) {
            error->clear();
        }
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
            this,
            [this](const QString &service, const QString &, const QString &newOwner) {
                if (!m_started || service != QLatin1StringView(ServiceName)) {
                    return;
                }
                ++m_resolutionGeneration;
                bindOwner(newOwner);
            });
    m_started = true;
    resolveInitialOwner();
    if (error) {
        error->clear();
    }
    return true;
}

void QtCompositorVisibilityTransport::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    ++m_resolutionGeneration;
    if (!m_uniqueOwner.isEmpty()) {
        m_connection.disconnect(
            m_uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName), QString::fromLatin1(ChangedSignal),
            this, SLOT(handleInvalidation()));
    }
    m_uniqueOwner.clear();
    for (auto *pending : std::as_const(m_pendingCalls)) {
        if (pending) {
            pending->disconnect(this);
            pending->deleteLater();
        }
    }
    m_pendingCalls.clear();
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
}

void QtCompositorVisibilityTransport::requestSnapshot(
    quint64 token, const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_uniqueOwner) {
        failRequest(token, uniqueOwner,
                    QStringLiteral("snapshot request owner is no longer current"));
        return;
    }

    const QDBusMessage message = QDBusMessage::createMethodCall(
        uniqueOwner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(InterfaceName), QString::fromLatin1(SnapshotMethod));
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, token, uniqueOwner] {
                m_pendingCalls.removeAll(watcher);
                QDBusPendingReply<QByteArray> reply = *watcher;
                watcher->deleteLater();
                if (!m_started) {
                    return;
                }
                if (reply.isError()) {
                    failRequest(token, uniqueOwner, reply.error().message());
                    return;
                }
                Q_EMIT snapshotReceived(token, uniqueOwner, reply.value());
            });
}

void QtCompositorVisibilityTransport::handleInvalidation()
{
    if (m_started && !m_uniqueOwner.isEmpty()) {
        Q_EMIT snapshotInvalidated(m_uniqueOwner);
    }
}

void QtCompositorVisibilityTransport::resolveInitialOwner()
{
    if (!m_started) {
        return;
    }
    const quint64 generation = ++m_resolutionGeneration;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(DBusService), QString::fromLatin1(DBusPath),
        QString::fromLatin1(DBusInterface), QString::fromLatin1(GetNameOwnerMethod));
    message << QString::fromLatin1(ServiceName);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, generation] {
                m_pendingCalls.removeAll(watcher);
                QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_resolutionGeneration) {
                    return;
                }
                if (reply.isError()) {
                    bindOwner({});
                    return;
                }
                bindOwner(reply.value());
            });
}

void QtCompositorVisibilityTransport::bindOwner(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner == m_uniqueOwner) {
        return;
    }
    const QString previous = m_uniqueOwner;
    if (!previous.isEmpty()) {
        m_connection.disconnect(
            previous, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName), QString::fromLatin1(ChangedSignal),
            this, SLOT(handleInvalidation()));
    }
    m_uniqueOwner.clear();

    if (uniqueOwner.isEmpty()) {
        if (!previous.isEmpty()) {
            Q_EMIT serviceOwnerChanged({});
        }
        return;
    }

    // AGENT-GUARD: Subscribe to the unique owner before exposing it to the
    // client. The client's immediate read can then never miss an invalidation
    // between owner resolution and its first snapshot request.
    const bool connected = m_connection.connect(
        uniqueOwner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(InterfaceName), QString::fromLatin1(ChangedSignal),
        this, SLOT(handleInvalidation()));
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

void QtCompositorVisibilityTransport::failRequest(
    quint64 token, const QString &uniqueOwner, QString message)
{
    if (message.trimmed().isEmpty()) {
        message = QStringLiteral("compositor snapshot D-Bus request failed");
    }
    Q_EMIT snapshotFailed(token, uniqueOwner, message);
}

} // namespace QindaQt::ShellVisibilityClient
