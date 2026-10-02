// SPDX-License-Identifier: LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2018 Red Hat Inc
// SPDX-FileCopyrightText: 2018 Jan Grulich <jgrulich@redhat.com>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
// Session/device/EIS flow adapted from xdg-desktop-portal-kde 6.6.6
// src/remotedesktop.cpp (9a5cc0e8). KWin process lookup, KNotification, the
// tray item, fake-input and restore tokens are replaced by native ports.
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include "remote_sessions_p.h"
#include <QSet>
#include <optional>

namespace QindaQt::Services::Portal::RemoteInput {
namespace {
constexpr auto kDenied = "org.freedesktop.DBus.Error.AccessDenied";
constexpr auto kNotAllowed = "org.freedesktop.portal.Error.NotAllowed";
constexpr auto kFailed = "org.freedesktop.portal.Error.Failed";
constexpr auto kNotSupported = "org.freedesktop.DBus.Error.NotSupported";
template<typename T> bool typed(const QVariantMap &options, const char *key) {
    const auto it = options.constFind(QLatin1String(key));
    return it == options.cend() || it->metaType() == QMetaType::fromType<T>();
}
// Unknown keys are ignored for standard forward compatibility. Persistence is
// not implemented: restore data is never trusted and every Start asks again.
std::optional<quint32> selectedDevices(const QVariantMap &options) {
    if (!typed<uint>(options, "types") || !typed<uint>(options, "persist_mode")) return std::nullopt;
    const quint32 types = options.value(QStringLiteral("types"), kAllDeviceTypes).toUInt();
    if (!types || (types & ~kAllDeviceTypes) || options.value(QStringLiteral("persist_mode"), 0U).toUInt() > 2)
        return std::nullopt;
    return types;
}
std::optional<AccessQuestion> consentQuestion(const QString &app, const QString &parent, quint32 devices) {
    QStringList names;
    if (devices & Keyboard) names << QStringLiteral("keyboard");
    if (devices & Pointer) names << QStringLiteral("pointer");
    if (devices & Touchscreen) names << QStringLiteral("touchscreen");
    return accessQuestion(app, parent, QStringLiteral("Allow remote control?"),
        QStringLiteral("Requested input devices: %1").arg(names.join(QStringLiteral(", "))),
        QStringLiteral("Input will reach every window on this desktop until the session ends, "
                       "the application closes it or the screen locks."), {});
}
}
class RemoteDesktopAdaptor::Private {
public:
    RequestRegistry &requests;
    AccessConsent &consent;
    CompositorEis &eis;
    QDBusConnection bus;
    RemoteSessions sessions;
    QHash<RequestToken, QString> asking;
    QSet<RequestToken> starting;
    Private(RequestRegistry &r, AccessConsent &c, CompositorEis &e, QDBusConnection connection)
        : requests(r), consent(c), eis(e), bus(connection), sessions(std::move(connection), r) {}
    RequestToken begin(const QDBusMessage &call, const QString &handle, const QString &path, const QString &app) {
        const auto slot = std::make_shared<RequestToken>(0);
        const auto token = requests.begin(call, handle, app, [this, slot, path](RequestResponse response) {
            asking.remove(*slot);
            if (starting.remove(*slot) && response != RequestResponse::Success) {
                consent.cancel(*slot);
                sessions.close(path);
            }
        }, 100000);
        *slot = token;
        return token;
    }
    void refuse(const QDBusMessage &call, const char *error) {
        call.setDelayedReply(true);
        bus.send(call.createErrorReply(QLatin1String(error), QStringLiteral("Remote input refused")));
    }
    void consentCompleted(RequestToken token, RequestResponse response) {
        const QString path = asking.take(token);
        if (path.isEmpty()) return;
        auto *entry = sessions.entry(path);
        if (!entry || !sessions.live(path) || !requests.live(token) || !consent.admitted()) response = RequestResponse::Failed;
        if (entry) entry->pending = 0;
        if (response != RequestResponse::Success) { requests.finish(token, response); return; }
        entry->phase = RemotePhase::Started;
        // Clipboard sharing has no native data path yet; it is never granted.
        requests.finish(token, RequestResponse::Success, {{QStringLiteral("devices"), entry->devices},
                                                          {QStringLiteral("clipboard_enabled"), false}});
    }
    void eisOpened(quint64 ticket, const QDBusUnixFileDescriptor &fd, const QString &compositor, int cookie) {
        const QString path = sessions.sessionForTicket(ticket);
        auto *entry = sessions.entry(path);
        if (!entry) { eis.close(compositor, cookie); return; }
        const QDBusMessage call = std::exchange(entry->eisCall, {});
        entry->eisTicket = 0;
        // AGENT-GUARD: publication rechecks actor, phase and native authority;
        // a retired session's late transport is disconnected, never delivered.
        if (!sessions.live(path) || entry->phase != RemotePhase::Started || !consent.admitted()
            || !requests.authenticated(call)) {
            eis.close(compositor, cookie);
            refuse(call, kFailed);
            return;
        }
        entry->compositor = compositor;
        entry->cookie = cookie;
        bus.send(call.createReply(QVariant::fromValue(fd)));
    }
    void eisFailed(quint64 ticket) {
        auto *entry = sessions.entry(sessions.sessionForTicket(ticket));
        if (!entry) return;
        entry->eisTicket = 0;
        refuse(std::exchange(entry->eisCall, {}), kFailed);
    }
    void retired(const RemoteSessions::Entry &entry) {
        if (entry.eisTicket) { eis.cancel(entry.eisTicket); refuse(entry.eisCall, kFailed); }
        if (entry.cookie) eis.close(entry.compositor, entry.cookie);
    }
};
RemoteDesktopAdaptor::RemoteDesktopAdaptor(QObject &host, RequestRegistry &requests, AccessConsent &consent,
                                           CompositorEis &eis, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), d(std::make_unique<Private>(requests, consent, eis, std::move(bus))) {
    registerAccessTypes();
    connect(&consent, &AccessConsent::completed, this,
            [this](RequestToken token, RequestResponse response, const ChoiceValues &) { d->consentCompleted(token, response); });
    // Native lock, lock uncertainty or selected-session loss ends every session.
    connect(&consent, &AccessConsent::authorityLost, this, [this] { d->sessions.clear(); });
    connect(&eis, &CompositorEis::opened, this,
            [this](quint64 ticket, const QDBusUnixFileDescriptor &fd, const QString &compositor, int cookie) {
                d->eisOpened(ticket, fd, compositor, cookie);
            });
    connect(&eis, &CompositorEis::failed, this, [this](quint64 ticket) { d->eisFailed(ticket); });
    connect(&d->sessions, &RemoteSessions::retired, this,
            [this](const QString &, const RemoteSessions::Entry &entry) { d->retired(entry); });
}
RemoteDesktopAdaptor::~RemoteDesktopAdaptor() { d->sessions.clear(); }
quint32 RemoteDesktopAdaptor::CreateSession(const QDBusObjectPath &handle, const QDBusObjectPath &session,
    const QString &app, const QVariantMap &, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto token = d->begin(call, handle.path(), session.path(), app);
    if (!token) return 2;
    if (!d->consent.admitted() || !d->sessions.create(call, handle.path(), session.path(), app)) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    d->requests.finish(token, RequestResponse::Success, {{QStringLiteral("session_id"), session.path().section(QLatin1Char('/'), -1)}});
    return 2; // Real reply is delayed; this return value is never publication.
}
quint32 RemoteDesktopAdaptor::SelectDevices(const QDBusObjectPath &handle, const QDBusObjectPath &session,
    const QString &app, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto token = d->begin(call, handle.path(), session.path(), app);
    if (!token) return 2;
    auto *entry = d->sessions.entry(session.path());
    const auto devices = selectedDevices(options);
    if (!entry || !d->sessions.authenticated(call, session.path(), app) || !d->sessions.requestMatches(session.path(), handle.path())
        || !d->consent.admitted() || !devices || (entry->phase != RemotePhase::Created && entry->phase != RemotePhase::Selected)) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    entry->devices = *devices;
    entry->phase = RemotePhase::Selected;
    d->requests.finish(token, RequestResponse::Success);
    return 2;
}
quint32 RemoteDesktopAdaptor::Start(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app,
    const QString &parent, const QVariantMap &, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto token = d->begin(call, handle.path(), session.path(), app);
    if (!token) return 2;
    auto *entry = d->sessions.entry(session.path());
    if (!entry || !d->sessions.authenticated(call, session.path(), app) || !d->sessions.requestMatches(session.path(), handle.path())) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    d->starting.insert(token);
    const auto question = consentQuestion(app, parent, entry->devices);
    if (!d->consent.admitted() || entry->phase != RemotePhase::Selected || !question) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    entry->phase = RemotePhase::Starting;
    entry->pending = token;
    d->asking.insert(token, session.path());
    d->consent.ask(token, *question);
    return 2;
}
QDBusUnixFileDescriptor RemoteDesktopAdaptor::ConnectToEIS(const QDBusObjectPath &session, const QString &app,
                                                           const QVariantMap &, const QDBusMessage &call) {
    auto *entry = d->sessions.entry(session.path());
    if (!entry || !d->sessions.authenticated(call, session.path(), app)) { d->refuse(call, kDenied); return {}; }
    if (entry->phase != RemotePhase::Started || entry->eisTicket || entry->cookie || !d->consent.admitted()) {
        d->refuse(call, kNotAllowed);
        return {};
    }
    const quint64 ticket = d->eis.open(entry->devices);
    if (!ticket) { d->refuse(call, kFailed); return {}; }
    call.setDelayedReply(true);
    entry->eisTicket = ticket;
    entry->eisCall = call;
    return {};
}
// AGENT-NOTE: upstream KDE emulates these through KWin fake-input. QindaQt
// routes input only through the consented compositor EIS transport, so the
// legacy calls fail explicitly instead of silently dropping events.
void RemoteDesktopAdaptor::NotifyPointerMotion(const QDBusObjectPath &, const QVariantMap &, double, double, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyPointerMotionAbsolute(const QDBusObjectPath &, const QVariantMap &, uint, double, double, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyPointerButton(const QDBusObjectPath &, const QVariantMap &, int, uint, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyPointerAxis(const QDBusObjectPath &, const QVariantMap &, double, double, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyPointerAxisDiscrete(const QDBusObjectPath &, const QVariantMap &, uint, int, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyKeyboardKeycode(const QDBusObjectPath &, const QVariantMap &, int, uint, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyKeyboardKeysym(const QDBusObjectPath &, const QVariantMap &, int, uint, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyTouchDown(const QDBusObjectPath &, const QVariantMap &, uint, uint, double, double, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyTouchMotion(const QDBusObjectPath &, const QVariantMap &, uint, uint, double, double, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
void RemoteDesktopAdaptor::NotifyTouchUp(const QDBusObjectPath &, const QVariantMap &, uint, const QDBusMessage &call) { d->refuse(call, kNotSupported); }
} // namespace QindaQt::Services::Portal::RemoteInput
