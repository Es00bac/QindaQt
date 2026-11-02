// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/portal_session.h>

#include "response_sink.h"

#include <QtCore/QRandomGenerator>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Link {
namespace {
constexpr auto kRemoteDesktop = "org.freedesktop.portal.RemoteDesktop";
constexpr auto kRequest = "org.freedesktop.portal.Request";
constexpr auto kSession = "org.freedesktop.portal.Session";
constexpr uint kKeyboardAndPointer = 3;
constexpr uint kPersistUntilRevoked = 2;

QString randomToken()
{
    return QStringLiteral("qindalink_%1").arg(QRandomGenerator::system()->generate64(), 16, 16, QLatin1Char('0'));
}
}

PortalInputSession::PortalInputSession(QDBusConnection bus, PortalOptions options, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_options(std::move(options)), m_tokens(m_options.tokenFile)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(Failure::Unavailable, QStringLiteral("portal step timed out")); });
}

PortalInputSession::~PortalInputSession() { resetSession(true); }

void PortalInputSession::ensure()
{
    if (m_state != State::Idle) return;
    m_state = State::Creating;
    ++m_generation;
    m_timeout.start(m_options.stepTimeoutMs);
    request(QStringLiteral("CreateSession"), {},
            {{QStringLiteral("session_handle_token"), randomToken()}},
            [this](uint r, const QVariantMap &res) { onCreated(r, res); });
}

void PortalInputSession::request(const QString &method, const QVariantList &args, QVariantMap options,
                                 std::function<void(uint, const QVariantMap &)> done)
{
    const QString token = randomToken();
    QString sender = m_bus.baseService();
    sender.remove(QLatin1Char(':')).replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString expected = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    options.insert(QStringLiteral("handle_token"), token);

    // AGENT-GUARD: subscribe before calling. The portal may answer
    // immediately, and a late subscription would wait for a reply that
    // already passed.
    auto *sink = new ResponseSink(this);
    m_sinks.emplace_back(sink);
    m_bus.connect(m_options.service, expected, QString::fromLatin1(kRequest), QStringLiteral("Response"), sink,
                  SLOT(Response(uint, QVariantMap)));
    const quint64 generation = m_generation;
    connect(sink, &ResponseSink::arrived, this, [this, generation, done = std::move(done)](uint r, const QVariantMap &res) {
        if (generation == m_generation) done(r, res);
    });

    QDBusMessage msg = QDBusMessage::createMethodCall(m_options.service, m_options.path,
                                                      QString::fromLatin1(kRemoteDesktop), method);
    QVariantList all = args;
    all << options;
    msg.setArguments(all);
    msg.setInteractiveAuthorizationAllowed(false);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, watcher, expected, sink] {
        watcher->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (generation != m_generation) return;
        if (reply.isError()) {
            fail(Failure::Unavailable, reply.error().name());
        } else if (reply.value().path() != expected) {
            // Older portals pick their own request path.
            m_bus.connect(m_options.service, reply.value().path(), QString::fromLatin1(kRequest),
                          QStringLiteral("Response"), sink, SLOT(Response(uint, QVariantMap)));
        }
    });
}

void PortalInputSession::onCreated(uint response, const QVariantMap &results)
{
    const QString handle = results.value(QStringLiteral("session_handle")).toString();
    if (response != 0 || handle.isEmpty()) {
        fail(Failure::Unavailable, QStringLiteral("CreateSession refused"));
        return;
    }
    m_session = handle;
    m_bus.connect(m_options.service, m_session, QString::fromLatin1(kSession), QStringLiteral("Closed"), this,
                  SLOT(onSessionClosed(QVariantMap)));
    m_state = State::Selecting;
    m_timeout.start(m_options.stepTimeoutMs);
    QVariantMap options{{QStringLiteral("types"), kKeyboardAndPointer},
                        {QStringLiteral("persist_mode"), kPersistUntilRevoked}};
    const QString token = m_tokens.load();
    if (!token.isEmpty()) options.insert(QStringLiteral("restore_token"), token);
    request(QStringLiteral("SelectDevices"), {QVariant::fromValue(QDBusObjectPath(m_session))}, options,
            [this](uint r, const QVariantMap &res) { onSelected(r, res); });
}

