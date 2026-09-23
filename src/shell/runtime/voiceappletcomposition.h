// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QObject>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Services::VoicePreferences {
class VoiceInputPreferenceGate;
}

namespace QindaQt::Services::Voice {
class VoiceClient;
class VoiceTransport;
}

namespace QindaQt::Shell::VoiceApplet {
class VoiceAppletController;
class VoiceClientInterface;
}

namespace QindaQt::Shell {

class SettingsRouteLauncher;

// Shell-private production boundary for the org.qindaqt.Voice1 client. The
// controller sees only its least-authority seam; the session bus and the
// provider's identity stay owned here.
//
// AGENT-CONTRACT: the client is only started when voice.read is granted
// and services.voiceInput is confirmed On for the current Settings1 owner.
// Without it nothing is asked of the provider at all — no snapshot fetch, no
// signal subscription, and no activation attempt — so a denied policy means the
// voice provider is never even started by the panel.
class VoiceAppletComposition final
{
public:
    VoiceAppletComposition(const Applets::ManifestCatalog &catalog,
                           const AppletHost::CapabilityPolicy &policy,
                           Services::SettingsClient::SettingsClient &settingsClient,
                           const QDBusConnection &sessionBus);
    // Test seam: compose over an injected transport instead of a session bus.
    VoiceAppletComposition(const Applets::ManifestCatalog &catalog,
                           const AppletHost::CapabilityPolicy &policy,
                           Services::SettingsClient::SettingsClient &settingsClient,
                           Services::Voice::VoiceTransport &transport);
    ~VoiceAppletComposition();

    VoiceAppletComposition(const VoiceAppletComposition &) = delete;
    VoiceAppletComposition &operator=(const VoiceAppletComposition &) = delete;

    [[nodiscard]] VoiceApplet::VoiceAppletController *access() const noexcept;

    // Attach the panel's two route affordances. Separate from construction
    // because the Settings route launcher is built after the service applet
    // compositions; until this is called, both routes report themselves
    // unavailable and the popup renders no dead button.
    void attachRoutes(SettingsRouteLauncher *launcher);

private:
    void compose(const Applets::ManifestCatalog &catalog,
                 const AppletHost::CapabilityPolicy &policy,
                 Services::SettingsClient::SettingsClient &settingsClient,
                 Services::Voice::VoiceTransport &transport);
    void publishTranscriptPreference(
        Services::SettingsClient::SettingsClient &settingsClient);

    // AGENT-CONTRACT: reverse member destruction is controller -> seam ->
    // client -> transport. The controller may still be answering a completion
    // while it is torn down, so everything it borrows outlives it.
    std::unique_ptr<Services::Voice::VoiceTransport> m_ownedTransport;
    std::unique_ptr<Services::Voice::VoiceClient> m_client;
    std::unique_ptr<VoiceApplet::VoiceClientInterface> m_seam;
    std::unique_ptr<VoiceApplet::VoiceAppletController> m_access;
    std::unique_ptr<Services::VoicePreferences::VoiceInputPreferenceGate> m_inputGate;
    // The controller borrows nothing from the settings client; this is a plain
    // lifetime anchor for the connection made in compose().
    std::unique_ptr<QObject> m_settingsBinding;
};

} // namespace QindaQt::Shell
