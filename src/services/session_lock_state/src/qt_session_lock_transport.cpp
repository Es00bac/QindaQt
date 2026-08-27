// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/session_lock_state/qt_session_lock_transport.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>

#include <array>
#include <utility>

namespace QindaQt::Services::SessionLockState {
namespace {

constexpr auto DBusService = "org.freedesktop.DBus";
constexpr auto DBusPath = "/org/freedesktop/DBus";
constexpr auto DBusInterface = "org.freedesktop.DBus";
constexpr auto DBusLocalPath = "/org/freedesktop/DBus/Local";
constexpr auto DBusLocalInterface = "org.freedesktop.DBus.Local";
constexpr auto ScreenSaverPath = "/ScreenSaver";
constexpr auto FreedesktopScreenSaverInterface = "org.freedesktop.ScreenSaver";
constexpr auto KdeScreenSaverInterface = "org.kde.screensaver";
constexpr int RequestTimeoutMilliseconds = 2'000;

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

ObservedService serviceFromName(const QString &name, bool *ok)
{
    constexpr std::array services{
        ObservedService::Compositor,
        ObservedService::FreedesktopScreenSaver,
        ObservedService::KdeScreenSaver,
    };
    for (const auto service : services) {
        if (name == observedServiceName(service)) {
            *ok = true;
            return service;
        }
    }
    *ok = false;
    return ObservedService::Compositor;
}

} // namespace

QtSessionLockTransport::QtSessionLockTransport(QDBusConnection connection,
                                               QObject *parent)
    : SessionLockTransport(parent)
    , m_connection(std::move(connection))
{
}

QtSessionLockTransport::~QtSessionLockTransport()
{
    stop();
}

bool QtSessionLockTransport::start(QString *error)
{
    if (m_started) {
        setError(error, {});
        return true;
    }
    if (!m_connection.isConnected()) {
        setError(error, QStringLiteral("session lock D-Bus is not connected"));
        return false;
    }

    // org.freedesktop.DBus.Local.Disconnected is generated locally by the
    // D-Bus implementation even when the daemon vanishes, unlike well-known
    // owner notifications which cannot be relied upon after transport loss.
    m_disconnectSubscribed = m_connection.connect(
        QString{}, QString::fromLatin1(DBusLocalPath),
        QString::fromLatin1(DBusLocalInterface), QStringLiteral("Disconnected"),
        this, SLOT(handleBusDisconnected()));
    if (!m_disconnectSubscribed) {
        setError(error,
                 QStringLiteral("could not observe session lock D-Bus disconnects"));
        return false;
    }

    // Install all owner watches before start() returns. The monitor issues its
    // initial GetNameOwner calls only afterward, closing the startup race.
    m_serviceWatcher = new QDBusServiceWatcher(
        observedServiceName(ObservedService::Compositor), m_connection,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    m_serviceWatcher->addWatchedService(
        observedServiceName(ObservedService::FreedesktopScreenSaver));
    m_serviceWatcher->addWatchedService(
        observedServiceName(ObservedService::KdeScreenSaver));
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &serviceName, const QString &,
                   const QString &newOwner) {
                if (!m_started) {
                    return;
                }
                bool ok = false;
                const ObservedService service = serviceFromName(serviceName, &ok);
                if (ok) {
                    Q_EMIT serviceOwnerChanged(service, newOwner);
                }
            });

    ++m_lifetimeGeneration;
    m_started = true;
    setError(error, {});
    return true;
}

void QtSessionLockTransport::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    ++m_lifetimeGeneration;
    clearRuntimeResources();
}

void QtSessionLockTransport::clearRuntimeResources()
{
    unsubscribeFromLockSignals();
    if (m_disconnectSubscribed) {
        m_connection.disconnect(
            QString{}, QString::fromLatin1(DBusLocalPath),
            QString::fromLatin1(DBusLocalInterface),
            QStringLiteral("Disconnected"), this,
            SLOT(handleBusDisconnected()));
        m_disconnectSubscribed = false;
    }
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

void QtSessionLockTransport::requestServiceOwner(quint64 generation,
                                                 ObservedService service)
{
    if (!m_started) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(DBusService), QString::fromLatin1(DBusPath),
        QString::fromLatin1(DBusInterface), QStringLiteral("GetNameOwner"));
    message << observedServiceName(service);
    const quint64 lifetime = m_lifetimeGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, RequestTimeoutMilliseconds), this);
    track(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, service, lifetime] {
                finish(watcher);
                if (!m_started || lifetime != m_lifetimeGeneration) {
                    return;
                }
                const QDBusPendingReply<QString> reply = *watcher;
                if (reply.isError()) {
                    if (reply.error().name() ==
                        QLatin1StringView("org.freedesktop.DBus.Error.NameHasNoOwner")) {
                        Q_EMIT serviceOwnerResolved(generation, service, QString{});
                    } else {
                        emitFailure(generation, 0, LockRequest::ServiceOwner,
                                    service, {}, reply.error().name(),
                                    reply.error().message());
                    }
                    watcher->deleteLater();
                    return;
                }
                Q_EMIT serviceOwnerResolved(generation, service, reply.value());
                watcher->deleteLater();
            });
}

void QtSessionLockTransport::requestUnixProcessId(
    quint64 generation, const QString &uniqueOwner)
{
    if (!m_started) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(DBusService), QString::fromLatin1(DBusPath),
        QString::fromLatin1(DBusInterface),
        QStringLiteral("GetConnectionUnixProcessID"));
    message << uniqueOwner;
    const quint64 lifetime = m_lifetimeGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, RequestTimeoutMilliseconds), this);
    track(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, uniqueOwner, lifetime] {
                finish(watcher);
                if (!m_started || lifetime != m_lifetimeGeneration) {
                    return;
                }
                const QDBusPendingReply<quint32> reply = *watcher;
                if (reply.isError()) {
                    emitFailure(generation, 0, LockRequest::UnixProcessId,
                                ObservedService::Compositor, uniqueOwner,
                                reply.error().name(), reply.error().message());
                    watcher->deleteLater();
                    return;
                }
                Q_EMIT unixProcessIdResolved(generation, uniqueOwner,
                                             reply.value());
                watcher->deleteLater();
            });
}