void PortalInputSession::onSelected(uint response, const QVariantMap &)
{
    if (response != 0) {
        fail(Failure::Unavailable, QStringLiteral("SelectDevices refused"));
        return;
    }
    m_state = State::Starting;
    m_timeout.start(m_options.stepTimeoutMs);
    request(QStringLiteral("Start"), {QVariant::fromValue(QDBusObjectPath(m_session)), QString()}, {},
            [this](uint r, const QVariantMap &res) { onStarted(r, res); });
}

void PortalInputSession::onStarted(uint response, const QVariantMap &results)
{
    if (response != 0) {
        m_tokens.clear();
        fail(Failure::Denied, QStringLiteral("not approved"));
        return;
    }
    if ((results.value(QStringLiteral("devices")).toUInt() & kKeyboardAndPointer) != kKeyboardAndPointer) {
        fail(Failure::Denied, QStringLiteral("devices not granted"));
        return;
    }
    // AGENT-GUARD: this is the only place the session becomes Active.
    const QString token = results.value(QStringLiteral("restore_token")).toString();
    if (!token.isEmpty()) m_tokens.save(token);
    m_timeout.stop();
    m_state = State::Active;
    Q_EMIT activated();
}

void PortalInputSession::onSessionClosed(const QVariantMap &)
{
    const bool wasActive = m_state == State::Active;
    resetSession(false);
    if (wasActive) {
        m_tokens.clear(); // revoked: ask again next time
        Q_EMIT revoked();
    } else {
        Q_EMIT failed(Failure::Denied, QStringLiteral("closed during setup"));
    }
}

void PortalInputSession::fail(Failure failure, const QString &reason)
{
    resetSession(true);
    Q_EMIT failed(failure, reason);
}

void PortalInputSession::close() { resetSession(true); }

void PortalInputSession::resetSession(bool sendClose)
{
    m_timeout.stop();
    ++m_generation; // abandons any handshake reply still in flight
    const QString session = std::exchange(m_session, QString());
    m_state = State::Idle;
    for (auto &sink : m_sinks) sink.release()->deleteLater(); // may be running right now
    m_sinks.clear();
    if (session.isEmpty()) return;
    m_bus.disconnect(m_options.service, session, QString::fromLatin1(kSession), QStringLiteral("Closed"), this,
                     SLOT(onSessionClosed(QVariantMap)));
    if (sendClose && m_bus.isConnected()) {
        m_bus.send(QDBusMessage::createMethodCall(m_options.service, session, QString::fromLatin1(kSession),
                                                  QStringLiteral("Close")));
    }
}

bool PortalInputSession::notify(const QString &method, const QVariantList &args)
{
    if (m_state != State::Active) return false;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_options.service, m_options.path,
                                                      QString::fromLatin1(kRemoteDesktop), method);
    QVariantList all{QVariant::fromValue(QDBusObjectPath(m_session)), QVariantMap{}};
    all += args;
    msg.setArguments(all);
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        const QDBusPendingReply<> reply = *watcher;
        if (!reply.isError() || generation != m_generation) return;
        // Only a vanished session is fatal; one bad argument must not end input.
        const QString name = reply.error().name();
        if (name == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown")
            || name == QLatin1String("org.freedesktop.portal.Error.NotFound")) {
            resetSession(false);
            m_tokens.clear();
            Q_EMIT revoked();
        }
    });
    return true;
}

bool PortalInputSession::pointerMotion(double dx, double dy)
{
    return notify(QStringLiteral("NotifyPointerMotion"), {dx, dy});
}

bool PortalInputSession::pointerButton(qint32 code, bool down)
{
    return notify(QStringLiteral("NotifyPointerButton"), {code, quint32(down ? 1 : 0)});
}

bool PortalInputSession::pointerAxis(double dx, double dy, bool finish)
{
    if (m_state != State::Active) return false;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_options.service, m_options.path,
                                                      QString::fromLatin1(kRemoteDesktop), QStringLiteral("NotifyPointerAxis"));
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_session)),
                      QVariantMap{{QStringLiteral("finish"), finish}}, dx, dy});
    m_bus.asyncCall(msg);
    return true;
}

bool PortalInputSession::pointerAxisDiscrete(quint32 axis, qint32 steps)
{
    return notify(QStringLiteral("NotifyPointerAxisDiscrete"), {axis, steps});
}

bool PortalInputSession::keysym(qint32 sym, bool down)
{
    return notify(QStringLiteral("NotifyKeyboardKeysym"), {sym, quint32(down ? 1 : 0)});
}

} // namespace QindaQt::Link
