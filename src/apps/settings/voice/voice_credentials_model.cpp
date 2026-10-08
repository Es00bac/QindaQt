// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_voice/voice_credentials_model.h>
namespace QindaQt::Apps::SettingsVoice {
VoiceCredentialsModel::VoiceCredentialsModel(Services::VoiceConfiguration::Client &client,
                                             QObject *parent)
    : QObject(parent), m_client(client) {
    connect(&client, &Services::VoiceConfiguration::Client::changed,
            this, &VoiceCredentialsModel::changed);
    connect(&client, &Services::VoiceConfiguration::Client::entryClearRequested,
            this, &VoiceCredentialsModel::clearEntry);
}
bool VoiceCredentialsModel::available() const {
    return m_client.ready() && m_client.snapshot().canConfigure && !busy();
}
bool VoiceCredentialsModel::busy() const { return m_client.busy(); }
bool VoiceCredentialsModel::save(const QString &key) { return m_client.save(key); }
bool VoiceCredentialsModel::reload() { return m_client.reload(); }
QString VoiceCredentialsModel::statusText() const {
    const QString status = m_client.status();
    if (status == QStringLiteral("unsupported"))
        return tr("This provider does not support secure credential settings. Update the voice provider to use these controls.");
    if (status == QStringLiteral("unavailable"))
        return tr("Connect the voice provider to configure credentials.");
    if (busy() || status == QStringLiteral("busy"))
        return tr("Waiting for secure storage and credential reload. Dictation is paused until this operation settles.");
    if (status == QStringLiteral("uncertain") || status == QStringLiteral("timeout"))
        return tr("The result is unconfirmed. Wait for the provider to settle, then deliberately reload; the request will not be repeated automatically.");
    if (status == QStringLiteral("saved_reload_unconfirmed"))
        return tr("The key was saved, but reload is unconfirmed. Unlock your keyring and choose Reload saved key.");
    if (status == QStringLiteral("keyring_unavailable") || status == QStringLiteral("credential_unavailable"))
        return tr("Secure storage was unavailable, locked, cancelled, or has no usable saved key. Unlock your keyring and choose Reload saved key.");
    if (status == QStringLiteral("reload_failed"))
        return tr("Credential reload failed. The previous speech engine was preserved. Try Reload saved key.");
    if (status == QStringLiteral("invalid_key"))
        return tr("Enter a nonempty API key without spaces, up to 512 ASCII characters.");
    if (m_client.ready() && m_client.snapshot().environmentOverride)
        return tr("An explicit environment key overrides secure storage. Saving a key here does not change that override.");
    if (status == QStringLiteral("ok") || status == QStringLiteral("ready"))
        return tr("Key loaded. Authentication is checked when you transcribe.");
    return tr("Reload the saved key if the keyring became available after the provider started.");
}
QString VoiceCredentialsModel::effectiveText() const {
    if (!m_client.ready()) return tr("Effective provider is unconfirmed.");
    const auto &state = m_client.snapshot();
    const auto label = [this](const QString &identifier) {
        if (identifier == QStringLiteral("elevenlabs")) return tr("ElevenLabs");
        if (identifier == QStringLiteral("whisper_local")) return tr("Whisper (local)");
        if (identifier == QStringLiteral("gemini")) return tr("Gemini");
        if (identifier == QStringLiteral("unresolved")) return tr("Unconfirmed");
        return identifier;
    };
    return state.fallbackActive
        ? tr("Configured: %1. Effective: %2 (fallback).").arg(label(state.configuredProvider), label(state.effectiveProvider))
        : tr("Configured: %1. Effective: %2.").arg(label(state.configuredProvider), label(state.effectiveProvider));
}
}
