// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/notification_feature.h>
#include <qindaqt/services/link_transport/session.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>

namespace QindaQt::Link {
namespace {

Q_LOGGING_CATEGORY(lcNotify, "qindaqt.link.notify")

// Peer text goes to a server that advertises plain-text bodies only (no
// body-markup), so nothing is escaped; control and bidi-format characters
// become spaces (newlines survive in the body).
QString clean(const QString &text, bool keepNewlines)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (keepNewlines && c == QLatin1Char('\n')) {
            out += c;
        } else if (c.category() == QChar::Other_Control || c.category() == QChar::Other_Format
                   || c == QChar(0x2028) || c == QChar(0x2029)) {
            out += QLatin1Char(' ');
        } else {
            out += c;
        }
    }
    return out.trimmed();
}

constexpr qsizetype kMaxTrackedPerDevice = 100;  // protocol page, "Rate and size"

QString waitKey(const QString &device, const QString &hash) { return device + QLatin1Char('/') + hash; }

} // namespace

NotificationFeature::NotificationFeature(NotificationFeatureDeps deps, QObject *parent)
    : QObject(parent), m_deps(std::move(deps))
{
    connect(m_deps.presenter, &NotificationPresenter::userDismissed, this, &NotificationFeature::onUserDismissed);
    connect(m_deps.presenter, &NotificationPresenter::actionInvoked, this, &NotificationFeature::onActionInvoked);
}

NotificationFeature::Entry *NotificationFeature::find(const QString &device, const QString &key)
{
    const auto d = m_active.find(device);
    if (d == m_active.end()) return nullptr;
    const auto e = d->find(key);
    return e == d->end() ? nullptr : &e.value();
}

void NotificationFeature::handle(Session &session, const Envelope &e)
{
    if (lcNotify().isDebugEnabled()) {
        qCDebug(lcNotify) << "recv" << e.t << "bytes" << QJsonDocument(e.b).toJson(QJsonDocument::Compact).size();
    }
    const QString device = session.deviceId();
    auto bad = [&] {
        if (e.id != 0) session.send(makeError(QStringLiteral("bad-request"), QStringLiteral("invalid notify frame"), e.id));
    };
    if (e.t == QLatin1String("notify.posted")) {
        PhoneNotification n;
        if (!parseNotification(e.b, &n)) return bad();
        onPosted(session, n, false);
    } else if (e.t == QLatin1String("notify.snapshot")) {
        QList<PhoneNotification> items;
        if (!parseSnapshot(e.b, &items)) return bad();
        onSnapshot(session, items);
    } else if (e.t == QLatin1String("notify.removed")) {
        const QJsonValue key = e.b.value(QStringLiteral("key"));
        if (!key.isString() || key.toString().isEmpty() || key.toString().size() > NotifyLimits::kKey) return bad();
        onRemoved(device, key.toString());
    } else if (e.t == QLatin1String("notify.icon")) {
        onIcon(device, e.b);
    } else if (e.id != 0) {
        // notify.dismiss / notify.action flow desktop to phone only.
        session.send(makeError(QStringLiteral("unsupported"), QStringLiteral("not accepted from a phone"), e.id));
    }
}

void NotificationFeature::sessionClosed(const QString &deviceId)
{
    // Entries stay: a reconnect sends a snapshot that reconciles them without
    // re-popping unchanged ones. Anything still waiting for an icon shows now.
    const QString prefix = deviceId + QLatin1Char('/');
    const QStringList keys = m_waiting.keys();
    for (const QString &k : keys) {
        if (k.startsWith(prefix)) flushIcon(deviceId, k.mid(prefix.size()));
    }
}

