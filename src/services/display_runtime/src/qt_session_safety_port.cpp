// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_runtime/session_safety_port.h>

#include <qindaqt/services/session_lock_state/qt_session_lock_transport.h>
#include <qindaqt/services/session_lock_state/session_lock_state_monitor.h>

#include <QtCore/QPointer>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusUnixFileDescriptor>

#include <limits>
#include <memory>
#include <utility>

namespace QindaQt::DisplayRuntime
{
namespace
{

constexpr auto kLogindService = "org.freedesktop.login1";
constexpr auto kLogindPath = "/org/freedesktop/login1";
constexpr auto kLogindInterface = "org.freedesktop.login1.Manager";

using Services::SessionLockState::LockState;
using Services::SessionLockState::QtSessionLockTransport;
using Services::SessionLockState::SessionLockStateMonitor;

bool validUniqueOwner(const QString &owner)
{
    return owner.startsWith(QLatin1Char(':')) && owner.toUtf8().size() <= 255
        && !owner.contains(QLatin1Char(' '))
        && !owner.contains(QChar(u'\0'));
}

class QtSessionSafetyPort final : public QObject, public SessionSafetyPort
{
    Q_OBJECT

public:
    QtSessionSafetyPort(QDBusConnection sessionConnection,
                        QDBusConnection systemConnection)
        : m_sessionConnection(std::move(sessionConnection))
        , m_systemConnection(std::move(systemConnection))
    {
    }

    ~QtSessionSafetyPort() override { stop(); }

    void setObserver(SessionSafetyObserver *observer) override
    {
        m_observer = observer;
    }

