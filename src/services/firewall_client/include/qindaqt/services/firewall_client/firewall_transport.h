// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Firewall::Client {

// Thread-confined asynchronous transport seam for `org.qindaqt.Firewall1`
// (QindaFirewall docs/monitor.md). Production implementations watch the
// well-known service name on the session bus; tests inject a fake. Unlike
// Network1, this contract has no owner/epoch lineage to fence: the service
// is a single well-known name with no mutation-conflict surface (the applet
// never changes the firewall), so admission here is simply "is the service
// currently owned".
//
// AGENT-CONTRACT: implementations must emit `serviceAvailableChanged(false)`
// before any other signal when the name stops being owned, and must not
// deliver a reply for a request made before the most recent availability
// transition to true.
class FirewallTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~FirewallTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;

    // Requests a full property fetch (State, Headline, AlertCount, TopAlert).
    // The reply is delivered through `snapshotReceived` or `requestFailed`
    // with the same token.
    virtual void requestSnapshot(quint64 token) = 0;

    // `Alerts()` / `Alert(id)`: optional v1 methods the applet does not
    // currently present, kept on the seam because the client contract names
    // them. Replies arrive through `alertsReceived`/`alertReceived` or
    // `requestFailed` with the same token.
    virtual void requestAlerts(quint64 token) = 0;
    virtual void requestAlert(quint64 token, const QString &id) = 0;

    // `Dismiss(id)`, `ConfirmMine(id)`, `OpenWindow(id)`: fire-and-forget by
    // contract (QindaFirewall docs/monitor.md lists no return value for any
    // of the three). A transport failure is a diagnostic, not a UI-visible
    // outcome; the applet never blocks a button on their result.
    virtual void requestDismiss(const QString &id) = 0;
    virtual void requestConfirmMine(const QString &id) = 0;
    virtual void requestOpenWindow(const QString &id) = 0;

Q_SIGNALS:
    // True exactly while org.qindaqt.Firewall1 is owned on the bus.
    void serviceAvailableChanged(bool available);
    // Pass-through of the service's own `Changed()` signal: any property
    // (State, Headline, AlertCount, TopAlert) may now be stale.
    void changed();
    // Pass-through of `AlertRaised(s)`: a live alert appeared or got worse.
    void alertRaised(const QByteArray &alertJson);
    // Pass-through of `Responded(s)`: the responder acted.
    void responded(const QByteArray &responseJson);

    void snapshotReceived(quint64 token, const QString &state,
                          const QByteArray &headlineJson, quint32 alertCount,
                          const QByteArray &topAlertJson);
    void alertsReceived(quint64 token, const QByteArray &alertsJson);
    void alertReceived(quint64 token, const QByteArray &alertJson);
    void requestFailed(quint64 token, const QString &reason);
};

} // namespace QindaQt::Firewall::Client
