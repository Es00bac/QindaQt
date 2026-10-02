// SPDX-License-Identifier: LGPL-3.0-or-later
#include "global_shortcuts_adaptor.h"
#include "shortcut_sessions_p.h"
#include <QCryptographicHash>
#include <QSet>
#include <algorithm>
namespace QindaQt::Services::Portal {
namespace {
QString componentFor(const QString &path) { return QStringLiteral("qindaqt.portal.") + QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex()); }
QVariantMap publication(const ShortcutDrafts &drafts) { return {{"shortcuts", QVariant::fromValue(shortcutDescriptions(drafts))}}; }
}
class GlobalShortcutsAdaptor::Private {
public:
    struct Pending { QString path; ShortcutDrafts offered; };
    RequestRegistry &requests; ShortcutUi &ui; QindaQt::Services::Shortcuts::QtShortcutTransport &native; ShortcutSessions sessions;
    QHash<QString, ShortcutDrafts> bindings; QHash<RequestToken, Pending> pending;
    Private(GlobalShortcutsAdaptor &adaptor, RequestRegistry &r, ShortcutUi &u, QindaQt::Services::Shortcuts::QtShortcutTransport &n, QDBusConnection bus)
        : requests(r), ui(u), native(n), sessions(std::move(bus), r) {
        QObject::connect(&sessions, &ShortcutSessions::retired, &adaptor, [this](const QString &path) {
            const auto old = bindings.take(path); const auto component = componentFor(path);
            for (const auto &binding : old) native.unregisterBinding(component, binding.id);
        });
        QObject::connect(&ui, &ShortcutUi::authorityLost, &adaptor, [this] { sessions.clear(); });
        QObject::connect(&native, &QindaQt::Services::Shortcuts::QtShortcutTransport::authorityLost, &adaptor, [this] { sessions.clear(); });
        const auto activated = [this, &adaptor](bool press, const QString &component, const QString &action, quint64 timestamp) {
            for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
                if (componentFor(it.key()) != component || !sessions.live(it.key())) continue;
                const bool bound = std::any_of(it->cbegin(), it->cend(), [&action](const ShortcutDraft &draft) { return draft.id == action; });
                if (!bound) continue;
                if (!press || ui.admitted()) {
                    if (press) Q_EMIT adaptor.Activated(QDBusObjectPath(it.key()), action, timestamp, {});
                    else Q_EMIT adaptor.Deactivated(QDBusObjectPath(it.key()), action, timestamp, {});
                    return;
                }
            }
        };
        QObject::connect(&native, &QindaQt::Services::Shortcuts::QtShortcutTransport::activated, &adaptor, [activated](const QString &c, const QString &a, quint64 t) { activated(true, c, a, t); });
        QObject::connect(&native, &QindaQt::Services::Shortcuts::QtShortcutTransport::deactivated, &adaptor, [activated](const QString &c, const QString &a, quint64 t) { activated(false, c, a, t); });
        QObject::connect(&ui, &ShortcutUi::completed, &adaptor, [this, &adaptor](RequestToken token, RequestResponse response, const QJsonObject &results) {
            const auto found = pending.find(token); if (found == pending.end()) return;
            const auto request = found.value(); pending.erase(found);
            auto *entry = sessions.entry(request.path);
            if (!entry || !sessions.live(request.path) || !requests.live(token) || !ui.admitted() || !native.available()) response = RequestResponse::Failed;
            const auto selected = response == RequestResponse::Success ? shortcutSelection(request.offered, results) : std::nullopt;
            if (response == RequestResponse::Success && !selected) response = RequestResponse::Failed;
            if (response == RequestResponse::Success && !apply(request.path, *selected)) response = RequestResponse::Failed;
            entry = sessions.entry(request.path);
            if (entry) { entry->pending = 0; if (response == RequestResponse::Success) entry->phase = ShortcutSessionPhase::Streaming; }
            requests.finish(token, response, response == RequestResponse::Success ? publication(*selected) : QVariantMap{});
            if (response == RequestResponse::Success) Q_EMIT adaptor.ShortcutsChanged(QDBusObjectPath(request.path), shortcutDescriptions(*selected));
        });
    }
    bool apply(const QString &path, const ShortcutDrafts &selected) {
        const auto component = componentFor(path); const auto previous = bindings.value(path); QStringList changed;
        for (const auto &draft : selected) {
            QindaQt::Services::Shortcuts::Binding binding; binding.component = component; binding.action = draft.id; binding.description = draft.description;
            if (!draft.key.isEmpty()) binding.keys = {draft.key};
            if (!native.registerBinding(binding, true)) {
                for (const auto &id : changed) native.unregisterBinding(component, id);
                bool restored = true;
                for (const auto &old : previous) {
                    binding.action = old.id; binding.description = old.description; binding.keys = old.key.isEmpty() ? QList<QKeySequence>{} : QList<QKeySequence>{old.key};
                    if (!native.registerBinding(binding, true)) restored = false;
                }
                if (!restored) sessions.close(path);
                return false;
            }
            changed.append(draft.id);
        }
        for (const auto &old : previous) if (!changed.contains(old.id)) native.unregisterBinding(component, old.id);
        bindings.insert(path, selected); return true;
    }
    RequestToken begin(const QDBusMessage &call, const QString &handle, const QString &path, const QString &app) {
        const auto slot = std::make_shared<RequestToken>(0);
        const auto token = requests.begin(call, handle, app, [this, slot, path](RequestResponse response) {
            pending.remove(*slot); ui.cancel(*slot);
            if (auto *entry = sessions.entry(path); entry && entry->pending == *slot) entry->pending = 0;
            if (response == RequestResponse::Failed) sessions.close(path);
        }, 100000); *slot = token; return token;
    }
};
GlobalShortcutsAdaptor::GlobalShortcutsAdaptor(QObject &host, RequestRegistry &requests, ShortcutUi &ui, QindaQt::Services::Shortcuts::QtShortcutTransport &native, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), d(std::make_unique<Private>(*this, requests, ui, native, std::move(bus))) { registerShortcutWire(); }
GlobalShortcutsAdaptor::~GlobalShortcutsAdaptor() { d->sessions.clear(); }
quint32 GlobalShortcutsAdaptor::CreateSession(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto token = d->begin(call, handle.path(), session.path(), app); if (!token) return 2;
    if (!options.isEmpty() || !d->ui.admitted() || !d->native.available() || !d->sessions.create(call, handle.path(), session.path(), app)) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    d->requests.finish(token, RequestResponse::Success, {{"session_id", session.path().section('/', -1)}}); return 2;
}
quint32 GlobalShortcutsAdaptor::BindShortcuts(const QDBusObjectPath &handle, const QDBusObjectPath &session, const PortalShortcuts &shortcuts, const QString &parent, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); auto *entry = d->sessions.entry(session.path()); const QString app = entry ? entry->app : QString();
    const auto token = d->begin(call, handle.path(), session.path(), app); if (!token) return 2;
    if (!entry || !d->sessions.authenticated(call, session.path(), app) || !d->sessions.requestMatches(session.path(), handle.path()) || entry->pending
        || !options.isEmpty() || !d->ui.admitted() || !d->native.available() || parent.size() > 2048) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    auto offered = shortcutDrafts(shortcuts); if (!offered) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    const auto previous = d->bindings.value(session.path());
    for (auto &draft : *offered) for (const auto &old : previous) if (draft.id == old.id) draft.key = old.key;
    entry->pending = token; d->pending.insert(token, {session.path(), *offered});
    d->ui.ask(token, shortcutFrame(app, parent, componentFor(session.path()), *offered)); return 2;
}
quint32 GlobalShortcutsAdaptor::ListShortcuts(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); auto *entry = d->sessions.entry(session.path()); const QString app = entry ? entry->app : QString();
    const auto token = d->begin(call, handle.path(), session.path(), app); if (!token) return 2;
    if (!entry || !d->sessions.authenticated(call, session.path(), app) || !d->sessions.requestMatches(session.path(), handle.path()) || !d->native.available()) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    d->requests.finish(token, RequestResponse::Success, publication(d->bindings.value(session.path()))); return 2;
}
}
