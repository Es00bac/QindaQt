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

// One bindable local address and the interface it belongs to.
struct LanAddress {
    QHostAddress address;
    QString iface;
};

// True for the ranges a LAN listener may serve by default: RFC1918 (10/8, 172.16/12, 192.168/16),
// 100.64.0.0/10 (Tailscale and other CGNAT) and fc00::/7. Public IPv4, global IPv6 and link-local are never
// bound by default (ADR-0441). A private-range hotspot or guest Wi-Fi still qualifies: use the interface
// allowlist (SetLanInterfaces) to restrict the listener to named interfaces.
[[nodiscard]] bool isPrivateLanAddress(const QHostAddress &address);

// Addresses the LAN listener should bind. The default lists up, non-loopback interface addresses that pass
// isPrivateLanAddress and skips container bridges. Tests inject their own.
using LanAddressProvider = std::function<QList<LanAddress>()>;
[[nodiscard]] QList<LanAddress> defaultLanAddresses();

struct LanOptions {
    QString configDir;               // service.json lives here
    quint16 port = 47160;            // 0 = an ephemeral port per address (tests)
    LanAddressProvider addresses;    // empty = defaultLanAddresses
    ServicePublisher *publisher = nullptr;   // not owned; null = no mDNS record
    QString instanceName;            // advertised instance name (the hostname)
    int pairingWindowMs = 120000;    // ADR-0434 item 5
    int resyncMs = 15000;            // interface-change poll while enabled
};

// Owns the optional LAN side of the service (ADR-0434 item 6, ADR-0441):
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
    // Interface-name allowlist; empty = every private-range interface. Names are validated (at most 16, each
    // 1-15 characters of [A-Za-z0-9_.:@-]); an invalid list is refused and changes nothing.
    bool setInterfaces(const QStringList &names, QString *error = nullptr);
    [[nodiscard]] QStringList interfaces() const { return m_allowlist; }
    // Opens (or restarts) the window. Returns its length in seconds, or 0 when LAN is off.
    quint32 openPairingWindow();
    [[nodiscard]] quint32 pairingWindowRemaining() const;
    // {enabled,state,listeners,interfaces:[bound names],allowlist:[names],failed:[reason],advertised,publisher,
    // pairingWindow,persisted}; carries no addresses.
    [[nodiscard]] QString statusJson() const;

Q_SIGNALS:
    void statusChanged();
    void pairingWindowChanged(bool open, quint32 seconds);

private:
    void sync();
    // `abort` ends LAN pairings still in flight (timeout); a consumed window leaves the one being served alone.
    void closeWindow(bool abort);
    void applyAdvert();
    [[nodiscard]] QStringList boundInterfaces() const;

    Hub &m_hub;
    LanOptions m_options;
    bool m_enabled = false;
    bool m_persistFailed = false;
    struct Bound { quint16 port = 0; QString iface; };
    QHash<QString, Bound> m_bound;        // address text -> bound port and interface
    QStringList m_allowlist;
    QStringList m_advertIfaces;
    QHash<QString, QString> m_failures;   // address text -> reason
    bool m_advertised = false;
    quint16 m_lastAdvertPort = 0;
    QTimer m_resync;
    QTimer m_window;
    bool m_windowOpen = false;
};

} // namespace QindaQt::Link