void NotificationFeature::onPosted(Session &session, const PhoneNotification &n, bool quiet)
{
    const QString device = session.deviceId();
    m_deps.apps->noteApp(n.pkg, n.app);
    Entry *existing = find(device, n.key);
    if (m_deps.apps->isMuted(n.pkg)) {
        if (existing != nullptr) erase(device, n.key, true);
        return;
    }
    if (existing != nullptr && existing->n == n) return;
    if (existing == nullptr) {
        m_active[device].insert(n.key, Entry{});
        m_order[device].append(n.key);
        existing = find(device, n.key);
    }
    existing->n = n;
    existing->gen = ++m_gen;
    existing->quiet = quiet;
    const quint64 gen = existing->gen;
    while (m_order[device].size() > kMaxTrackedPerDevice) {
        erase(device, m_order[device].constFirst(), true);  // invalidates `existing`
    }
    if (const Entry *changed = find(device, n.key)) Q_EMIT entryChanged(entryJson(device, *changed));
    if (n.icon.isEmpty() || m_deps.icons->has(n.icon)) {
        show(device, n.key);
        return;
    }
    const QString wk = waitKey(device, n.icon);
    const bool firstWaiter = !m_waiting.contains(wk);
    m_waiting[wk].append(Waiter{device, n.key, gen});
    if (!firstWaiter) return;
    session.send(makeIconRequest(n.icon));
    const QString icon = n.icon;
    QTimer::singleShot(m_deps.iconTimeoutMs, this, [this, device, icon] { flushIcon(device, icon); });
}

void NotificationFeature::onSnapshot(Session &session, const QList<PhoneNotification> &items)
{
    const QString device = session.deviceId();
    QSet<QString> keep;
    for (const PhoneNotification &n : items) keep.insert(n.key);
    const QStringList existing = m_order.value(device);
    for (const QString &key : existing) {
        if (!keep.contains(key)) erase(device, key, true);
    }
    for (const PhoneNotification &n : items) onPosted(session, n, true);
}

void NotificationFeature::onRemoved(const QString &device, const QString &key)
{
    if (find(device, key) != nullptr) erase(device, key, true);
}

void NotificationFeature::onIcon(const QString &device, const QJsonObject &b)
{
    const QString hash = b.value(QStringLiteral("hash")).toString();
    if (!isIconHash(hash) || !m_waiting.contains(waitKey(device, hash))) return;  // unsolicited
    const QJsonValue png = b.value(QStringLiteral("png"));
    if (png.isString() && !png.toString().isEmpty() && png.toString().size() <= NotifyLimits::kIconBase64) {
        (void)m_deps.icons->store(hash, QByteArray::fromBase64(png.toString().toLatin1()));
    }
    flushIcon(device, hash);  // shows with the icon if it verified, without otherwise
}

void NotificationFeature::flushIcon(const QString &device, const QString &hash)
{
    const QList<Waiter> waiters = m_waiting.take(waitKey(device, hash));
    for (const Waiter &w : waiters) {
        const Entry *e = find(w.device, w.key);
        if (e != nullptr && e->gen == w.gen) show(w.device, w.key);
    }
}

void NotificationFeature::show(const QString &device, const QString &key)
{
    Entry *e = find(device, key);
    if (e == nullptr) return;
    const bool canReply = m_deps.reply != nullptr && m_deps.reply->available();
    PresentedNotification p;
    p.appName = clean(e->n.app, false) + QStringLiteral(" (phone)");
    p.summary = clean(e->n.title, false);
    if (p.summary.isEmpty()) p.summary = clean(e->n.app, false);
    p.body = clean(e->n.text, true);
    p.iconPath = e->n.icon.isEmpty() || !m_deps.icons->has(e->n.icon) ? QString() : m_deps.icons->path(e->n.icon);
    p.quiet = e->quiet;
    for (const PhoneAction &a : std::as_const(e->n.actions)) {
        if (a.reply && !canReply) continue;
        p.actions.append({(a.reply ? QStringLiteral("r:") : QStringLiteral("a:")) + a.id,
                          a.reply ? QStringLiteral("Reply…") : clean(a.title, false)});
    }
    e->actionTaken = false;
    const quint32 old = e->handle;
    e->handle = m_deps.presenter->present(p, old);
    if (old != 0 && old != e->handle) m_byHandle.remove(old);
    m_byHandle.insert(e->handle, {device, key});
}

void NotificationFeature::erase(const QString &device, const QString &key, bool closeShown)
{
    Entry *e = find(device, key);
    if (e == nullptr) return;
    if (e->handle != 0) {
        m_byHandle.remove(e->handle);
        if (closeShown) m_deps.presenter->close(e->handle);
    }
    m_active[device].remove(key);
    m_order[device].removeAll(key);
    Q_EMIT entryChanged(QJsonObject{{QStringLiteral("device"), device}, {QStringLiteral("key"), key},
                                    {QStringLiteral("removed"), true}});
}

