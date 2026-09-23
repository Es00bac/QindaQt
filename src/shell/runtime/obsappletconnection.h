// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <optional>

namespace QindaQt::Obs { class ObsClient; class ObsSecretStore; }
namespace QindaQt::Services::StreamingPreferences { class StreamingPreferences; }

namespace QindaQt::Shell {

// Shell-private gate over borrowed, same-thread collaborators. It never owns
// a credential or a socket; ObsClient and its secret store retain those.
class ObsAppletConnection final {
public:
    using ActivePort = std::function<std::optional<int>()>;
    ObsAppletConnection(Obs::ObsClient &client, Obs::ObsSecretStore &secrets,
                        Services::StreamingPreferences::StreamingPreferences &preferences,
                        ActivePort activePort);
    // Called on confirmed preference changes and a slow setup watch.
    [[nodiscard]] bool reconcile();

private:
    Obs::ObsClient &m_client;
    Obs::ObsSecretStore &m_secrets;
    Services::StreamingPreferences::StreamingPreferences &m_preferences;
    ActivePort m_activePort;
    bool m_started = false;
    int m_connectedPort = 0;
};

} // namespace QindaQt::Shell
