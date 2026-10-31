// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/firewall_client/firewall_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Firewall::Client {

// Production `FirewallTransport`: watches `org.qindaqt.Firewall1`
// (QindaFirewall docs/monitor.md) on the injected connection, normally the
// session bus. Availability tracks name ownership through
// `QDBusServiceWatcher`; `Changed`/`StateChanged`/`AlertRaised`/`Responded`
// are bound once against the well-known name itself, which Qt keeps
// delivering across an owner replacement without manual rebinding.
// Properties are fetched in one `org.freedesktop.DBus.Properties.GetAll`
// call rather than four separate property reads.
//
// AGENT-NOTE: the live service exposes `StateChanged()` (confirmed with
// `busctl --user introspect org.qindaqt.Firewall1 /org/qindaqt/Firewall1`)
// in addition to the documented `Changed()`/`AlertRaised`/`Responded` table
// in QindaFirewall docs/monitor.md. Both are bound to the same invalidation
// path (`changed()`) rather than given separate meaning here, so a State-only
// transition is never missed because only one of the two signals fired.
class QtFirewallTransport final : public FirewallTransport {
    Q_OBJECT
public:
    explicit QtFirewallTransport(QDBusConnection connection, QObject *parent = nullptr);
    ~QtFirewallTransport() override;

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;

    void requestSnapshot(quint64 token) override;
    void requestAlerts(quint64 token) override;
    void requestAlert(quint64 token, const QString &id) override;
    void requestDismiss(const QString &id) override;
    void requestConfirmMine(const QString &id) override;
    void requestOpenWindow(const QString &id) override;

private Q_SLOTS:
    void onServiceOwnerChanged(const QString &name, const QString &oldOwner,
                               const QString &newOwner);
    void onChanged();
    void onStateChanged();
    void onAlertRaised(const QString &alertJson);
    void onResponded(const QString &responseJson);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Firewall::Client
