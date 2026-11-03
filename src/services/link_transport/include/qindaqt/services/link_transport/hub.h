// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_transport/identity.h>
#include <qindaqt/services/link_transport/ring_feature.h>
#include <qindaqt/services/link_transport/session.h>
#include <qindaqt/services/link_transport/status_feature.h>
#include <qindaqt/services/link_transport/tls_listener.h>
#include <qindaqt/services/link_transport/trust_store.h>

#include <QtCore/QJsonArray>
#include <QtCore/QObject>

#include <functional>
#include <memory>
#include <vector>

namespace QindaQt::Link {

struct HubOptions {
    PeerHello local;          // name/model/os/app advertised in hello
    SessionTimings timings;   // tests shrink these
    int maxUnpaired = 8;      // UNPAIRED/AwaitHello connections held at once
};

// Owns the listeners and live sessions, applies the one-connection-per-device
// rule, pins keys on completed pairing and publishes device snapshots.
//
// Ownership: the Hub borrows `store` (it must outlive the Hub) and owns its
// sessions, listeners and built-in features. Threading: one thread.
//
// AGENT-CONTRACT: Link1 (link_service) is the only consumer. It relies on
// devicesChanged() for every snapshot-visible change and on pairingRequested()
// carrying the raw six digit code.
class Hub final : public QObject, public SessionPolicy {
    Q_OBJECT
public:
    Hub(DeviceIdentity identity, TrustStore &store, HubOptions options, QObject *parent = nullptr);
    ~Hub() override;

    // Adds a listener; port 0 picks an ephemeral port. On failure `error`
    // (when given) receives the socket's reason. LAN-origin listeners are
    // opened and closed only by the service's LanController (ADR-0441).
    [[nodiscard]] bool listen(const QHostAddress &address, quint16 port, Origin origin = Origin::Loopback,
                              QString *error = nullptr);
    // Closes the listener bound to exactly `address` with `origin`; live sessions stay. False if none.
    bool unlisten(const QHostAddress &address, Origin origin);
    // Closes every live session that arrived through `origin` with bye{reason:"shutdown"}.
    void dropSessions(Origin origin);
    [[nodiscard]] quint16 port(qsizetype listenerIndex = 0) const;
    [[nodiscard]] qsizetype listenerCount(Origin origin) const;
    // Bound port of the listener on exactly `address` with `origin`, 0 if none.
    [[nodiscard]] quint16 portOf(const QHostAddress &address, Origin origin) const;
    void setPairingWindowOpen(bool open) { m_pairingWindow = open; }
    // Registers a feature (non-owning). Must happen before connections arrive.
    void addFeature(FeatureHandler *handler) { m_router.add(handler); }

    [[nodiscard]] QJsonArray devices() const;
    // Pairing requests waiting for the desktop user: [{id,name,model,code}].
    [[nodiscard]] QJsonArray pendingPairings() const;
    void confirmPairing(const QString &deviceId, bool accept);
    void unpair(const QString &deviceId);
    void ring(const QString &deviceId, std::function<void(bool ok, const QString &code)> done);
    [[nodiscard]] Session *session(const QString &deviceId) const { return m_sessions.value(deviceId); }
    [[nodiscard]] const DeviceIdentity &identity() const { return m_identity; }

    // SessionPolicy
    [[nodiscard]] bool isPinned(const QString &deviceId) const override { return m_store.contains(deviceId); }
    [[nodiscard]] bool pairingWindowOpen() const override { return m_pairingWindow; }
    [[nodiscard]] PeerHello localHello() const override { return m_options.local; }
    [[nodiscard]] FeatureRouter *router() override { return &m_router; }
    [[nodiscard]] bool beginPairing(Session *session) override;
    void endPairing(Session *session) override;
    [[nodiscard]] bool persistPairing(Session &session) override;

    // Takes a handshaken socket (also used directly by tests).
    void adopt(QSslSocket *socket, Origin origin);

Q_SIGNALS:
    void devicesChanged();
    void pairingRequested(const QString &deviceId, const QString &name, const QString &model,
                          const QString &code);

private:
    void wire(Session *session);
    QJsonObject snapshotFor(const QString &id, const PairedDevice *paired, Session *session) const;

    DeviceIdentity m_identity;
    TrustStore &m_store;
    HubOptions m_options;
    FeatureRouter m_router;
    StatusFeature m_status;
    RingFeature m_ring;
    std::vector<std::unique_ptr<TlsListener>> m_listeners;
    QHash<QString, Session *> m_sessions;
    bool m_pairingWindow = false;
    Session *m_pairingOwner = nullptr;
};

} // namespace QindaQt::Link
