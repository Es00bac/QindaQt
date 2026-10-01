// SPDX-License-Identifier: LGPL-3.0-or-later
#include "capture_sessions_p.h"
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QRegularExpression>
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
namespace {
class SessionObject final : public QDBusVirtualObject {
public:
    SessionObject(std::function<void(const QDBusMessage &)> close, QObject *parent) : QDBusVirtualObject(parent), m_close(std::move(close)) {}
    QString introspect(const QString &) const override { return QStringLiteral("<interface name='org.freedesktop.impl.portal.Session'><method name='Close'/><signal name='Closed'/><property name='version' type='u' access='read'/></interface>"); }
    bool handleMessage(const QDBusMessage &call, const QDBusConnection &bus) override {
        if (call.interface() == "org.freedesktop.impl.portal.Session" && call.member() == "Close") { m_close(call); return true; }
        if (call.interface() != "org.freedesktop.DBus.Properties") return false;
        const auto args = call.arguments();
        if (call.member() == "Get" && call.signature() == "ss" && args[0].toString() == "org.freedesktop.impl.portal.Session" && args[1].toString() == "version") bus.send(call.createReply({QVariant::fromValue(QDBusVariant(1U))}));
        else if (call.member() == "GetAll" && call.signature() == "s" && args[0].toString() == "org.freedesktop.impl.portal.Session") bus.send(call.createReply({QVariantMap{{"version", 1U}}}));
        else bus.send(call.createErrorReply("org.freedesktop.DBus.Error.InvalidArgs", "Property refused"));
        return true;
    }
private:
    std::function<void(const QDBusMessage &)> m_close;
};
std::optional<QString> callerForPath(const QString &path, const QString &kind) {
    const QRegularExpression pattern("^/org/freedesktop/portal/desktop/" + kind + "/([0-9]+_[0-9]+)/[A-Za-z0-9_]+$");
    const auto match = pattern.match(path);
    if (path.size() > 512 || !match.hasMatch()) return {};
    return ':' + match.captured(1).replace('_', '.');
}
bool pidAlive(int fd) { pollfd event{fd, POLLIN, 0}; return fd >= 0 && poll(&event, 1, 0) == 0; }
}
CaptureSessions::CaptureSessions(QDBusConnection bus, RequestRegistry &requests, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_requests(requests) {
    m_lifetime.setInterval(50);
    connect(&m_lifetime, &QTimer::timeout, this, [this] { const auto paths = m_entries.keys(); for (const auto &path : paths) if (!live(path)) close(path); });
    m_lifetime.start();
}
CaptureSessions::~CaptureSessions() { clear(); }
bool CaptureSessions::create(const QDBusMessage &call, const QString &requestPath, const QString &path, const QString &app) {
    const auto caller = callerForPath(path, "session"); const auto requestCaller = callerForPath(requestPath, "request");
    if (!m_requests.authenticated(call) || !caller || !requestCaller || *caller != *requestCaller || m_entries.contains(path) || m_entries.size() >= 8) return false;
    auto *daemon = m_bus.interface(); if (!daemon) return false;
    const QDBusReply<uint> uid = daemon->serviceUid(*caller), pid = daemon->servicePid(*caller);
    if (!uid.isValid() || uid.value() != static_cast<uint>(geteuid()) || !pid.isValid() || !pid.value()) return false;
    const int pidfd = static_cast<int>(syscall(SYS_pidfd_open, pid.value(), 0));
    if (!pidAlive(pidfd)) { if (pidfd >= 0) ::close(pidfd); return false; }
    auto *object = new SessionObject([this, path, app](const QDBusMessage &closeCall) {
        if (!closeCall.signature().isEmpty() || !authenticated(closeCall, path, app)) { m_bus.send(closeCall.createErrorReply("org.freedesktop.DBus.Error.AccessDenied", "Close refused")); return; }
        m_bus.send(closeCall.createReply()); close(path, false);
    }, this);
    if (!m_bus.registerVirtualObject(path, object, QDBusConnection::SingleNode)) { delete object; ::close(pidfd); return false; }
    m_entries.insert(path, {call.service(), *caller, app, pidfd, CaptureSessionPhase::Created, 0, object});
    // Only pre-start inactivity expires. A live share is ended by explicit stop
    // or actual actor/dependency lifetime, never by arbitrary wall-clock success.
    QTimer::singleShot(60000, this, [this, path] { const auto e = m_entries.constFind(path); if (e != m_entries.cend() && e->phase != CaptureSessionPhase::Streaming) close(path); });
    return live(path);
}
bool CaptureSessions::live(const QString &path) const {
    const auto e = m_entries.constFind(path); if (e == m_entries.cend() || !pidAlive(e->pidfd) || e->frontend != m_requests.frontendOwner()) return false;
    auto *daemon = m_bus.interface(); if (!daemon) return false;
    const QDBusReply<QString> callerOwner = daemon->serviceOwner(e->caller);
    return callerOwner.isValid() && callerOwner.value() == e->caller;
}
bool CaptureSessions::authenticated(const QDBusMessage &call, const QString &path, const QString &app) const {
    const auto e = m_entries.constFind(path);
    return e != m_entries.cend() && e->app == app && e->frontend == call.service() && m_requests.authenticated(call) && live(path);
}
bool CaptureSessions::requestMatches(const QString &path, const QString &request) const {
    const auto e = m_entries.constFind(path); const auto caller = callerForPath(request, "request");
    return e != m_entries.cend() && caller && *caller == e->caller;
}
CaptureSessions::Entry *CaptureSessions::entry(const QString &path) { const auto e = m_entries.find(path); return e == m_entries.end() ? nullptr : &e.value(); }
void CaptureSessions::close(const QString &path, bool notify) {
    const auto it = m_entries.find(path); if (it == m_entries.end()) return;
    const auto e = it.value(); m_entries.erase(it); m_bus.unregisterObject(path); e.object->deleteLater(); ::close(e.pidfd);
    if (notify) { auto signal = QDBusMessage::createTargetedSignal(e.frontend, path, "org.freedesktop.impl.portal.Session", "Closed"); m_bus.send(signal); }
    if (e.pending) m_requests.retire(e.pending);
    Q_EMIT retired(path);
}
void CaptureSessions::clear() { const auto paths = m_entries.keys(); for (const auto &path : paths) close(path); }
}
