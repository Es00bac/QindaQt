// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_logind_backlight_service.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusVariant>

namespace QindaQt::Tests {
namespace {

constexpr char kServiceName[] = "org.freedesktop.login1";
constexpr char kManagerPath[] = "/org/freedesktop/login1";
constexpr char kManagerInterface[] = "org.freedesktop.login1.Manager";
constexpr char kSessionInterface[] = "org.freedesktop.login1.Session";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";
constexpr char kAutoSessionPath[] = "/org/freedesktop/login1/session/auto";

} // namespace

QDBusArgument &operator<<(QDBusArgument &argument,
                          const FakeLogindSessionWire &value)
{
    argument.beginStructure();
    argument << value.sessionId << value.uid << value.userName << value.seatId
             << value.sessionPath;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                FakeLogindSessionWire &value)
{
    argument.beginStructure();
    argument >> value.sessionId >> value.uid >> value.userName >> value.seatId
        >> value.sessionPath;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const FakeLogindSeatWire &value)
{
    argument.beginStructure();
    argument << value.seatId << value.seatPath;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                FakeLogindSeatWire &value)
{
    argument.beginStructure();
    argument >> value.seatId >> value.seatPath;
    argument.endStructure();
    return argument;
}

FakeLogindBacklightService::FakeLogindBacklightService(
    const QDBusConnection &connection, QObject *parent)
    : QDBusVirtualObject(parent)
    , m_connection(connection)
{
    qDBusRegisterMetaType<FakeLogindSessionWire>();
    qDBusRegisterMetaType<QList<FakeLogindSessionWire>>();
    qDBusRegisterMetaType<FakeLogindSeatWire>();
    m_autoSession.path = QString::fromLatin1(kAutoSessionPath);
}

FakeLogindBacklightService::~FakeLogindBacklightService()
{
    unregisterService();
}

bool FakeLogindBacklightService::registerService()
{
    if (!m_connection.registerVirtualObject(QString::fromLatin1(kManagerPath), this,
                                            QDBusConnection::SubPath)) {
        return false;
    }
    if (!m_connection.registerService(QString::fromLatin1(kServiceName))) {
        m_connection.unregisterObject(QString::fromLatin1(kManagerPath));
        return false;
    }
    return true;
}

void FakeLogindBacklightService::unregisterService()
{
    m_connection.unregisterObject(QString::fromLatin1(kManagerPath));
    m_connection.unregisterService(QString::fromLatin1(kServiceName));
}

void FakeLogindBacklightService::setAutoSession(const QString &seatId,
                                                const bool active)
{
    const QMutexLocker locker(&m_mutex);
    m_autoSessionExists = true;
    m_autoSession.seatId = seatId;
    m_autoSession.active = active;
    m_autoSession.path = QString::fromLatin1(kAutoSessionPath);
}

void FakeLogindBacklightService::setSessions(const QList<SessionSpec> &sessions)
{
    const QMutexLocker locker(&m_mutex);
    m_sessions = sessions;
}

void FakeLogindBacklightService::setListSessionsFails(const bool fails)
{
    const QMutexLocker locker(&m_mutex);
    m_listSessionsFails = fails;
}

void FakeLogindBacklightService::setBrightnessError(const QString &errorName,
                                                    const QString &errorText)
{
    const QMutexLocker locker(&m_mutex);
    m_brightnessErrorName = errorName;
    m_brightnessErrorText = errorText;
}

void FakeLogindBacklightService::clearBrightnessError()
{
    const QMutexLocker locker(&m_mutex);
    m_brightnessErrorName.clear();
    m_brightnessErrorText.clear();
}

void FakeLogindBacklightService::setUnknownSessionPath(const QString &path)
{
    const QMutexLocker locker(&m_mutex);
    m_unknownSessionPath = path;
}

void FakeLogindBacklightService::setActiveProbeDelayMs(const int delayMs)
{
    const QMutexLocker locker(&m_mutex);
    m_activeProbeDelayMs = delayMs;
}

int FakeLogindBacklightService::activeProbeCalls() const
{
    const QMutexLocker locker(&m_mutex);
    return m_activeProbeCalls;
}

void FakeLogindBacklightService::resetCounters()
{
    const QMutexLocker locker(&m_mutex);
    m_activeProbeCalls = 0;
    m_brightnessCalls.clear();
    m_listSessionsCalls = 0;
}

QList<FakeLogindBacklightService::BrightnessCall>
FakeLogindBacklightService::brightnessCalls() const
{
    const QMutexLocker locker(&m_mutex);
    return m_brightnessCalls;
}

int FakeLogindBacklightService::listSessionsCalls() const
{
    const QMutexLocker locker(&m_mutex);
    return m_listSessionsCalls;
}

QString FakeLogindBacklightService::introspect(const QString &path) const
{
    Q_UNUSED(path)
    return QStringLiteral("<node/>");
}

void FakeLogindBacklightService::sendError(const QDBusMessage &message,
                                           const QString &name, const QString &text)
{
    m_connection.send(message.createErrorReply(name, text));
}

std::optional<FakeLogindBacklightService::SessionSpec>
FakeLogindBacklightService::sessionForPath(const QString &path) const
{
    if (path == QString::fromLatin1(kAutoSessionPath)) {
        return m_autoSessionExists ? std::optional<SessionSpec>(m_autoSession)
                                   : std::nullopt;
    }
    for (const SessionSpec &session : m_sessions) {
        if (session.path == path) {
            return session;
        }
    }
    return std::nullopt;
}

bool FakeLogindBacklightService::handleMessage(const QDBusMessage &message,
                                               const QDBusConnection &connection)
{
    Q_UNUSED(connection)
    if (message.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    // Serves on the worker thread while the test thread configures the fake.
    QMutexLocker locker(&m_mutex);
    const QString member = message.member();
    const QString path = message.path();

    if (path == QString::fromLatin1(kManagerPath)) {
        if (message.interface() != QString::fromLatin1(kManagerInterface)
            || member != QStringLiteral("ListSessions")) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                      QStringLiteral("Unsupported manager call ") + member);
            return true;
        }
        ++m_listSessionsCalls;
        if (m_listSessionsFails) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                      QStringLiteral("Fake ListSessions failure"));
            return true;
        }
        QList<FakeLogindSessionWire> wire;
        for (const SessionSpec &session : m_sessions) {
            wire.push_back({session.sessionId, session.uid, session.userName,
                            session.seatId, QDBusObjectPath(session.path)});
        }
        QDBusMessage reply = message.createReply();
        reply.setArguments({QVariant::fromValue(wire)});
        m_connection.send(reply);
        return true;
    }

    if (!m_unknownSessionPath.isEmpty() && path == m_unknownSessionPath) {
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"),
                  QStringLiteral("Fake dead session ") + path);
        return true;
    }

    const std::optional<SessionSpec> session = sessionForPath(path);
    if (!session.has_value()) {
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"),
                  QStringLiteral("No such session ") + path);
        return true;
    }

    if (message.interface() == QString::fromLatin1(kPropertiesInterface)) {
        if (member != QStringLiteral("Get") || message.arguments().size() != 2
            || message.arguments().at(0).toString()
                != QString::fromLatin1(kSessionInterface)) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                      QStringLiteral("Bad session property call"));
            return true;
        }
        const QString property = message.arguments().at(1).toString();
        QDBusMessage reply = message.createReply();
        if (property == QStringLiteral("Seat")) {
            // The real daemon reports an empty seat id (with the root seat
            // path) for a session that has no seat, not an error.
            const FakeLogindSeatWire seat{
                session->seatId,
                QDBusObjectPath(session->seatId.isEmpty()
                                    ? QStringLiteral("/org/freedesktop/login1/seat/auto")
                                    : QStringLiteral("/org/freedesktop/login1/seat/")
                                        + session->seatId)};
            reply.setArguments({QVariant::fromValue(QDBusVariant(QVariant::fromValue(seat)))});
            m_connection.send(reply);
            return true;
        }
        if (property == QStringLiteral("Active")) {
            ++m_activeProbeCalls;
            const int delayMs = m_activeProbeDelayMs;
            reply.setArguments(
                {QVariant::fromValue(QDBusVariant(QVariant(session->active)))});
            if (delayMs > 0) {
                // AGENT-GUARD: stall with the lock released. The test thread is
                // blocked inside the writer's own call, but it must still be
                // able to read these counters afterwards without waiting for
                // every queued stall.
                locker.unlock();
                QThread::msleep(static_cast<unsigned long>(delayMs));
            }
            m_connection.send(reply);
            return true;
        }
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownProperty"),
                  QStringLiteral("Unknown session property ") + property);
        return true;
    }

    if (message.interface() == QString::fromLatin1(kSessionInterface)
        && member == QStringLiteral("SetBrightness")) {
        const QList<QVariant> arguments = message.arguments();
        if (arguments.size() != 3) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                      QStringLiteral("SetBrightness takes ssu"));
            return true;
        }
        m_brightnessCalls.push_back({path, arguments.at(0).toString(),
                                     arguments.at(1).toString(),
                                     arguments.at(2).toUInt()});
        if (!m_brightnessErrorName.isEmpty()) {
            sendError(message, m_brightnessErrorName, m_brightnessErrorText);
            return true;
        }
        m_connection.send(message.createReply());
        return true;
    }

    sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
              QStringLiteral("Unknown session call ") + member);
    return true;
}