bool QtSessionLockTransport::subscribeToLockSignals(
    const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner.isEmpty()) {
        return false;
    }
    unsubscribeFromLockSignals();
    const bool aboutConnected = m_connection.connect(
        uniqueOwner, QString::fromLatin1(ScreenSaverPath),
        QString::fromLatin1(KdeScreenSaverInterface), QStringLiteral("AboutToLock"),
        this, SLOT(handleAboutToLock()));
    if (!aboutConnected) {
        return false;
    }
    const bool activeConnected = m_connection.connect(
        uniqueOwner, QString::fromLatin1(ScreenSaverPath),
        QString::fromLatin1(FreedesktopScreenSaverInterface),
        QStringLiteral("ActiveChanged"),
        this, SLOT(handleActiveChanged(bool)));
    if (!activeConnected) {
        m_connection.disconnect(
            uniqueOwner, QString::fromLatin1(ScreenSaverPath),
            QString::fromLatin1(KdeScreenSaverInterface),
            QStringLiteral("AboutToLock"), this, SLOT(handleAboutToLock()));
        return false;
    }
    m_signalOwner = uniqueOwner;
    return true;
}

void QtSessionLockTransport::unsubscribeFromLockSignals()
{
    if (m_signalOwner.isEmpty()) {
        return;
    }
    m_connection.disconnect(
        m_signalOwner, QString::fromLatin1(ScreenSaverPath),
        QString::fromLatin1(KdeScreenSaverInterface),
        QStringLiteral("AboutToLock"),
        this, SLOT(handleAboutToLock()));
    m_connection.disconnect(
        m_signalOwner, QString::fromLatin1(ScreenSaverPath),
        QString::fromLatin1(FreedesktopScreenSaverInterface),
        QStringLiteral("ActiveChanged"),
        this, SLOT(handleActiveChanged(bool)));
    m_signalOwner.clear();
}

void QtSessionLockTransport::requestActiveState(
    quint64 generation, quint64 serial, const QString &uniqueOwner)
{
    if (!m_started) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        uniqueOwner, QString::fromLatin1(ScreenSaverPath),
        QString::fromLatin1(FreedesktopScreenSaverInterface),
        QStringLiteral("GetActive"));
    const quint64 lifetime = m_lifetimeGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, RequestTimeoutMilliseconds), this);
    track(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, serial, uniqueOwner, lifetime] {
                finish(watcher);
                if (!m_started || lifetime != m_lifetimeGeneration) {
                    return;
                }
                const QDBusPendingReply<bool> reply = *watcher;
                if (reply.isError()) {
                    emitFailure(generation, serial, LockRequest::ActiveState,
                                ObservedService::FreedesktopScreenSaver,
                                uniqueOwner, reply.error().name(),
                                reply.error().message());
                    watcher->deleteLater();
                    return;
                }
                Q_EMIT activeStateResolved(generation, serial, uniqueOwner,
                                           reply.value());
                watcher->deleteLater();
            });
}

void QtSessionLockTransport::scheduleActiveRetry(
    quint64 generation, quint64 serial, int delayMilliseconds)
{
    if (!m_started || delayMilliseconds <= 0) {
        return;
    }
    const quint64 lifetime = m_lifetimeGeneration;
    QTimer::singleShot(delayMilliseconds, this,
                       [this, generation, serial, lifetime] {
                           if (m_started && lifetime == m_lifetimeGeneration) {
                               Q_EMIT activeRetryReady(generation, serial);
                           }
                       });
}

void QtSessionLockTransport::handleAboutToLock()
{
    if (m_started && !m_signalOwner.isEmpty()) {
        Q_EMIT aboutToLock(m_signalOwner);
    }
}

void QtSessionLockTransport::handleBusDisconnected()
{
    if (!m_started) {
        return;
    }
    // Fence every pending callback before notifying policy. The QDBusConnection
    // object cannot be reused until a caller explicitly stops/restarts against
    // a connected transport, so start() must no longer report idempotent
    // success after this terminal loss.
    m_started = false;
    ++m_lifetimeGeneration;
    clearRuntimeResources();
    Q_EMIT transportLost();
}

void QtSessionLockTransport::handleActiveChanged(bool active)
{
    if (m_started && !m_signalOwner.isEmpty()) {
        Q_EMIT activeChanged(m_signalOwner, active);
    }
}

void QtSessionLockTransport::track(QDBusPendingCallWatcher *watcher)
{
    m_pendingCalls.append(watcher);
}

void QtSessionLockTransport::finish(QDBusPendingCallWatcher *watcher)
{
    m_pendingCalls.removeAll(watcher);
}

void QtSessionLockTransport::emitFailure(
    quint64 generation, quint64 serial, LockRequest request,
    ObservedService service, const QString &uniqueOwner,
    const QString &errorName, const QString &message)
{
    const QString normalizedName = errorName.trimmed().isEmpty()
        ? QStringLiteral("org.freedesktop.DBus.Error.Failed")
        : errorName;
    const QString normalizedMessage = message.trimmed().isEmpty()
        ? QStringLiteral("session lock D-Bus request failed")
        : message;
    Q_EMIT requestFailed(generation, serial, request, service, uniqueOwner,
                         normalizedName, normalizedMessage);
}

} // namespace QindaQt::Services::SessionLockState