void NotificationFeature::onUserDismissed(quint32 handle)
{
    const auto it = m_byHandle.constFind(handle);
    if (it == m_byHandle.constEnd()) return;
    const QString device = it->first, key = it->second;
    Entry *e = find(device, key);
    if (e == nullptr) return;
    if (e->actionTaken) {
        // Servers close a notification after an action button; that is not a dismissal
        // (a reply button must not cancel the conversation on the phone).
        e->actionTaken = false;
        m_byHandle.remove(handle);
        e->handle = 0;
        return;
    }
    if (Session *s = m_deps.sessionFor ? m_deps.sessionFor(device) : nullptr) {
        s->send(makeDismiss(key));
    }
    erase(device, key, false);
}

void NotificationFeature::onActionInvoked(quint32 handle, const QString &actionKey)
{
    const auto it = m_byHandle.constFind(handle);
    if (it == m_byHandle.constEnd() || actionKey.size() < 3) return;
    const QString device = it->first, key = it->second;
    Entry *e = find(device, key);
    if (e == nullptr) return;
    e->actionTaken = true;
    const quint64 token = e->actionToken = ++m_gen;
    // The close that follows an action button arrives within moments; a later real dismissal must still count.
    QTimer::singleShot(m_deps.actionGraceMs, this, [this, device, key, token] {
        if (Entry *x = find(device, key); x != nullptr && x->actionToken == token) x->actionTaken = false;
    });
    const QString id = actionKey.mid(2);
    if (actionKey.startsWith(QLatin1String("r:"))) {
        if (m_deps.reply != nullptr) m_deps.reply->launch(device, key, id);
        return;
    }
    if (!actionKey.startsWith(QLatin1String("a:"))) return;
    reply(device, key, id, QString(), [](const QString &error) {
        if (!error.isEmpty()) qCWarning(lcNotify) << "notify.action failed:" << error;
    });
}

void NotificationFeature::reply(const QString &deviceId, const QString &key, const QString &actionId,
                                const QString &text, std::function<void(const QString &)> done)
{
    const Entry *e = find(deviceId, key);
    const PhoneAction *action = nullptr;
    if (e != nullptr) {
        for (const PhoneAction &a : e->n.actions) {
            if (a.id == actionId) action = &a;
        }
    }
    if (action == nullptr) return done(QStringLiteral("not-found"));
    const bool hasText = action->reply;
    if (text.size() > NotifyLimits::kReplyText || (!text.isEmpty() && !action->reply)) return done(QStringLiteral("bad-request"));
    Session *s = m_deps.sessionFor ? m_deps.sessionFor(deviceId) : nullptr;
    if (s == nullptr || !s->peerHasCap(QStringLiteral("notify"))) return done(QStringLiteral("not-connected"));
    s->request(makeAction(key, actionId, action->title, text, hasText), [done = std::move(done)](const Envelope &r) {
        if (r.t == QLatin1String("ok")) return done(QString());
        const QString code = r.b.value(QStringLiteral("code")).toString(QStringLiteral("internal"));
        // unsupported + "unverified": the action launches an activity and Android may have suppressed it.
        const bool unverified = code == QLatin1String("unsupported")
            && r.b.value(QStringLiteral("message")).toString() == QLatin1String("unverified");
        done(unverified ? QStringLiteral("unverified") : code);
    });
}

void NotificationFeature::setAppMuted(const QString &pkg, bool muted)
{
    if (!m_deps.apps->setMuted(pkg, muted) || !muted) return;
    for (const QString &device : m_active.keys()) {
        for (const QString &key : m_order.value(device)) {
            const Entry *e = find(device, key);
            if (e != nullptr && e->n.pkg == pkg) erase(device, key, true);
        }
    }
}

QJsonObject NotificationFeature::entryJson(const QString &device, const Entry &e) const
{
    QJsonObject o = e.n.toJson();
    o.insert(QStringLiteral("device"), device);
    o.insert(QStringLiteral("hasReply"), e.n.hasReply());
    return o;
}

QJsonArray NotificationFeature::current() const
{
    QJsonArray out;
    for (auto d = m_active.begin(); d != m_active.end(); ++d) {
        for (const QString &key : m_order.value(d.key())) {
            const auto e = d->constFind(key);
            if (e == d->constEnd()) continue;
            out.append(entryJson(d.key(), *e));
        }
    }
    return out;
}

} // namespace QindaQt::Link
