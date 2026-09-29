// SPDX-License-Identifier: GPL-3.0-or-later
#include "record_connection.h"

#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_secret_store.h>
#include <qindaqt/services/streaming_preferences/streaming_preferences.h>

#include <QCoreApplication>

#include <utility>

namespace QindaQt::Screenshot {

RecordConnection::RecordConnection(
    Obs::ObsClient &client, Obs::ObsSecretStore &secrets,
    Services::StreamingPreferences::StreamingPreferences &preferences, ActivePort activePort)
    : m_client(client)
    , m_secrets(secrets)
    , m_preferences(preferences)
    , m_activePort(std::move(activePort))
{
}

void RecordConnection::stopClient()
{
    if (m_gate == Gate::Connected)
        m_client.stop();
    m_connectedPort = 0;
}

RecordConnection::Gate RecordConnection::reconcile()
{
    if (!m_preferences.isLoaded()) {
        stopClient();
        return m_gate = Gate::NotLoaded;
    }
    if (!m_preferences.autoConnect()) {
        stopClient();
        return m_gate = Gate::AutoConnectOff;
    }
    const auto configuredPort = m_activePort ? m_activePort() : std::nullopt;
    if (!configuredPort || *configuredPort != m_preferences.webSocketPort()) {
        stopClient();
        return m_gate = Gate::PortNotActive;
    }
    if (m_gate == Gate::Connected && m_connectedPort == *configuredPort)
        return m_gate;
    QString keyringError;
    const auto password = m_secrets.password(&keyringError);
    // AGENT-GUARD: only a confirmed baseline and OBS's matching active config
    // may reach the keyring or the socket.
    if (!password.has_value()) {
        stopClient();
        return m_gate = Gate::NoPassword;
    }
    m_client.start(QStringLiteral("ws://127.0.0.1:%1").arg(*configuredPort), *password);
    m_connectedPort = *configuredPort;
    return m_gate = Gate::Connected;
}

QString RecordConnection::gateText(Gate gate)
{
    const auto tr = [](const char *text) { return QCoreApplication::translate("Screenshot", text); };
    switch (gate) {
    case Gate::Connected:
        return {};
    case Gate::NotLoaded:
        return tr("Reading the Streaming settings…");
    case Gate::AutoConnectOff:
        return tr("QindaQt is set not to connect to OBS. Turn it on in Settings → Streaming.");
    case Gate::PortNotActive:
    case Gate::NoPassword:
        return tr("OBS is not set up for QindaQt yet. Set it up in Settings → Streaming.");
    }
    return {};
}

} // namespace QindaQt::Screenshot
