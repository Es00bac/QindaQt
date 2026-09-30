// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/inhibit_adaptor.h>
namespace QindaQt::Services::Portal {
InhibitAdaptor::InhibitAdaptor(QObject &host, RequestRegistry &requests, IdleInhibition &idle, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_idle(idle), m_bus(bus) {
    connect(&idle, &IdleInhibition::acquired, this, [this](RequestToken token, bool success) {
        if (!m_tokens.contains(token)) return;
        if (success && m_requests.live(token)) m_requests.acknowledgeHeld(token);
        else m_requests.failVoid(token, QStringLiteral("org.freedesktop.portal.Error.NotAllowed"));
    });
    connect(&idle, &IdleInhibition::unavailable, this, [this] {
        const auto tokens = m_tokens.values();
        for (const auto token : tokens) m_requests.failVoid(token, QStringLiteral("org.freedesktop.portal.Error.NotAllowed"));
    });
}
InhibitAdaptor::~InhibitAdaptor() {
    const auto tokens = m_tokens.values();
    for (const auto token : tokens) m_requests.failVoid(token, QStringLiteral("org.freedesktop.portal.Error.Failed"));
}
void InhibitAdaptor::Inhibit(const QDBusObjectPath &handle, const QString &app, const QString &window,
    quint32 flags, const QVariantMap &options, const QDBusMessage &call) {
    const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) {
        m_tokens.remove(*slot); m_idle.cancel(*slot);
    }, 5000, true);
    *slot = token; if (!token) return;
    const auto reason = options.value(QStringLiteral("reason"));
    if (flags != 8 || options.size() > 32 || window.size() > 512
        || (reason.isValid() && reason.metaType() != QMetaType::fromType<QString>()) || reason.toString().size() > 512) {
        m_requests.failVoid(token, QStringLiteral("org.freedesktop.portal.Error.NotAllowed")); return;
    }
    m_tokens.insert(token); m_idle.acquire(token, app, reason.toString());
}
quint32 InhibitAdaptor::CreateMonitor(const QDBusObjectPath &, const QDBusObjectPath &,
    const QString &, const QString &, const QDBusMessage &call) {
    if (!m_requests.authenticated(call)) {
        call.setDelayedReply(true); m_bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                                                                   QStringLiteral("Frontend unavailable")));
    }
    // No native end-session monitor is wired. A failure is truthful; no fake
    // Running state/session token or screensaver payload is manufactured.
    return 2;
}
void InhibitAdaptor::QueryEndResponse(const QDBusObjectPath &, const QDBusMessage &call) {
    call.setDelayedReply(true); m_bus.send(call.createErrorReply(m_requests.authenticated(call)
        ? QStringLiteral("org.freedesktop.portal.Error.NotFound") : QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                                                               QStringLiteral("No monitoring session")));
}
} // namespace QindaQt::Services::Portal
