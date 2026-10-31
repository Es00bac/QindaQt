// SPDX-License-Identifier: GPL-3.0-or-later
#include "firewall_applet_controller.h"

#include <qindaqt/shell/firewall_applet/firewall_applet_presentation.h>

#include <utility>

namespace QindaQt::Shell::FirewallApplet
{

FirewallAppletController::FirewallAppletController(
    Firewall::Client::FirewallClient *client, const bool securityReadGranted,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_granted(securityReadGranted)
{
    if (m_granted) {
        Q_ASSERT(m_client != nullptr);
        Q_ASSERT(m_client->thread() == thread());
        using Firewall::Client::FirewallClient;
        connect(m_client, &FirewallClient::changed, this,
                &FirewallAppletController::reproject);
        connect(m_client, &FirewallClient::stateChanged, this,
                &FirewallAppletController::reproject);
        connect(m_client, &FirewallClient::alertRaised, this,
                &FirewallAppletController::reproject);
        connect(m_client, &FirewallClient::responded, this,
                &FirewallAppletController::reproject);
    }
    reproject();
}

FirewallAppletController::~FirewallAppletController() = default;

void FirewallAppletController::reproject()
{
    ProjectionContext context;
    if (m_granted && m_client != nullptr) {
        context.serviceAvailable = m_client->serviceAvailable();
        context.state = m_client->state();
        context.headline = m_client->headline();
        context.alertCount = m_client->alertCount();
        context.topAlert = m_client->topAlert();
    }
    // AGENT-NOTE: a denied grant leaves `context` at its defaults
    // (serviceAvailable false), which projects to Unavailable without ever
    // reading `m_client` — matching the Agent Usage applet's "a denied
    // grant constructs no collector and exposes no rows" precedent.
    FirewallAppletModel next = projectFirewallApplet(context);
    if (next == m_model) {
        return;
    }
    m_model = std::move(next);
    Q_EMIT modelChanged();
}

void FirewallAppletController::openWindow()
{
    if (!m_granted || m_client == nullptr) {
        return;
    }
    m_client->openWindow(m_model.hasAlert ? m_model.alert.id : QString());
}

void FirewallAppletController::dismissTopAlert()
{
    if (!m_granted || m_client == nullptr || !m_model.hasAlert) {
        return;
    }
    m_client->dismiss(m_model.alert.id);
}

void FirewallAppletController::confirmTopAlertMine()
{
    if (!m_granted || m_client == nullptr || !m_model.hasAlert
        || !m_model.alert.canConfirm) {
        return;
    }
    m_client->confirmMine(m_model.alert.id);
}

} // namespace QindaQt::Shell::FirewallApplet
