// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/hub.h>

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>

namespace QindaQt::Link {

Hub::Hub(DeviceIdentity identity, TrustStore &store, HubOptions options, QObject *parent)
    : QObject(parent), m_identity(std::move(identity)), m_store(store), m_options(std::move(options))
{
    m_router.add(&m_status);
    m_router.add(&m_ring);
    connect(&m_status, &StatusFeature::statusChanged, this, &Hub::devicesChanged);
}

Hub::~Hub() = default;

bool Hub::listen(const QHostAddress &address, quint16 port, Origin origin, QString *error)
{
    auto listener = std::make_unique<TlsListener>(m_identity, origin, this);
    if (origin == Origin::Lan) {
        if (!m_lanLimiter) {
            m_lanLimiter = std::make_unique<AcceptLimiter>(
                m_options.lanAcceptsPerSource, m_options.lanAcceptsGlobal, m_options.lanAcceptWindowMs,
                [] { static QElapsedTimer t = [] { QElapsedTimer e; e.start(); return e; }(); return t.elapsed(); });
        }
        listener->setAcceptLimiter(m_lanLimiter.get(), m_options.lanHandshakeTimeoutMs);
    }
    if (!listener->listen(address, port)) {
        if (error != nullptr) *error = listener->errorString();
        return false;
    }
    connect(listener.get(), &TlsListener::connectionReady, this, &Hub::adopt);
    m_listeners.push_back(std::move(listener));
    return true;
}

bool Hub::unlisten(const QHostAddress &address, Origin origin)
{
    for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it) {
        if ((*it)->origin() == origin && (*it)->address() == address) {
            (*it)->close();
            (*it)->deleteLater();
            it->release();
            m_listeners.erase(it);
            return true;
        }
    }
    return false;
}

void Hub::dropSessions(Origin origin)
{
    const auto sessions = m_sessions.values();
    for (Session *s : sessions) {
        if (s->origin() == origin && s->state() != SessionState::Closed) {
            s->close(QStringLiteral("shutdown"));
        }
    }
}

qsizetype Hub::listenerCount(Origin origin) const
{
    qsizetype n = 0;
    for (const auto &l : m_listeners) n += l->origin() == origin ? 1 : 0;
    return n;
}

quint16 Hub::portOf(const QHostAddress &address, Origin origin) const
{
    for (const auto &l : m_listeners) {
        if (l->origin() == origin && l->address() == address) return l->port();
    }
    return 0;
}

quint16 Hub::port(qsizetype index) const
{
    return index >= 0 && index < qsizetype(m_listeners.size()) ? m_listeners[size_t(index)]->port() : 0;
}

void Hub::adopt(QSslSocket *socket, Origin origin)
{
    auto *session = new Session(socket, origin, m_options.timings, *this, this);
    const QString id = session->deviceId();
    if (id.isEmpty()) {
        connect(session, &Session::closed, session, &QObject::deleteLater);
        return;
    }
    // Cap unauthenticated connections: refuse right after the handshake. A session whose proven id is
    // pinned is exempt, and LAN sessions have their own small budget, so neither a LAN flood nor throwaway
    // keys can starve a paired phone or the USB pairing path (ADR-0441).
    if (!m_store.contains(id)) {
        int loopbackUnauth = 0, lanUnauth = 0, lanFromSource = 0;
        for (Session *other : std::as_const(m_sessions)) {
            if (other == session || other->state() == SessionState::Ready || other->state() == SessionState::Closed
                || other->deviceId() == id || m_store.contains(other->deviceId())) {
                continue;
            }
            if (other->origin() == Origin::Lan) {
                ++lanUnauth;
                if (other->sourceAddress() == session->sourceAddress()) ++lanFromSource;
            } else {
                ++loopbackUnauth;
            }
        }
        const bool refuse = origin == Origin::Lan
            ? (lanUnauth >= m_options.maxLanUnpaired || lanFromSource >= m_options.maxLanUnpairedPerSource)
            : loopbackUnauth >= m_options.maxUnpaired;
        if (refuse) {
            connect(session, &Session::closed, session, &QObject::deleteLater);
            session->close(QString());
            return;
        }
    }
    Session *existing = m_sessions.value(id);
    if (existing != nullptr && existing->state() != SessionState::Closed) {
        // A LAN connection never displaces a live loopback one.
        if (origin == Origin::Lan && existing->origin() == Origin::Loopback) {
            connect(session, &Session::closed, session, &QObject::deleteLater);
            session->close(QStringLiteral("replaced"));
            return;
        }
        m_sessions.remove(id);
        existing->close(QStringLiteral("replaced"));
    }
    m_sessions.insert(id, session);
    wire(session);
}

void Hub::wire(Session *session)
{
    const QString id = session->deviceId();
    connect(session, &Session::helloReceived, this, &Hub::devicesChanged);
    connect(session, &Session::readyEntered, this, [this, session, id] {
        if (auto d = m_store.find(id)) {
            // Keep the display name current without changing the pin.
            d->name = session->peer().name;
            d->model = session->peer().model;
            m_store.upsert(*d);
        }
        Q_EMIT devicesChanged();
    });
    connect(session, &Session::pairingCodeReady, this, [this, session, id](const QString &code) {
        const bool lan = session->origin() == Origin::Lan;
        Q_EMIT pairingRequested(id, session->peer().name, session->peer().model, code,
                                lan ? QStringLiteral("lan") : QStringLiteral("usb"),
                                lan ? session->sourceAddress().toString() : QString());
    });
    connect(session, &Session::pairingCompleted, this, &Hub::devicesChanged);
    connect(session, &Session::closed, this, [this, session, id] {
        if (m_sessions.value(id) == session) {
            m_sessions.remove(id);
        }
        if (auto d = m_store.find(id)) {
            d->lastSeen = QDateTime::currentMSecsSinceEpoch();
            m_store.upsert(*d);
        }
        session->deleteLater();
        Q_EMIT devicesChanged();
    });
}