void FakeLogindBacklightServiceThread::Worker::run()
{
    const QString connectionName =
        QStringLiteral("qindaqt-fake-logind-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QDBusConnection connection =
            QDBusConnection::connectToBus(m_address, connectionName);
        auto *service = new FakeLogindBacklightService(connection);
        const bool registered = connection.isConnected() && service->registerService();
        m_owner->m_connectionName = connectionName;
        m_owner->m_service = registered ? service : nullptr;
        m_owner->m_ready = registered;
        m_owner->m_started.release();
        if (!registered) {
            delete service;
            QDBusConnection::disconnectFromBus(connectionName);
            return;
        }
        // The connection lives on this thread, so its socket notifier
        // dispatches here: a blocking call from the test thread is answered.
        exec();
        delete service;
        m_owner->m_service = nullptr;
    }
    QDBusConnection::disconnectFromBus(connectionName);
}

FakeLogindBacklightServiceThread::FakeLogindBacklightServiceThread(
    const QString &busAddress)
    : m_worker(busAddress, this)
{
    m_worker.start();
    m_started.acquire();
}

FakeLogindBacklightServiceThread::~FakeLogindBacklightServiceThread()
{
    m_worker.quit();
    if (!m_worker.wait(5000)) {
        m_worker.terminate();
        m_worker.wait();
    }
}

} // namespace QindaQt::Tests
