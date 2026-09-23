// SPDX-License-Identifier: GPL-3.0-or-later

#include "voiceappletcomposition.h"

#include "settingsroutelauncher.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/voice_protocol/voice_settings_keys.h"
#include "qindaqt/services/voice_client/qt_voice_transport.h"
#include "qindaqt/services/voice_client/voice_client.h"
#include "qindaqt/services/voice_preferences/voice_input_preference_gate.h"
#include "qindaqt/shell/voice_applet/voice_applet_controller.h"
#include "qindaqt/shell/voice_applet/voice_service_client_adapter.h"

#include <QProcess>
#include <QStringList>

namespace QindaQt::Shell {
namespace {

struct VoiceAppletGrants {
    bool read = false;
    bool control = false;
};

VoiceAppletGrants voiceAppletGrants(const Applets::ManifestCatalog &catalog,
                                    const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("voice"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{manifest->id,
                                              AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return {};
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return {};
    }
    VoiceAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::VoiceRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::VoiceControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

VoiceAppletComposition::VoiceAppletComposition(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy,
    Services::SettingsClient::SettingsClient &settingsClient,
    const QDBusConnection &sessionBus)
{
    m_ownedTransport = std::make_unique<Services::Voice::QtVoiceTransport>(sessionBus);
    compose(catalog, policy, settingsClient, *m_ownedTransport);
}

VoiceAppletComposition::VoiceAppletComposition(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy,
    Services::SettingsClient::SettingsClient &settingsClient,
    Services::Voice::VoiceTransport &transport)
{
    compose(catalog, policy, settingsClient, transport);
}

void VoiceAppletComposition::compose(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy,
    Services::SettingsClient::SettingsClient &settingsClient,
    Services::Voice::VoiceTransport &transport)
{
    const VoiceAppletGrants grants = voiceAppletGrants(catalog, policy);
    m_client = std::make_unique<Services::Voice::VoiceClient>(&transport);
    m_seam = std::make_unique<VoiceApplet::VoiceServiceClientAdapter>(m_client.get());
    m_access = std::make_unique<VoiceApplet::VoiceAppletController>(
        m_seam.get(), grants.read, grants.control);

    // services.voicePanelTranscript decides whether the panel shows the words
    // as they are recognised. It is a privacy control, so it is applied to the
    // projection rather than to a QML binding a second surface could forget.
    m_settingsBinding = std::make_unique<QObject>();
    QObject::connect(&settingsClient,
                     &Services::SettingsClient::SettingsClient::snapshotChanged,
                     m_settingsBinding.get(),
                     [this, &settingsClient] { publishTranscriptPreference(settingsClient); });
    publishTranscriptPreference(settingsClient);

    m_inputGate = std::make_unique<Services::VoicePreferences::VoiceInputPreferenceGate>(
        settingsClient);
    QObject::connect(m_inputGate.get(),
                     &Services::VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     m_settingsBinding.get(), [this, readGranted = grants.read](bool allowed) {
                         if (allowed && readGranted) {
                             m_client->start();
                         } else {
                             m_client->stop();
                         }
                     });
    if (m_inputGate->allowed() && grants.read) {
        m_client->start();
    }
}

void VoiceAppletComposition::publishTranscriptPreference(
    Services::SettingsClient::SettingsClient &settingsClient)
{
    // AGENT-GUARD: shown is the schema default, and the fallback when the
    // settings service has not answered. A settings failure must not silently
    // suppress a feature the user never turned off.
    bool visible = true;
    const auto &snapshot = settingsClient.snapshot();
    if (snapshot.has_value()) {
        const QVariant value = snapshot->values.value(
            QString::fromLatin1(Services::Voice::kVoicePanelTranscriptSettingsKey));
        if (value.metaType().id() == QMetaType::Bool) {
            visible = value.toBool();
        }
    }
    m_access->setTranscriptVisible(visible);
}

VoiceAppletComposition::~VoiceAppletComposition()
{
    m_client->stop();
}

VoiceApplet::VoiceAppletController *VoiceAppletComposition::access() const noexcept
{
    return m_access.get();
}

void VoiceAppletComposition::attachRoutes(SettingsRouteLauncher *launcher)
{
    if (m_access == nullptr || launcher == nullptr) {
        return;
    }
    m_access->setSettingsLaunch(
        [launcher] { return launcher->openRoute(QStringLiteral("voice")); });
    // AGENT-NOTE: the console is an ordinary installed application, not a
    // Settings route, so it is started the way a launcher would start it. It
    // is deliberately detached: the shell must not own its lifetime.
    m_access->setConsoleLaunch([] {
        return QProcess::startDetached(QStringLiteral("qindaqt-voice"), QStringList{});
    });
}

} // namespace QindaQt::Shell
