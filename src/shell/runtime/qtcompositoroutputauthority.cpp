// SPDX-License-Identifier: GPL-3.0-or-later
#include "qtcompositoroutputauthority.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

#include <algorithm>
#include <array>
#include <utility>

namespace QindaQt::Shell {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/Compositor";
constexpr auto InterfaceName = "org.qindaqt.Compositor1";
constexpr auto OutputsMethod = "Outputs";
constexpr auto OutputsChangedSignal = "OutputsChanged";
constexpr auto DBusService = "org.freedesktop.DBus";
constexpr auto DBusPath = "/org/freedesktop/DBus";
constexpr auto DBusInterface = "org.freedesktop.DBus";
constexpr auto GetNameOwnerMethod = "GetNameOwner";
constexpr int DBusTimeoutMilliseconds = 2'000;
constexpr std::array RetryMilliseconds{100, 250, 500, 1'000, 2'000, 5'000};

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

QtCompositorOutputAuthority::QtCompositorOutputAuthority(
    QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, [this] {
        if (!m_started) {
            return;
        }
        if (m_uniqueOwner.isEmpty()) {
            resolveInitialOwner();
        } else {
            requestSnapshot();
        }
    });
}

QtCompositorOutputAuthority::~QtCompositorOutputAuthority()
{
    stop();
}

bool QtCompositorOutputAuthority::start(QString *error)
{
    if (m_started) {
        setError(error, {});
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
                if (!m_started || service != QLatin1StringView(ServiceName)) {
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

void QtCompositorOutputAuthority::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    ++m_resolutionGeneration;
    ++m_requestSerial;
    m_retryTimer.stop();
    if (!m_uniqueOwner.isEmpty()) {
        (void)m_connection.disconnect(
            m_uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName),
            QString::fromLatin1(OutputsChangedSignal), this,
            SLOT(outputsChanged()));
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
    m_inFlight = false;
    m_dirty = false;
    m_retryIndex = 0;
    publish({}, false);
}

const std::optional<CompositorOutputAuthorityFrame> &
QtCompositorOutputAuthority::frame() const noexcept
{
    return m_frame;
}

void QtCompositorOutputAuthority::outputsChanged()
{
    if (!m_started || m_uniqueOwner.isEmpty()) {
        return;
    }
    // AGENT-GUARD: OutputsChanged is only an invalidation hint. Withdraw the
    // old semantic order immediately; never route a new visibility generation
    // through a previous primary while the complete Outputs read is pending.
    publish({});
    m_dirty = true;
    if (!m_inFlight) {
        requestSnapshot();
    }
}

void QtCompositorOutputAuthority::resolveInitialOwner()
{
    if (!m_started) {
        return;
    }
    const quint64 generation = ++m_resolutionGeneration;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(DBusService), QString::fromLatin1(DBusPath),
        QString::fromLatin1(DBusInterface),
        QString::fromLatin1(GetNameOwnerMethod));
    message << QString::fromLatin1(ServiceName);
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

void QtCompositorOutputAuthority::bindOwner(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner == m_uniqueOwner) {
        return;
    }
    ++m_requestSerial;
    m_retryTimer.stop();
    if (!m_uniqueOwner.isEmpty()) {
        (void)m_connection.disconnect(
            m_uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName),
            QString::fromLatin1(OutputsChangedSignal), this,
            SLOT(outputsChanged()));
    }
    m_uniqueOwner.clear();
    m_inFlight = false;
    m_dirty = false;
    m_retryIndex = 0;
    publish({});
    if (uniqueOwner.isEmpty()) {
        return;
    }
    // Subscribe to the exact owner before the first read so a configuration
    // change between resolution and reply cannot be missed.
    if (!m_connection.connect(
            uniqueOwner, QString::fromLatin1(ObjectPath),
            QString::fromLatin1(InterfaceName),
            QString::fromLatin1(OutputsChangedSignal), this,
            SLOT(outputsChanged()))) {
        scheduleRetry();
        return;
    }
    m_uniqueOwner = uniqueOwner;
    requestSnapshot();
}

void QtCompositorOutputAuthority::requestSnapshot()
{
    if (!m_started || m_uniqueOwner.isEmpty() || m_inFlight) {
        return;
    }
    m_retryTimer.stop();
    m_dirty = false;
    m_inFlight = true;
    const QString requestedOwner = m_uniqueOwner;
    const quint64 serial = ++m_requestSerial;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        requestedOwner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(InterfaceName), QString::fromLatin1(OutputsMethod));
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(message, DBusTimeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, requestedOwner, serial] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<QByteArray> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || serial != m_requestSerial
                    || requestedOwner != m_uniqueOwner) {
                    return;
                }
                m_inFlight = false;
                if (m_dirty) {
                    requestSnapshot();
                    return;
                }
                if (reply.isError()) {
                    publish({});
                    scheduleRetry();
                    return;
                }
                auto decoded = CompositorOutputAuthorityDecoder::decode(
                    reply.value(), requestedOwner);
                if (!decoded.ok()) {
                    publish({});
                    scheduleRetry();
                    return;
                }
                m_retryIndex = 0;
                publish(std::move(decoded.frame));
            });
}

void QtCompositorOutputAuthority::scheduleRetry()
{
    if (!m_started || m_retryTimer.isActive()) {
        return;
    }
    const qsizetype index = std::min(
        m_retryIndex, static_cast<qsizetype>(RetryMilliseconds.size() - 1));
    m_retryTimer.start(RetryMilliseconds.at(static_cast<std::size_t>(index)));
    if (m_retryIndex < static_cast<qsizetype>(RetryMilliseconds.size() - 1)) {
        ++m_retryIndex;
    }
}

void QtCompositorOutputAuthority::publish(
    std::optional<CompositorOutputAuthorityFrame> frame, bool notify)
{
    if (frame == m_frame) {
        return;
    }
    m_frame = std::move(frame);
    if (notify) {
        Q_EMIT stateChanged();
    }
}

} // namespace QindaQt::Shell
