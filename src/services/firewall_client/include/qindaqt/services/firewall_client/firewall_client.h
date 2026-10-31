// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/firewall_protocol/firewall_types.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <functional>
#include <optional>

namespace QindaQt::Firewall::Client {

class FirewallTransport;

// Owns one `org.qindaqt.Firewall1` binding (QindaFirewall docs/monitor.md).
// All calls are asynchronous; the client borrows its transport, which must
// outlive it. Thread-confined to the Qt main loop.
//
// AGENT-CONTRACT: `serviceAvailable() == false` is a first-class state, not
// an error: QindaFirewall's background monitor is optional, off by default
// until the user enables it, and the applet must present that plainly
// rather than guessing a security posture from its absence. `state()` is
// the raw `State` property (`off`/`starting`/`live`/`denied`/`retrying`, or
// empty before the first successful fetch); `headline()`, `alertCount()`,
// and `topAlert()` mirror `Headline`, `AlertCount`, and `TopAlert` exactly as
// decoded. Every getter returns the last-known value even while a refresh is
// in flight; a consumer that needs to know whether a value is current reads
// `serviceAvailable()` first.
class FirewallClient final : public QObject {
    Q_OBJECT
public:
    explicit FirewallClient(FirewallTransport &transport, QObject *parent = nullptr);
    ~FirewallClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    // Asks for a fresh property fetch. Coalesced: a refresh requested while
    // one is already in flight runs again once that one completes rather
    // than overlapping it.
    void refresh();

    [[nodiscard]] bool serviceAvailable() const noexcept { return m_serviceAvailable; }
    [[nodiscard]] const QString &state() const noexcept { return m_state; }
    [[nodiscard]] const Headline &headline() const noexcept { return m_headline; }
    [[nodiscard]] quint32 alertCount() const noexcept { return m_alertCount; }
    [[nodiscard]] const std::optional<Alert> &topAlert() const noexcept { return m_topAlert; }

    // `Dismiss(id)` / `ConfirmMine(id)` / `OpenWindow(id)`: fire-and-forget,
    // exactly like the documented D-Bus methods (none returns a value). The
    // applet never gates a button on their outcome; a transport failure is
    // logged by the production transport, not surfaced here.
    void dismiss(const QString &id);
    void confirmMine(const QString &id);
    void openWindow(const QString &id);

    // `Alerts()` / `Alert(id)`: optional v1 methods kept on the public
    // client surface because the documented contract names them, though the
    // panel applet's popup presents only the top alert today. A callback is
    // dropped silently if the client stops before the reply arrives.
    using AlertsCallback = std::function<void(QList<Alert>)>;
    using AlertCallback = std::function<void(std::optional<Alert>)>;
    void requestAlerts(AlertsCallback callback);
    void requestAlert(const QString &id, AlertCallback callback);

Q_SIGNALS:
    // Coarse: some property may have changed (mirrors the service's own
    // `Changed()` plus every successful refresh this client itself drove).
    void changed();
    // Specifically: `serviceAvailable()` or `state()` changed value.
    void stateChanged();
    // Pass-through of `AlertRaised(s)`: a live alert appeared or got worse.
    // The payload is the raw JSON text; `changed()` follows once the
    // resulting refresh lands.
    void alertRaised(const QString &alertJson);
    // Pass-through of `Responded(s)`: the responder acted.
    void responded(const QString &responseJson);

private:
    void handleServiceAvailableChanged(bool available);
    void handleTransportChanged();
    void handleAlertRaised(const QByteArray &alertJson);
    void handleResponded(const QByteArray &responseJson);
    void handleSnapshotReceived(quint64 token, const QString &state,
                                const QByteArray &headlineJson, quint32 alertCount,
                                const QByteArray &topAlertJson);
    void handleAlertsReceived(quint64 token, const QByteArray &json);
    void handleAlertReceived(quint64 token, const QByteArray &json);
    void handleRequestFailed(quint64 token, const QString &reason);
    void requestSnapshotNow();
    void publishStateIfChanged(bool previousAvailable, const QString &previousState);
    [[nodiscard]] quint64 nextToken();

    FirewallTransport &m_transport;
    bool m_started = false;
    bool m_serviceAvailable = false;
    QString m_state;
    Headline m_headline;
    quint32 m_alertCount = 0;
    std::optional<Alert> m_topAlert;
    quint64 m_nextToken = 1;
    std::optional<quint64> m_snapshotToken;
    bool m_snapshotDirty = false;
    QHash<quint64, AlertsCallback> m_alertsCallbacks;
    QHash<quint64, AlertCallback> m_alertCallbacks;
};

} // namespace QindaQt::Firewall::Client
