// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session_supervisor/session_service.h>

#include <qindaqt/session_supervisor/session_process_supervisor.h>

#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusReply>
#include <QTimer>

#include <utility>

namespace QindaQt::SessionSupervisor {
namespace {

constexpr auto ServiceName = "org.qindaqt.Session1";
constexpr auto ObjectPath = "/org/qindaqt/Session1";
constexpr auto UnauthorizedError = "org.qindaqt.Session1.Error.Unauthorized";

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

class SessionService::Endpoint final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Session1")

public:
    Endpoint(SessionProcessSupervisor &supervisor, QDBusConnection connection)
        : m_supervisor(supervisor)
        , m_connection(std::move(connection))
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE bool CanLogout()
    {
        if (!authorizedCaller()) {
            sendErrorReply(QString::fromLatin1(UnauthorizedError),
                           QStringLiteral("caller is not the supervised shell"));
            return false;
        }
        return m_supervisor.canLogout();
    }

    Q_SCRIPTABLE void Logout()
    {
        if (!authorizedCaller()) {
            sendErrorReply(QString::fromLatin1(UnauthorizedError),
                           QStringLiteral("caller is not the supervised shell"));
            return;
        }
        if (!m_supervisor.canLogout()) {
            sendErrorReply(QStringLiteral("org.qindaqt.Session1.Error.Unavailable"),
                           QStringLiteral("session logout is unavailable"));
            return;
        }

        // The caller is one of the children about to be stopped. Queue the
        // shutdown only after its empty success reply has entered the private
        // bus, so the client can distinguish acceptance from transport loss.
        setDelayedReply(true);
        m_connection.send(message().createReply());
        QTimer::singleShot(0, &m_supervisor,
                           &SessionProcessSupervisor::requestLogout);
    }

private:
    [[nodiscard]] bool authorizedCaller() const
    {
        if (!calledFromDBus() || m_connection.interface() == nullptr) {
            return false;
        }
        const qint64 shellProcessId = m_supervisor.shellProcessId();
        if (shellProcessId <= 1) {
            return false;
        }
        const QDBusReply<quint32> callerProcessId =
            m_connection.interface()->servicePid(message().service());
        // AGENT-GUARD: resolve credentials for every invocation. Caching the
        // first shell's unique name or PID would authorize stale callers after
        // the supervisor's one allowed shell replacement.
        return callerProcessId.isValid()
            && callerProcessId.value() == static_cast<quint32>(shellProcessId);
    }

    SessionProcessSupervisor &m_supervisor;
    QDBusConnection m_connection;
};

SessionService::SessionService(SessionProcessSupervisor &supervisor,
                               QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_supervisor(supervisor)
    , m_connection(std::move(connection))
{
}

SessionService::~SessionService()
{
    stop();
}

bool SessionService::start(QString *error)
{
    if (m_active) {
        setError(error, QStringLiteral("Session1 is already active"));
        return false;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        setError(error, QStringLiteral("session bus is unavailable"));
        return false;
    }
    m_endpoint = std::make_unique<Endpoint>(m_supervisor, m_connection);
    if (!m_connection.registerObject(
            QString::fromLatin1(ObjectPath), m_endpoint.get(),
            QDBusConnection::ExportScriptableSlots)) {
        m_endpoint.reset();
        setError(error, QStringLiteral("could not register the Session1 object"));
        return false;
    }
    const auto reply = m_connection.interface()->registerService(
        QString::fromLatin1(ServiceName),
        QDBusConnectionInterface::DontQueueService,
        QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid()
        || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(ObjectPath));
        m_endpoint.reset();
        setError(error, QStringLiteral("could not own the Session1 bus name"));
        return false;
    }
    m_active = true;
    setError(error, {});
    return true;
}

void SessionService::stop() noexcept
{
    if (!m_active) {
        return;
    }
    m_connection.unregisterObject(QString::fromLatin1(ObjectPath));
    m_connection.unregisterService(QString::fromLatin1(ServiceName));
    m_endpoint.reset();
    m_active = false;
}

bool SessionService::active() const noexcept
{
    return m_active;
}

} // namespace QindaQt::SessionSupervisor

#include "session_service.moc"
