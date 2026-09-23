// SPDX-License-Identifier: GPL-3.0-or-later
#include "obsappletconnection.h"

#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_secret_store.h>
#include <qindaqt/services/streaming_preferences/streaming_preferences.h>

#include <utility>

namespace QindaQt::Shell {

ObsAppletConnection::ObsAppletConnection(
    Obs::ObsClient &client, Obs::ObsSecretStore &secrets,
    Services::StreamingPreferences::StreamingPreferences &preferences,
    ActivePort activePort)
    : m_client(client), m_secrets(secrets), m_preferences(preferences),
      m_activePort(std::move(activePort)) {}

bool ObsAppletConnection::reconcile() {
    if (!m_preferences.isLoaded() || !m_preferences.autoConnect()) {
        if (m_started) m_client.stop();
        m_started = false;
        m_connectedPort = 0;
        return false;
    }
    const auto configuredPort = m_activePort();
    if (!configuredPort || *configuredPort != m_preferences.webSocketPort()) {
        if (m_started) m_client.stop();
        m_started = false;
        m_connectedPort = 0;
        return false;
    }
    if (m_started && m_connectedPort == *configuredPort) return true;
    QString keyringError;
    const auto password = m_secrets.password(&keyringError);
    // AGENT-GUARD: only an exact confirmed Settings1 baseline and OBS's
    // matching active config can reach the keyring or socket.
    if (!password.has_value()) return false;
    m_client.start(QStringLiteral("ws://127.0.0.1:%1").arg(*configuredPort), *password);
    m_connectedPort = *configuredPort;
    m_started = true;
    return true;
}

} // namespace QindaQt::Shell
