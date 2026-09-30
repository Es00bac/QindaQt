// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/idle_inhibition.h>
#include <QHash>
#include <QTimer>
namespace QindaQt::Services::Portal {
using namespace QindaQt::Power;
namespace { constexpr quint32 allIdle = 7; }
class PowerIdleInhibition::Private {
public:
    struct Entry { RequestToken token = 0; quint64 serial = 0; QString owner, app, reason; Handle handle; bool cancelled = false, sent = false; };
    PowerIdleInhibition &q; PowerTransport &transport; std::function<bool()> admission;
    QString owner; quint64 serial = 0, stateQuery = 0;
    QHash<RequestToken, Entry> entries;
    Private(PowerIdleInhibition &object, PowerTransport &port, std::function<bool()> gate)
        : q(object), transport(port), admission(std::move(gate)) {
        QObject::connect(&transport, &PowerTransport::ownerChanged, &q, [this](const QString &value) {
            if (owner == value) return;
            owner = value; stateQuery = 0; entries.clear(); Q_EMIT q.unavailable();
        });
        QObject::connect(&transport, &PowerTransport::idleInhibitorStateReply, &q,
            [this](const QString &actor, quint64 id, bool success, quint32 supported, quint32 active, const QString &) {
                if (actor != owner || id != stateQuery) return;
                stateQuery = 0;
                const bool admitted = success && (supported & ~allIdle) == 0 && (active & ~supported) == 0
                    && (supported & allIdle) == allIdle && admission && admission();
                const auto tokens = entries.keys();
                for (const auto token : tokens) {
                    auto it = entries.find(token);
                    if (it == entries.end() || it->cancelled || it->sent) continue;
                    if (!admitted) { entries.erase(it); Q_EMIT q.acquired(token, false); continue; }
                    it->sent = true; it->serial = ++serial;
                    transport.acquireIdleInhibitor(owner, it->serial,
                        it->app.isEmpty() ? QStringLiteral("Local application") : it->app, it->reason,
                        IdleInhibitorScopes::fromInt(static_cast<int>(allIdle)));
                }
            });
        QObject::connect(&transport, &PowerTransport::idleInhibitorAcquireReply, &q,
            [this](const QString &actor, quint64 id, bool success, const Handle &handle, const QString &) {
                if (actor != owner) return;
                for (auto it = entries.begin(); it != entries.end(); ++it) {
                    if (it->serial != id || !it->sent) continue;
                    const auto token = it.key();
                    const bool valid = success && handle.epoch != 0 && !handle.opaqueId.isEmpty() && handle.opaqueId.size() <= 128;
                    if (it->cancelled || !admission || !admission()) {
                        entries.erase(it);
                        if (valid) transport.releaseIdleInhibitor(actor, ++serial, handle);
                        return;
                    }
                    if (!valid) { entries.erase(it); Q_EMIT q.acquired(token, false); return; }
                    it->handle = handle; Q_EMIT q.acquired(token, true); return;
                }
            });
        QObject::connect(&transport, &PowerTransport::idleInhibitorsChanged, &q,
            [this](const QString &actor, quint32 supported, quint32 active) {
                if (actor == owner && ((supported & allIdle) != allIdle || (supported & ~allIdle) != 0
                    || (active & ~supported) != 0)) q.revoke();
            });
    }
};
PowerIdleInhibition::PowerIdleInhibition(PowerTransport &transport, std::function<bool()> admission, QObject *parent)
    : IdleInhibition(parent), d(std::make_unique<Private>(*this, transport, std::move(admission))) { transport.start(); }
PowerIdleInhibition::~PowerIdleInhibition() { revoke(); d->transport.stop(); }
void PowerIdleInhibition::acquire(RequestToken token, const QString &app, const QString &reason) {
    if (!token || d->entries.contains(token) || d->entries.size() >= 32 || d->owner.isEmpty()
        || !d->admission || !d->admission() || app.size() > 255 || reason.size() > 512) {
        QTimer::singleShot(0, this, [this, token] { Q_EMIT acquired(token, false); }); return;
    }
    d->entries.insert(token, {token, 0, d->owner, app, reason, {}, false, false});
    if (!d->stateQuery) { d->stateQuery = ++d->serial; d->transport.queryIdleInhibitorState(d->owner, d->stateQuery); }
    QTimer::singleShot(4000, this, [this, token] {
        const auto it = d->entries.constFind(token);
        if (it != d->entries.cend() && it->handle.epoch == 0 && !it->cancelled) {
            cancel(token); Q_EMIT acquired(token, false);
        }
    });
}
void PowerIdleInhibition::cancel(RequestToken token) {
    auto it = d->entries.find(token); if (it == d->entries.end()) return;
    if (it->handle.epoch != 0) {
        const auto owner = it->owner; const auto handle = it->handle; d->entries.erase(it);
        if (owner == d->owner) d->transport.releaseIdleInhibitor(owner, ++d->serial, handle);
    } else if (!it->sent) d->entries.erase(it);
    else {
        // Retain a bounded tombstone through Qt's method timeout so a late
        // successful acquisition cannot leak a lease owned by this resident.
        it->cancelled = true;
        QTimer::singleShot(30000, this, [this, token] {
            const auto found = d->entries.find(token);
            if (found != d->entries.end() && found->cancelled) d->entries.erase(found);
        });
    }
}
void PowerIdleInhibition::revoke() {
    const auto tokens = d->entries.keys();
    for (const auto token : tokens) cancel(token);
    Q_EMIT unavailable();
}
} // namespace QindaQt::Services::Portal
