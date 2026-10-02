// SPDX-License-Identifier: LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2018 Red Hat Inc
// SPDX-FileCopyrightText: 2018 Jan Grulich <jgrulich@redhat.com>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
// Session/device/EIS flow adapted from xdg-desktop-portal-kde 6.6.6
// src/remotedesktop.cpp (9a5cc0e8). KWin process lookup, KNotification, the
// tray item, fake-input and restore tokens are replaced by native ports; the
// Clipboard adjunct is the private ClipboardAdaptor on the same host. Combined
// screen sharing follows upstream's shared ScreenCast/RemoteDesktop session
// (screencast.cpp SelectSources, remotedesktop.cpp continueStart) over the
// existing protected capture producer instead of KWin's screencast client.
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include "clipboard_adaptor_p.h"
#include "remote_sessions_p.h"
#include <qindaqt/services/portal/capture_types.h>
#include <QPointer>
#include <QSet>
#include <algorithm>
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
constexpr auto kClipboardChoice = "clipboard";
std::optional<AccessQuestion> consentQuestion(const QString &app, const QString &parent, quint32 devices, bool clipboard,
                                             bool screens) {
    QStringList names;
    if (devices & Keyboard) names << QStringLiteral("keyboard");
    if (devices & Pointer) names << QStringLiteral("pointer");
    if (devices & Touchscreen) names << QStringLiteral("touchscreen");
    auto question = accessQuestion(app, parent, QStringLiteral("Allow remote control?"),
        QStringLiteral("Requested input devices: %1").arg(names.join(QStringLiteral(", ")))
            + (screens ? QStringLiteral(". Screens to share are chosen next.") : QString{}),
        QStringLiteral("Input will reach every window on this desktop until the session ends, "
                       "the application closes it or the screen locks."), {});
    // Clipboard sharing is a separate explicit opt-in, off unless chosen.
    if (question && clipboard)
        question->choices.append({QLatin1String(kClipboardChoice), QStringLiteral("Also share the clipboard"), {}, QStringLiteral("false")});
    return question;
}
bool clipboardChosen(const ChoiceValues &choices) {
    return std::any_of(choices.cbegin(), choices.cend(), [](const ChoiceValue &choice) {
        return choice.id == QLatin1String(kClipboardChoice) && choice.value == QLatin1String("true");
    });
}
}
class RemoteDesktopAdaptor::Private final : public ScreenCastSourceDelegate {
public:
    RequestRegistry &requests;
    AccessConsent &consent;
    CompositorEis &eis;
    CaptureUI *capture = nullptr;
    QDBusConnection bus;
    RemoteSessions sessions;
    QHash<RequestToken, QString> asking, capturing;
    QSet<RequestToken> starting;
    std::unique_ptr<ClipboardAdaptor> clipboard;
    Private(RequestRegistry &r, AccessConsent &c, CompositorEis &e, CaptureUI *port, QDBusConnection connection)
        : requests(r), consent(c), eis(e), capture(port), bus(connection), sessions(std::move(connection), r) {}
    bool ownsSession(const QString &path) const override { return sessions.find(path) != nullptr; }
    bool selectSources(const QDBusMessage &call, const QString &request, const QString &path, const QString &app,
                       bool multiple, quint32 cursorMode) override {
        auto *entry = sessions.entry(path);
        if (!capture || !entry || entry->screenCast || !sessions.authenticated(call, path, app)
            || !sessions.requestMatches(path, request) || !consent.admitted() || !capture->admitted()
            || (entry->phase != RemotePhase::Created && entry->phase != RemotePhase::Selected)) return false;
        entry->screenCast = true; entry->multiple = multiple; entry->cursorMode = cursorMode;
        return true;
    }
    RequestToken begin(const QDBusMessage &call, const QString &handle, const QString &path, const QString &app) {
        const auto slot = std::make_shared<RequestToken>(0);
        const auto token = requests.begin(call, handle, app, [this, slot, path](RequestResponse response) {
            asking.remove(*slot);
            capturing.remove(*slot);
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
    void consentCompleted(RequestToken token, RequestResponse response, const ChoiceValues &choices, QObject *guardObject) {
        const QString path = asking.take(token);
        if (path.isEmpty()) return;
        auto *entry = sessions.entry(path);
        if (!entry || !sessions.live(path) || !requests.live(token) || !consent.admitted()) response = RequestResponse::Failed;
        if (response != RequestResponse::Success) { requests.finish(token, response); return; }
        if (!entry->clipboardRequested || !clipboardChosen(choices)) { publishStart(token, path, false); return; }
        // The request stays pending (and retirable) until the compositor
        // clipboard handle exists, so clipboard_enabled is never claimed early.
        const QPointer<QObject> guard(guardObject);
        clipboard->enable(path, [this, guard, token, path](bool enabled) { if (guard) publishStart(token, path, enabled); });
    }
    void publishStart(RequestToken token, const QString &path, bool clipboardEnabled) {
        auto *entry = sessions.entry(path);
        if (!entry || !sessions.live(path) || !requests.live(token) || !consent.admitted()) {
            requests.finish(token, RequestResponse::Failed);
            return;
        }
        entry->clipboardEnabled = clipboardEnabled;
        if (entry->screenCast) {
            // AGENT-GUARD: streams come only from the protected capture producer
            // after its own monitor choice and consent; Start stays pending (and
            // retirable) until every selected stream is ready, then publishes once.
            auto request = screenshotRequest(entry->app, entry->parent, {}, false);
            if (!capture || !capture->admitted() || !request) { requests.finish(token, RequestResponse::Failed); return; }
            request->kind = CaptureKind::Stream; request->session = path; request->caller = entry->caller;
            request->multiple = entry->multiple; request->cursorMode = entry->cursorMode;
            capturing.insert(token, path);
            capture->request(token, *request);
            return;
        }
        complete(token, *entry, {});
    }
    void complete(RequestToken token, RemoteSessions::Entry &entry, QVariantMap results) {
        entry.pending = 0;
        entry.phase = RemotePhase::Started;
        results.insert(QStringLiteral("devices"), entry.devices);
        results.insert(QStringLiteral("clipboard_enabled"), entry.clipboardEnabled);
        requests.finish(token, RequestResponse::Success, results);
    }
    void captureCompleted(RequestToken token, RequestResponse response, const QVariantMap &results) {
        const QString path = capturing.take(token);
        if (path.isEmpty()) return;
        auto *entry = sessions.entry(path);
        if (!entry || !sessions.live(path) || !requests.live(token) || !consent.admitted() || !capture->admitted()
            || (response == RequestResponse::Success && (!validCapturePublication(CaptureKind::Stream, results)
                || (!entry->multiple && results.value(QStringLiteral("streams")).value<CaptureStreams>().size() != 1))))
            response = RequestResponse::Failed;
        // A refused or failed share retires the request, which closes the whole
        // session through begin(): input is never granted without its streams.
        if (response != RequestResponse::Success) { requests.finish(token, response); return; }
        complete(token, *entry, results);
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
    void retired(const QString &path, const RemoteSessions::Entry &entry) {
        if (entry.eisTicket) { eis.cancel(entry.eisTicket); refuse(entry.eisCall, kFailed); }
        if (entry.cookie) eis.close(entry.compositor, entry.cookie);
        clipboard->retired(entry);
        // Closing for any reason (Close, actor/frontend loss, lock, compositor
        // loss, failed Start) stops the session's pending or live streams.
        if (capture && entry.screenCast) capture->stop(path);
    }
};
RemoteDesktopAdaptor::RemoteDesktopAdaptor(QObject &host, RequestRegistry &requests, AccessConsent &consent,
                                           CompositorEis &eis, QDBusConnection bus)
    : RemoteDesktopAdaptor(host, requests, consent, eis, nullptr, std::move(bus)) {}
RemoteDesktopAdaptor::RemoteDesktopAdaptor(QObject &host, RequestRegistry &requests, AccessConsent &consent,
                                           CompositorEis &eis, CaptureUI &capture, QDBusConnection bus)
    : RemoteDesktopAdaptor(host, requests, consent, eis, &capture, std::move(bus)) {}
RemoteDesktopAdaptor::RemoteDesktopAdaptor(QObject &host, RequestRegistry &requests, AccessConsent &consent,
                                           CompositorEis &eis, CaptureUI *capture, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), d(std::make_unique<Private>(requests, consent, eis, capture, std::move(bus))) {
    registerAccessTypes();
    if (capture) {
        registerCaptureWireTypes();
        connect(capture, &CaptureUI::completed, this,
                [this](RequestToken token, RequestResponse response, const QVariantMap &results) {
                    d->captureCompleted(token, response, results);
                });
        // Producer stop/failure ends the shared session; compositor or authority
        // loss ends every session (input included).
        connect(capture, &CaptureUI::closed, this, [this](const QString &path) {
            if (d->sessions.find(path)) d->sessions.close(path);
        });
        connect(capture, &CaptureUI::authorityLost, this, [this] { d->sessions.clear(); });
    }
    d->clipboard = std::make_unique<ClipboardAdaptor>(host, d->sessions, consent, eis, d->bus);
    connect(&consent, &AccessConsent::completed, this,
            [this](RequestToken token, RequestResponse response, const ChoiceValues &choices) {
                d->consentCompleted(token, response, choices, this);
            });
    // Native lock, lock uncertainty or selected-session loss ends every session.
    connect(&consent, &AccessConsent::authorityLost, this, [this] { d->sessions.clear(); });
    connect(&eis, &CompositorEis::opened, this,
            [this](quint64 ticket, const QDBusUnixFileDescriptor &fd, const QString &compositor, int cookie) {
                d->eisOpened(ticket, fd, compositor, cookie);
            });
    connect(&eis, &CompositorEis::failed, this, [this](quint64 ticket) { d->eisFailed(ticket); });
    connect(&d->sessions, &RemoteSessions::retired, this,
            [this](const QString &path, const RemoteSessions::Entry &entry) { d->retired(path, entry); });
}
RemoteDesktopAdaptor::~RemoteDesktopAdaptor() { d->sessions.clear(); }
ScreenCastSourceDelegate &RemoteDesktopAdaptor::screenCastSources() { return *d; }
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
    const auto question = consentQuestion(app, parent, entry->devices, entry->clipboardRequested, entry->screenCast);
    if (!d->consent.admitted() || entry->phase != RemotePhase::Selected || !question) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    entry->phase = RemotePhase::Starting;
    entry->pending = token;
    entry->parent = parent;
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
