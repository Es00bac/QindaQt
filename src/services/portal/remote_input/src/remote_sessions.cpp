// SPDX-License-Identifier: LGPL-3.0-or-later
#include "remote_sessions_p.h"
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTimer>
#include <optional>
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace QindaQt::Services::Portal::RemoteInput {
namespace {
constexpr auto kFrontend = "org.freedesktop.portal.Desktop";
constexpr auto kSession = "org.freedesktop.impl.portal.Session";
class SessionObject final : public QDBusVirtualObject {
public:
    SessionObject(std::function<void(const QDBusMessage &)> close, QObject *parent)
        : QDBusVirtualObject(parent), m_close(std::move(close)) {}
    QString introspect(const QString &) const override {
        return QStringLiteral("<interface name='org.freedesktop.impl.portal.Session'><method name='Close'/>"
                              "<signal name='Closed'/><property name='version' type='u' access='read'/></interface>");
    }
    bool handleMessage(const QDBusMessage &call, const QDBusConnection &bus) override {
        if (call.interface() == QLatin1String(kSession) && call.member() == QLatin1String("Close")) { m_close(call); return true; }
        if (call.interface() != QLatin1String("org.freedesktop.DBus.Properties")) return false;
        const auto args = call.arguments();
        if (call.member() == QLatin1String("Get") && call.signature() == QLatin1String("ss")
            && args[0].toString() == QLatin1String(kSession) && args[1].toString() == QLatin1String("version"))
            bus.send(call.createReply(QVariantList{QVariant::fromValue(QDBusVariant(1U))}));
        else if (call.member() == QLatin1String("GetAll") && call.signature() == QLatin1String("s")
                 && args[0].toString() == QLatin1String(kSession))
            bus.send(call.createReply(QVariantList{QVariant::fromValue(QVariantMap{{QStringLiteral("version"), 1U}})}));
        else bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"), QStringLiteral("Property refused")));
        return true;
    }
private:
    std::function<void(const QDBusMessage &)> m_close;
};
// Standard handles encode the caller's unique name: /…/<kind>/1_23/<token>.
std::optional<QString> callerForPath(const QString &path, const QString &kind) {
    const QRegularExpression pattern(QStringLiteral("^/org/freedesktop/portal/desktop/%1/([0-9]+_[0-9]+)/[A-Za-z0-9_]+$").arg(kind));
    const auto match = pattern.match(path);
    if (path.size() > 512 || !match.hasMatch()) return {};
    return QLatin1Char(':') + match.captured(1).replace(QLatin1Char('_'), QLatin1Char('.'));
}
bool pidAlive(int fd) { pollfd event{fd, POLLIN, 0}; return fd >= 0 && poll(&event, 1, 0) == 0; }
}
RemoteSessions::RemoteSessions(QDBusConnection bus, RequestRegistry &requests, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_requests(requests),
      m_watcher(QLatin1String(kFrontend), m_bus, QDBusServiceWatcher::WatchForOwnerChange) {
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &RemoteSessions::sweep);
}
RemoteSessions::~RemoteSessions() { clear(); }
bool RemoteSessions::create(const QDBusMessage &call, const QString &request, const QString &path, const QString &app) {
    const auto caller = callerForPath(path, QStringLiteral("session"));
    const auto requestCaller = callerForPath(request, QStringLiteral("request"));
    if (!m_requests.authenticated(call) || !caller || !requestCaller || *caller != *requestCaller
        || m_entries.contains(path) || m_entries.size() >= 8) return false;
    auto *daemon = m_bus.interface();
    if (!daemon) return false;
    const QDBusReply<uint> uid = daemon->serviceUid(*caller), pid = daemon->servicePid(*caller);
    if (!uid.isValid() || uid.value() != static_cast<uint>(geteuid()) || !pid.isValid() || !pid.value()) return false;
    const int pidfd = static_cast<int>(syscall(SYS_pidfd_open, pid.value(), 0));
    if (!pidAlive(pidfd)) { if (pidfd >= 0) ::close(pidfd); return false; }
    auto *object = new SessionObject([this, path, app](const QDBusMessage &close) {
        if (!close.signature().isEmpty() || !authenticated(close, path, app)) {
            m_bus.send(close.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"), QStringLiteral("Close refused")));
            return;
        }
        m_bus.send(close.createReply());
        this->close(path, false);
    }, this);
    if (!m_bus.registerVirtualObject(path, object, QDBusConnection::SingleNode)) { delete object; ::close(pidfd); return false; }
    auto *exit = new QSocketNotifier(pidfd, QSocketNotifier::Read, this);
    connect(exit, &QSocketNotifier::activated, this, &RemoteSessions::sweep);
    m_watcher.addWatchedService(*caller);
    Entry entry;
    entry.frontend = call.service(); entry.caller = *caller; entry.app = app;
    entry.pidfd = pidfd; entry.object = object; entry.exit = exit;
    m_entries.insert(path, entry);
    // Only an unstarted session expires; a granted session ends on Close,
    // actor loss, native lock or compositor loss, never on wall-clock time.
    QTimer::singleShot(60000, this, [this, path] {
        const auto it = m_entries.constFind(path);
        if (it != m_entries.cend() && (it->phase == RemotePhase::Created || it->phase == RemotePhase::Selected)) close(path);
    });
    return live(path);
}
bool RemoteSessions::live(const QString &path) const {
    const auto it = m_entries.constFind(path);
    if (it == m_entries.cend() || !pidAlive(it->pidfd) || it->frontend != m_requests.frontendOwner()) return false;
    auto *daemon = m_bus.interface();
    if (!daemon) return false;
    const QDBusReply<QString> owner = daemon->serviceOwner(it->caller);
    return owner.isValid() && owner.value() == it->caller;
}
bool RemoteSessions::authenticated(const QDBusMessage &call, const QString &path, const QString &app) const {
    const auto it = m_entries.constFind(path);
    return it != m_entries.cend() && it->app == app && it->frontend == call.service()
        && m_requests.authenticated(call) && live(path);
}
bool RemoteSessions::owned(const QDBusMessage &call, const QString &path) const {
    const auto it = m_entries.constFind(path);
    return it != m_entries.cend() && it->frontend == call.service() && m_requests.authenticated(call) && live(path);
}
bool RemoteSessions::requestMatches(const QString &path, const QString &request) const {
    const auto it = m_entries.constFind(path);
    const auto caller = callerForPath(request, QStringLiteral("request"));
    return it != m_entries.cend() && caller && *caller == it->caller;
}
RemoteSessions::Entry *RemoteSessions::entry(const QString &path) {
    const auto it = m_entries.find(path);
    return it == m_entries.end() ? nullptr : &it.value();
}
const RemoteSessions::Entry *RemoteSessions::find(const QString &path) const {
    const auto it = m_entries.constFind(path);
    return it == m_entries.cend() ? nullptr : &it.value();
}
QStringList RemoteSessions::paths() const { return m_entries.keys(); }
QString RemoteSessions::sessionForTicket(quint64 ticket) const {
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it)
        if (ticket && it->eisTicket == ticket) return it.key();
    return {};
}
void RemoteSessions::close(const QString &path, bool notify) {
    const auto it = m_entries.find(path);
    if (it == m_entries.end()) return;
    const Entry entry = it.value();
    m_entries.erase(it);
    m_bus.unregisterObject(path);
    entry.object->deleteLater();
    entry.exit->setEnabled(false); entry.exit->deleteLater();
    ::close(entry.pidfd);
    bool callerShared = false;
    for (const auto &other : std::as_const(m_entries)) callerShared |= other.caller == entry.caller;
    if (!callerShared) m_watcher.removeWatchedService(entry.caller);
    if (notify) m_bus.send(QDBusMessage::createTargetedSignal(entry.frontend, path, QLatin1String(kSession), QStringLiteral("Closed")));
    if (entry.pending) m_requests.retire(entry.pending, RequestResponse::Failed);
    Q_EMIT retired(path, entry);
}
void RemoteSessions::clear() {
    const auto paths = m_entries.keys();
    for (const auto &path : paths) close(path);
}
void RemoteSessions::sweep() {
    const auto paths = m_entries.keys();
    for (const auto &path : paths) if (!live(path)) close(path);
}
} // namespace QindaQt::Services::Portal::RemoteInput