bool Hub::beginPairing(Session *session)
{
    if (m_pairingOwner != nullptr && m_pairingOwner != session) {
        // A USB pairing outranks a LAN one: otherwise a LAN host could hold the single slot.
        if (m_pairingOwner->origin() != Origin::Lan || session->origin() != Origin::Loopback) {
            return false;
        }
        Session *old = m_pairingOwner;
        m_pairingOwner = nullptr;
        old->close(QStringLiteral("replaced"));
    }
    m_pairingOwner = session;
    if (session->origin() == Origin::Lan && m_pairingWindow) {
        // One accepted LAN pair.request per window.
        m_pairingWindow = false;
        Q_EMIT lanPairingWindowConsumed();
    }
    return true;
}

void Hub::abortLanPairings()
{
    const auto sessions = m_sessions.values();
    for (Session *s : sessions) {
        if (s->origin() == Origin::Lan && s->pairingPending()) s->close(QStringLiteral("pair-timeout"));
    }
}

void Hub::endPairing(Session *session)
{
    if (m_pairingOwner == session) {
        m_pairingOwner = nullptr;
    }
}

bool Hub::persistPairing(Session &session)
{
    PairedDevice d;
    d.id = session.deviceId();
    d.name = session.peer().name;
    d.model = session.peer().model;
    d.pairedAt = d.lastSeen = QDateTime::currentMSecsSinceEpoch();
    return m_store.upsert(d);
}

void Hub::confirmPairing(const QString &deviceId, bool accept)
{
    if (Session *s = m_sessions.value(deviceId)) {
        s->confirmPairing(accept);
    }
}

void Hub::unpair(const QString &deviceId)
{
    m_store.remove(deviceId);
    if (Session *s = m_sessions.value(deviceId)) {
        s->close(QStringLiteral("unpaired"));
    }
    Q_EMIT devicesChanged();
}

void Hub::ring(const QString &deviceId, std::function<void(bool, const QString &)> done)
{
    Session *s = m_sessions.value(deviceId);
    if (s == nullptr) {
        done(false, QStringLiteral("not-paired"));
        return;
    }
    RingFeature::ring(*s, std::move(done));
}

QJsonObject Hub::snapshotFor(const QString &id, const PairedDevice *paired, Session *session) const
{
    const bool live = session != nullptr && session->state() != SessionState::AwaitHello
        && session->state() != SessionState::Closed;
    const QJsonObject status = m_status.snapshot(id);
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("name"), live ? session->peer().name : paired->name);
    o.insert(QStringLiteral("model"), live ? session->peer().model : paired->model);
    o.insert(QStringLiteral("paired"), paired != nullptr);
    o.insert(QStringLiteral("connected"), live);
    o.insert(QStringLiteral("ready"), live && session->state() == SessionState::Ready);
    o.insert(QStringLiteral("transport"),
             !live ? QStringLiteral("none")
                   : session->origin() == Origin::Loopback ? QStringLiteral("usb") : QStringLiteral("lan"));
    o.insert(QStringLiteral("caps"), live ? QJsonArray::fromStringList(session->peer().caps) : QJsonArray());
    o.insert(QStringLiteral("battery"), status.value(QStringLiteral("battery")));
    o.insert(QStringLiteral("network"), status.value(QStringLiteral("network")));
    o.insert(QStringLiteral("lastSeen"),
             double(live ? QDateTime::currentMSecsSinceEpoch() : (paired != nullptr ? paired->lastSeen : 0)));
    return o;
}

QJsonArray Hub::pendingPairings() const
{
    QJsonArray out;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (it.value()->awaitingLocalDecision()) {
            out.append(QJsonObject{{QStringLiteral("id"), it.key()},
                                   {QStringLiteral("name"), it.value()->peer().name},
                                   {QStringLiteral("model"), it.value()->peer().model},
                                   {QStringLiteral("code"), it.value()->pairingCode()},
                                   {QStringLiteral("origin"), it.value()->origin() == Origin::Lan ? QStringLiteral("lan") : QStringLiteral("usb")},
                                   {QStringLiteral("source"), it.value()->origin() == Origin::Lan ? it.value()->sourceAddress().toString() : QString()}});
        }
    }
    return out;
}

QJsonArray Hub::devices() const
{
    QJsonArray out;
    QSet<QString> seen;
    for (const PairedDevice &d : m_store.devices()) {
        seen.insert(d.id);
        out.append(snapshotFor(d.id, &d, m_sessions.value(d.id)));
    }
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (!seen.contains(it.key()) && it.value()->state() != SessionState::AwaitHello
            && it.value()->state() != SessionState::Closed) {
            out.append(snapshotFor(it.key(), nullptr, it.value()));
        }
    }
    return out;
}

} // namespace QindaQt::Link