    SessionSafetyStartStatus start(
        const qint64 expectedCompositorProcessId) override
    {
        if (m_running) {
            return SessionSafetyStartStatus::AlreadyStarted;
        }
        if (!m_sessionConnection.isConnected()
            || m_sessionConnection.interface() == nullptr
            || !m_systemConnection.isConnected()
            || m_systemConnection.interface() == nullptr) {
            return SessionSafetyStartStatus::InvalidConnection;
        }
        if (expectedCompositorProcessId <= 1
            || static_cast<quint64>(expectedCompositorProcessId)
                > static_cast<quint64>(std::numeric_limits<qint32>::max())) {
            return SessionSafetyStartStatus::InvalidCompositorProcess;
        }

        m_running = true;
        m_failed = false;
        ++m_generation;
        m_lockTransport = std::make_unique<QtSessionLockTransport>(
            m_sessionConnection, this);
        m_lockMonitor = std::make_unique<SessionLockStateMonitor>(
            *m_lockTransport, expectedCompositorProcessId,
            Services::SessionLockState::SessionLockRetryPolicy{}, this);
        QObject::connect(m_lockMonitor.get(), &SessionLockStateMonitor::stateChanged,
                         this, [this](const LockState) { updateSafety(); });
        QObject::connect(m_lockTransport.get(),
                         &QtSessionLockTransport::transportLost, this,
                         [this] { fail(QStringLiteral("session-bus-lost")); });
        QString lockError;
        if (!m_lockMonitor->start(&lockError)) {
            cleanup();
            return SessionSafetyStartStatus::LockMonitorFailed;
        }

        if (!m_systemConnection.connect(
                QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
                QStringLiteral("org.freedesktop.DBus.Local"),
                QStringLiteral("Disconnected"), this,
                SLOT(systemBusDisconnected()))) {
            cleanup();
            return SessionSafetyStartStatus::LogindRegistrationFailed;
        }
        m_systemDisconnectSubscribed = true;
        m_logindWatcher = std::make_unique<QDBusServiceWatcher>(
            QString::fromLatin1(kLogindService), m_systemConnection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        QObject::connect(m_logindWatcher.get(),
                         &QDBusServiceWatcher::serviceOwnerChanged, this,
                         &QtSessionSafetyPort::logindOwnerChanged);

        const QDBusReply<QString> owner = m_systemConnection.interface()->serviceOwner(
            QString::fromLatin1(kLogindService));
        if (!owner.isValid() || owner.value().isEmpty()
            || !bindLogindOwner(owner.value())) {
            cleanup();
            return SessionSafetyStartStatus::LogindRegistrationFailed;
        }
        updateSafety();
        return SessionSafetyStartStatus::Started;
    }

    void stop() override
    {
        cleanup();
    }

    void releaseSleepDelay() override
    {
        if (!m_running || !m_sleepPreparing) {
            return;
        }
        m_delayDescriptor = {};
        updateSafety();
    }

    bool delayHeld() const noexcept override
    {
        return m_delayDescriptor.isValid();
    }

    DisplayTransaction::SafetyState currentSafety() const noexcept override
    {
        return m_safety;
    }

private Q_SLOTS:
    void systemBusDisconnected()
    {
        fail(QStringLiteral("system-bus-lost"));
    }

    void prepareForSleep(const bool preparing)
    {
        if (!m_running || m_failed) {
            return;
        }
        if (preparing) {
            if (m_sleepPreparing) {
                return;
            }
            if (!delayHeld()) {
                fail(QStringLiteral("sleep-without-delay-authority"));
                return;
            }
            m_sleepPreparing = true;
            updateSafety();
            if (m_observer != nullptr) {
                m_observer->sessionPreparingForSleep();
            }
            return;
        }
        if (!m_sleepPreparing) {
            return;
        }
        m_sleepPreparing = false;
        m_delayDescriptor = {};
        updateSafety();
        requestDelayInhibitor();
    }

private:
    void logindOwnerChanged(const QString &, const QString &oldOwner,
                            const QString &newOwner)
    {
        if (!m_running || newOwner == m_logindOwner) {
            return;
        }
        if (!m_everBoundLogind && oldOwner.isEmpty() && !newOwner.isEmpty()) {
            if (!bindLogindOwner(newOwner)) {
                fail(QStringLiteral("logind-bind-failed"));
            }
            return;
        }
        fail(QStringLiteral("logind-owner-replaced"));
    }

    bool bindLogindOwner(const QString &owner)
    {
        if (!validUniqueOwner(owner) || !m_logindOwner.isEmpty()) {
            return false;
        }
        if (!m_systemConnection.connect(
                owner, QString::fromLatin1(kLogindPath),
                QString::fromLatin1(kLogindInterface),
                QStringLiteral("PrepareForSleep"), this,
                SLOT(prepareForSleep(bool)))) {
            return false;
        }
        m_logindOwner = owner;
        m_everBoundLogind = true;
        ++m_generation;
        requestDelayInhibitor();
        return true;
    }

    void requestDelayInhibitor()
    {
        if (!m_running || m_failed || m_logindOwner.isEmpty()
            || m_inhibitPending || delayHeld() || m_sleepPreparing) {
            return;
        }
        m_inhibitPending = true;
        const quint64 generation = m_generation;
        QDBusMessage message = QDBusMessage::createMethodCall(
            m_logindOwner, QString::fromLatin1(kLogindPath),
            QString::fromLatin1(kLogindInterface), QStringLiteral("Inhibit"));
        message.setArguments({QStringLiteral("sleep"),
                              QStringLiteral("QindaQt Display1"),
                              QStringLiteral("recover display preview before sleep"),
                              QStringLiteral("delay")});
        auto *watcher = new QDBusPendingCallWatcher(
            m_systemConnection.asyncCall(message, 5'000), this);
        QObject::connect(
            watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *finished;
                finished->deleteLater();
                if (!m_running || generation != m_generation) {
                    return;
                }
                m_inhibitPending = false;
                if (reply.isError() || !reply.value().isValid()) {
                    fail(QStringLiteral("logind-inhibit-failed"));
                    return;
                }
                m_delayDescriptor = reply.value();
                updateSafety();
                if (m_observer != nullptr) {
                    m_observer->sessionSafetyReady();
                }
            });
    }

    void updateSafety()
    {
        DisplayTransaction::SafetyState next =
            DisplayTransaction::SafetyState::Unknown;
        if (m_lockMonitor != nullptr) {
            const LockState lock = m_lockMonitor->state();
            if (lock == LockState::Locked || lock == LockState::Locking) {
                next = DisplayTransaction::SafetyState::Locked;
            } else if (lock == LockState::Unlocked && delayHeld()
                       && !m_sleepPreparing && !m_failed) {
                next = DisplayTransaction::SafetyState::Safe;
            }
        }
        if (next == m_safety) {
            return;
        }
        m_safety = next;
        if (m_observer != nullptr) {
            m_observer->sessionSafetyChanged(m_safety);
        }
    }

    void fail(const QString &reasonCode)
    {
        if (m_failed || !m_running) {
            return;
        }
        m_failed = true;
        m_delayDescriptor = {};
        updateSafety();
        SessionSafetyObserver *observer = m_observer;
        cleanup();
        if (observer != nullptr) {
            observer->sessionAuthorityLost(reasonCode);
        }
    }

    void cleanup()
    {
        if (!m_running && m_lockMonitor == nullptr && m_logindWatcher == nullptr) {
            return;
        }
        m_running = false;
        ++m_generation;
        m_inhibitPending = false;
        m_sleepPreparing = false;
        m_delayDescriptor = {};
        if (!m_logindOwner.isEmpty()) {
            (void)m_systemConnection.disconnect(
                m_logindOwner, QString::fromLatin1(kLogindPath),
                QString::fromLatin1(kLogindInterface),
                QStringLiteral("PrepareForSleep"), this,
                SLOT(prepareForSleep(bool)));
        }
        m_logindOwner.clear();
        m_everBoundLogind = false;
        if (m_logindWatcher != nullptr) {
            // AGENT-GUARD: Owner loss invokes cleanup from this watcher's own
            // signal. Synchronous destruction would delete the sender while
            // Qt is still dispatching it.
            m_logindWatcher->disconnect(this);
            m_logindWatcher.release()->deleteLater();
        }
        if (m_systemDisconnectSubscribed) {
            (void)m_systemConnection.disconnect(
                QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
                QStringLiteral("org.freedesktop.DBus.Local"),
                QStringLiteral("Disconnected"), this,
                SLOT(systemBusDisconnected()));
            m_systemDisconnectSubscribed = false;
        }
        if (m_lockMonitor != nullptr) {
            m_lockMonitor->stop();
            m_lockMonitor.release()->deleteLater();
        }
        if (m_lockTransport != nullptr) {
            // transportLost can also be the active sender of fail(). Its
            // QObject parent owns it until this deferred deletion runs.
            m_lockTransport->disconnect(this);
            m_lockTransport.release()->deleteLater();
        }
        m_safety = DisplayTransaction::SafetyState::Unknown;
    }

    QDBusConnection m_sessionConnection;
    QDBusConnection m_systemConnection;
    std::unique_ptr<QtSessionLockTransport> m_lockTransport;
    std::unique_ptr<SessionLockStateMonitor> m_lockMonitor;
    std::unique_ptr<QDBusServiceWatcher> m_logindWatcher;
    QDBusUnixFileDescriptor m_delayDescriptor;
    SessionSafetyObserver *m_observer = nullptr;
    QString m_logindOwner;
    quint64 m_generation = 0;
    DisplayTransaction::SafetyState m_safety =
        DisplayTransaction::SafetyState::Unknown;
    bool m_running = false;
    bool m_failed = false;
    bool m_everBoundLogind = false;
    bool m_inhibitPending = false;
    bool m_sleepPreparing = false;
    bool m_systemDisconnectSubscribed = false;
};

} // namespace

std::unique_ptr<SessionSafetyPort> makeQtSessionSafetyPort(
    const QDBusConnection &sessionConnection,
    const QDBusConnection &systemConnection)
{
    return std::make_unique<QtSessionSafetyPort>(sessionConnection,
                                                 systemConnection);
}

} // namespace QindaQt::DisplayRuntime

#include "qt_session_safety_port.moc"
