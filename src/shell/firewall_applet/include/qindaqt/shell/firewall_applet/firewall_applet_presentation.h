// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/firewall_protocol/firewall_types.h>
#include <qindaqt/shell/firewall_applet/firewall_applet_types.h>

#include <optional>

namespace QindaQt::Shell::FirewallApplet {

// Client facts the pure projection cannot read from the protocol values
// themselves. The runtime controller fills this from its FirewallClient;
// tests fill it directly.
struct ProjectionContext final {
    // FirewallClient::serviceAvailable(): false when org.qindaqt.Firewall1
    // is not owned on the bus, i.e. the background monitor is not running.
    bool serviceAvailable = false;
    // FirewallClient::state(): the raw LogWatcher `State` string.
    QString state;
    // FirewallClient::headline(): decoded `Headline` property.
    Firewall::Headline headline;
    // FirewallClient::alertCount(): decoded `AlertCount` property.
    quint32 alertCount = 0;
    // FirewallClient::topAlert(): decoded `TopAlert` property, absent when
    // there is no open alert.
    std::optional<Firewall::Alert> topAlert;
};

// Projects (service absent, State, Headline, AlertCount, TopAlert) onto one
// of the six AppletState values and its plain-English presentation
// (docs/wiki/shell/firewall-applet.md). Pure and deterministic: the same
// context always projects to the same model, and no technical term is
// introduced here that QindaFirewall's own alert text did not already use.
[[nodiscard]] FirewallAppletModel projectFirewallApplet(const ProjectionContext &context);

} // namespace QindaQt::Shell::FirewallApplet
