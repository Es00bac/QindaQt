// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Obs {
class ObsClient;
class ObsSecretStore;
} // namespace QindaQt::Obs
namespace QindaQt::Services::StreamingPreferences {
class StreamingPreferences;
}

namespace QindaQt::Screenshot {

// Decides whether this process may open its obs-websocket connection.
//
// AGENT-CONTRACT (ADR-0289, mirroring the shell's ObsAppletConnection and
// ADR-0248): the client starts only from a confirmed Settings1 baseline with
// auto-connect on, only when OBS's own config has loaded the selected port,
// and only once a password is already in the keyring. Connecting without
// one would make OBS's refusal look like a wrong password and retry it
// forever. Borrowed, same-thread collaborators; nothing here owns a secret.
class RecordConnection final {
public:
    enum class Gate { Connected, NotLoaded, AutoConnectOff, PortNotActive, NoPassword };

    using ActivePort = std::function<std::optional<int>()>;
    RecordConnection(Obs::ObsClient &client, Obs::ObsSecretStore &secrets,
                     Services::StreamingPreferences::StreamingPreferences &preferences,
                     ActivePort activePort);

    // Re-evaluated on confirmed preference changes and on a slow watch.
    Gate reconcile();
    [[nodiscard]] Gate gate() const noexcept { return m_gate; }

    // The sentence for a closed gate, or empty when connected.
    [[nodiscard]] static QString gateText(Gate gate);

private:
    void stopClient();

    Obs::ObsClient &m_client;
    Obs::ObsSecretStore &m_secrets;
    Services::StreamingPreferences::StreamingPreferences &m_preferences;
    ActivePort m_activePort;
    Gate m_gate = Gate::NotLoaded;
    int m_connectedPort = 0;
};

} // namespace QindaQt::Screenshot
