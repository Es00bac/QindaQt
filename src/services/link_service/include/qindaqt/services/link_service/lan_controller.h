// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_service/service_publisher.h>
#include <qindaqt/services/link_transport/hub.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>

#include <functional>

namespace QindaQt::Link {

// Addresses the LAN listener should bind. The default lists up, non-loopback
// interface addresses (IPv4, plus IPv6 global/ULA; never link-local, which
// needs a scope the phone cannot supply) and skips container bridges.
using LanAddressProvider = std::function<QList<QHostAddress>()>;
[[nodiscard]] QList<QHostAddress> defaultLanAddresses();

struct LanOptions {
    QString configDir;               // service.json lives here
    quint16 port = 47160;            // 0 = an ephemeral port per address (tests)
    LanAddressProvider addresses;    // empty = defaultLanAddresses
    ServicePublisher *publisher = nullptr;   // not owned; null = no mDNS record
    QString instanceName;            // advertised instance name (the hostname)
    int pairingWindowMs = 120000;    // ADR-0434 item 5
    int resyncMs = 15000;            // interface-change poll while enabled
};

// Owns the optional LAN side of the service (ADR-0434 item 6, ADR-0440):
// the persisted setting, one TLS listener per local address, the
// `_qindalink._tcp` advert and the pairing window.
//
// AGENT-GUARD: the loopback listener is the Hub's and is never touched here.
// Listeners are bound per address, never to 0.0.0.0/::, because a wildcard bind
// would also catch 127.0.0.1 traffic and fail against the loopback listener.
// Ownership: borrows hub (must outlive); publisher is injected. Threading: one
// thread. Error behavior: bind and publish failures are reported in status(),
// never thrown; the next resync retries them.
class LanController final : public QObject {
    Q_OBJECT
public:
    LanController(Hub &hub, LanOptions options, QObject *parent = nullptr);
    ~LanController() override;

    [[nodiscard]] bool enabled() const { return m_enabled; }
    // Persists and applies. False when the setting could not be persisted (it is still applied until restart).
    bool setEnabled(bool enabled);
    // Opens (or restarts) the window. Returns its length in seconds, or 0 when LAN is off.
    quint32 openPairingWindow();
    [[nodiscard]] quint32 pairingWindowRemaining() const;
    // {enabled,state,listeners,failed:[reason],advertised,publisher,pairingWindow}; carries no addresses.
    [[nodiscard]] QString statusJson() const;

Q_SIGNALS:
    void statusChanged();
    void pairingWindowChanged(bool open, quint32 seconds);

private:
    void sync();
    void closeWindow();
    void applyAdvert();

    Hub &m_hub;
    LanOptions m_options;
    bool m_enabled = false;
    bool m_persistFailed = false;
    QHash<QString, quint16> m_bound;      // address text -> bound port
    QHash<QString, QString> m_failures;   // address text -> reason
    bool m_advertised = false;
    quint16 m_lastAdvertPort = 0;
    QTimer m_resync;
    QTimer m_window;
    bool m_windowOpen = false;
};

} // namespace QindaQt::Link
