// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/screencast_adaptor.h>
#include "capture_sessions_p.h"
#include <QSet>
namespace QindaQt::Services::Portal {
class ScreenCastAdaptor::Private {
public:
    RequestRegistry &requests; CaptureUI &ui; CaptureSessions sessions;
    Private(ScreenCastAdaptor &adaptor, RequestRegistry &r, CaptureUI &port, QDBusConnection bus) : requests(r), ui(port), sessions(std::move(bus), r) {
        QObject::connect(&sessions, &CaptureSessions::retired, &adaptor, [&port](const QString &path) { port.stop(path); });
        QObject::connect(&ui, &CaptureUI::closed, &sessions, [this](const QString &path) { sessions.close(path); });
        QObject::connect(&ui, &CaptureUI::authorityLost, &sessions, [this] { sessions.clear(); });
        QObject::connect(&ui, &CaptureUI::completed, &adaptor, [this](RequestToken token, RequestResponse response, const QVariantMap &results) {
            const auto path = pending.take(token); if (path.isEmpty()) return;
            auto *entry = sessions.entry(path);
            if (!entry || !sessions.live(path) || !requests.live(token) || !ui.admitted()
                || (response == RequestResponse::Success && (!validCapturePublication(CaptureKind::Stream, results)
                    || (!entry->multiple && results.value("streams").value<CaptureStreams>().size() != 1)))) response = RequestResponse::Failed;
            if (entry) { entry->pending = 0; if (response == RequestResponse::Success) entry->phase = CaptureSessionPhase::Streaming; }
            requests.finish(token, response, results);
            if (response != RequestResponse::Success) sessions.close(path);
        });
    }
    QHash<RequestToken, QString> pending;
    QSet<RequestToken> owned;
    ScreenCastSourceDelegate *remote = nullptr;
    RequestToken begin(const QDBusMessage &call, const QString &handle, const QString &path, const QString &app) {
        const auto slot = std::make_shared<RequestToken>(0);
        const auto token = requests.begin(call, handle, app, [this, slot, path](RequestResponse response) {
            pending.remove(*slot); const bool owns = owned.remove(*slot); if (response != RequestResponse::Success) { ui.cancel(*slot); if (owns) sessions.close(path); }
        }, 100000);
        *slot = token; return token;
    }
};
ScreenCastAdaptor::ScreenCastAdaptor(QObject &host, RequestRegistry &requests, CaptureUI &ui, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), d(std::make_unique<Private>(*this, requests, ui, std::move(bus))) { registerCaptureWireTypes(); }
ScreenCastAdaptor::ScreenCastAdaptor(QObject &host, RequestRegistry &requests, CaptureUI &ui, QDBusConnection bus, ScreenCastSourceDelegate *remote)
    : ScreenCastAdaptor(host, requests, ui, std::move(bus)) { d->remote = remote; }
ScreenCastAdaptor::~ScreenCastAdaptor() { d->sessions.clear(); }
quint32 ScreenCastAdaptor::CreateSession(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto token = d->begin(call, handle.path(), session.path(), app); if (!token) return 2;
    if (!options.isEmpty() || !d->ui.admitted() || !d->sessions.create(call, handle.path(), session.path(), app)) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    d->owned.insert(token);
    d->requests.finish(token, RequestResponse::Success, {{"session_id", session.path().section('/', -1)}}); return 2;
}
quint32 ScreenCastAdaptor::SelectSources(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto token = d->begin(call, handle.path(), session.path(), app); if (!token) return 2;
    auto *entry = d->sessions.entry(session.path());
    if (!entry && d->remote && d->remote->ownsSession(session.path())) {
        // RemoteDesktop session: the frontend refuses persistence for it, so any
        // persist/restore option here is malformed. The owner checks the actor.
        const bool selected = d->ui.admitted() && validScreenCastSelection(options) && !options.contains("restore_data")
            && options.value("persist_mode", 0U).toUInt() == 0 && d->remote->selectSources(call, handle.path(), session.path(), app,
                options.value("multiple", false).toBool(), options.value("cursor_mode", 1U).toUInt());
        d->requests.finish(token, selected ? RequestResponse::Success : RequestResponse::Failed); return 2;
    }
    if (!entry || !d->sessions.authenticated(call, session.path(), app) || !d->sessions.requestMatches(session.path(), handle.path())) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    d->owned.insert(token);
    if (!d->ui.admitted() || entry->phase != CaptureSessionPhase::Created || !validScreenCastSelection(options)) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    entry->multiple = options.value("multiple", false).toBool(); entry->cursorMode = options.value("cursor_mode", 1U).toUInt();
    entry->phase = CaptureSessionPhase::Selected; d->requests.finish(token, RequestResponse::Success); return 2;
}
quint32 ScreenCastAdaptor::Start(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app, const QString &parent, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto token = d->begin(call, handle.path(), session.path(), app); if (!token) return 2;
    auto request = screenshotRequest(app, parent, {}, false); auto *entry = d->sessions.entry(session.path());
    if (!entry || !d->sessions.authenticated(call, session.path(), app) || !d->sessions.requestMatches(session.path(), handle.path())) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    d->owned.insert(token);
    if (!d->ui.admitted() || entry->phase != CaptureSessionPhase::Selected || !options.isEmpty() || !request) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    request->kind = CaptureKind::Stream; request->session = session.path(); request->caller = entry->caller;
    request->multiple = entry->multiple; request->cursorMode = entry->cursorMode;
    entry->phase = CaptureSessionPhase::Starting; entry->pending = token;
    d->pending.insert(token, session.path()); d->ui.request(token, *request); return 2;
}
}
